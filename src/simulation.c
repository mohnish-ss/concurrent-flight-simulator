#include "flight_sim.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

enum
{
    ALERT_PRIORITY_NOTICE = 1,
    ALERT_PRIORITY_ADVISORY = 2,
    ALERT_PRIORITY_CRITICAL = 3
};

static void sleep_for_ms(unsigned milliseconds)
{
    const struct timespec duration = {
        .tv_sec = (time_t)(milliseconds / 1000),
        .tv_nsec = (long)(milliseconds % 1000) * 1000000L,
    };
    (void)nanosleep(&duration, NULL);
}

static bool tank_init(SharedTank *tank, const char *label, int level, int limit)
{
    if (!sim_lock_init(&tank->guard))
    {
        return false;
    }
    (void)snprintf(tank->label, sizeof(tank->label), "%s", label);
    tank->level = level;
    tank->limit = limit;
    return true;
}

static int tank_snapshot(SharedTank *tank)
{
    sim_lock_acquire(&tank->guard);
    const int level = tank->level;
    sim_lock_release(&tank->guard);
    return level;
}

static bool tank_take(SharedTank *tank, int amount, int *remaining)
{
    sim_lock_acquire(&tank->guard);
    const bool available = tank->level >= amount;
    if (available)
    {
        tank->level -= amount;
    }
    *remaining = tank->level;
    sim_lock_release(&tank->guard);
    return available;
}

static bool tank_add(SharedTank *tank, int amount, int *updated)
{
    sim_lock_acquire(&tank->guard);
    const int available_space = tank->limit - tank->level;
    const int accepted = amount < available_space ? amount : available_space;
    tank->level += accepted;
    *updated = tank->level;
    const bool full = tank->level == tank->limit;
    sim_lock_release(&tank->guard);
    return full;
}

static void unit_init(
    FlightUnit *unit,
    const char *label,
    SharedTank *input,
    int input_rate,
    SharedTank *output,
    int output_rate,
    unsigned delay_ms,
    FlightSimulation *simulation)
{
    (void)snprintf(unit->label, sizeof(unit->label), "%s", label);
    unit->input = input;
    unit->output = output;
    unit->input_rate = input_rate;
    unit->output_rate = output_rate;
    unit->delay_ms = delay_ms;
    unit->alerts = &simulation->alerts;
    unit->running = &simulation->running;
    atomic_init(&unit->mode, FLIGHT_MODE_NOMINAL);
}

static void publish_alert(
    FlightUnit *unit,
    AlertKind kind,
    int priority,
    SharedTank *tank,
    int observed_level)
{
    const FlightAlert alert = {
        .kind = kind,
        .priority = priority,
        .sequence = 0,
        .source = unit,
        .tank = tank,
        .observed_level = observed_level,
    };
    (void)alert_buffer_push(unit->alerts, alert);
}

static unsigned adjusted_delay(const FlightUnit *unit)
{
    const int mode = atomic_load_explicit(&unit->mode, memory_order_relaxed);
    if (mode == FLIGHT_MODE_ECO)
    {
        return unit->delay_ms * 2;
    }
    if (mode == FLIGHT_MODE_BOOST)
    {
        const unsigned faster = unit->delay_ms / 2;
        return faster == 0 ? 1 : faster;
    }
    return unit->delay_ms;
}

static void *unit_worker(void *context)
{
    FlightUnit *unit = context;

    while (atomic_load_explicit(unit->running, memory_order_acquire))
    {
        if (atomic_load_explicit(&unit->mode, memory_order_relaxed) ==
            FLIGHT_MODE_STOPPED)
        {
            break;
        }

        int remaining = 0;
        if (!tank_take(unit->input, unit->input_rate, &remaining))
        {
            publish_alert(
                unit,
                ALERT_INPUT_EMPTY,
                ALERT_PRIORITY_CRITICAL,
                unit->input,
                remaining);
            sleep_for_ms(2);
            continue;
        }

        if (remaining * 100 <= unit->input->limit * 30)
        {
            publish_alert(
                unit,
                ALERT_INPUT_LOW,
                ALERT_PRIORITY_ADVISORY,
                unit->input,
                remaining);
        }

        sleep_for_ms(adjusted_delay(unit));

        if (unit->output != NULL)
        {
            int updated = 0;
            if (tank_add(unit->output, unit->output_rate, &updated))
            {
                publish_alert(
                    unit,
                    ALERT_OUTPUT_FULL,
                    ALERT_PRIORITY_NOTICE,
                    unit->output,
                    updated);
            }
        }
    }

    return NULL;
}

static void stop_all_units(FlightSimulation *simulation)
{
    atomic_store_explicit(&simulation->running, false, memory_order_release);
    for (size_t index = 0; index < FLIGHT_UNIT_COUNT; ++index)
    {
        atomic_store_explicit(
            &simulation->units[index].mode,
            FLIGHT_MODE_STOPPED,
            memory_order_relaxed);
    }
}

static void adjust_producers(
    FlightSimulation *simulation,
    SharedTank *tank,
    FlightMode mode)
{
    for (size_t index = 0; index < FLIGHT_UNIT_COUNT; ++index)
    {
        FlightUnit *unit = &simulation->units[index];
        if (unit->output == tank)
        {
            atomic_store_explicit(&unit->mode, mode, memory_order_relaxed);
        }
    }
}

static void handle_alert(FlightSimulation *simulation, const FlightAlert *alert)
{
    if (alert->kind == ALERT_INPUT_EMPTY &&
        strcmp(alert->tank->label, "Cabin air") == 0)
    {
        puts("Cabin air depleted; stopping all systems.");
        stop_all_units(simulation);
        return;
    }

    if (alert->kind == ALERT_OUTPUT_FULL &&
        strcmp(alert->tank->label, "Travel") == 0)
    {
        puts("Travel target reached; stopping all systems.");
        stop_all_units(simulation);
        return;
    }

    if (alert->kind == ALERT_OUTPUT_FULL)
    {
        adjust_producers(simulation, alert->tank, FLIGHT_MODE_ECO);
    }
    else
    {
        adjust_producers(simulation, alert->tank, FLIGHT_MODE_BOOST);
    }
}

static void *manager_worker(void *context)
{
    FlightSimulation *simulation = context;

    while (atomic_load_explicit(&simulation->running, memory_order_acquire))
    {
        FlightAlert alert;
        while (alert_buffer_pop(&simulation->alerts, &alert))
        {
            handle_alert(simulation, &alert);
            if (!atomic_load_explicit(&simulation->running, memory_order_acquire))
            {
                break;
            }
        }
        sleep_for_ms(1);
    }

    return NULL;
}

bool flight_simulation_init(FlightSimulation *simulation)
{
    if (simulation == NULL)
    {
        return false;
    }

    memset(simulation, 0, sizeof(*simulation));
    atomic_init(&simulation->running, true);

    if (!alert_buffer_init(&simulation->alerts))
    {
        return false;
    }

    size_t initialized_tanks = 0;
    const struct
    {
        const char *label;
        int level;
        int limit;
    } tank_config[FLIGHT_TANK_COUNT] = {
        {"Propellant", 300, 300},
        {"Cabin air", 30, 60},
        {"Electrical power", 50, 60},
        {"Travel", 0, 600},
    };

    for (; initialized_tanks < FLIGHT_TANK_COUNT; ++initialized_tanks)
    {
        if (!tank_init(
                &simulation->tanks[initialized_tanks],
                tank_config[initialized_tanks].label,
                tank_config[initialized_tanks].level,
                tank_config[initialized_tanks].limit))
        {
            for (size_t index = 0; index < initialized_tanks; ++index)
            {
                sim_lock_destroy(&simulation->tanks[index].guard);
            }
            alert_buffer_destroy(&simulation->alerts);
            return false;
        }
    }

    unit_init(
        &simulation->units[0],
        "Drive",
        &simulation->tanks[0],
        3,
        &simulation->tanks[3],
        12,
        8,
        simulation);
    unit_init(
        &simulation->units[1],
        "Generator",
        &simulation->tanks[0],
        2,
        &simulation->tanks[2],
        5,
        10,
        simulation);
    unit_init(
        &simulation->units[2],
        "Air recycler",
        &simulation->tanks[2],
        3,
        &simulation->tanks[1],
        4,
        12,
        simulation);
    unit_init(
        &simulation->units[3],
        "Cabin",
        &simulation->tanks[1],
        2,
        NULL,
        0,
        15,
        simulation);

    return true;
}

bool flight_simulation_run(FlightSimulation *simulation)
{
    if (simulation == NULL)
    {
        return false;
    }

    if (pthread_create(
            &simulation->manager_thread,
            NULL,
            manager_worker,
            simulation) != 0)
    {
        return false;
    }

    size_t started_units = 0;
    for (; started_units < FLIGHT_UNIT_COUNT; ++started_units)
    {
        if (pthread_create(
                &simulation->unit_threads[started_units],
                NULL,
                unit_worker,
                &simulation->units[started_units]) != 0)
        {
            stop_all_units(simulation);
            break;
        }
    }

    (void)pthread_join(simulation->manager_thread, NULL);
    for (size_t index = 0; index < started_units; ++index)
    {
        (void)pthread_join(simulation->unit_threads[index], NULL);
    }

    puts("Final resource snapshot:");
    for (size_t index = 0; index < FLIGHT_TANK_COUNT; ++index)
    {
        SharedTank *tank = &simulation->tanks[index];
        printf("  %-18s %d / %d\n", tank->label, tank_snapshot(tank), tank->limit);
    }

    return started_units == FLIGHT_UNIT_COUNT;
}

void flight_simulation_destroy(FlightSimulation *simulation)
{
    if (simulation == NULL)
    {
        return;
    }
    alert_buffer_destroy(&simulation->alerts);
    for (size_t index = 0; index < FLIGHT_TANK_COUNT; ++index)
    {
        sim_lock_destroy(&simulation->tanks[index].guard);
    }
}
