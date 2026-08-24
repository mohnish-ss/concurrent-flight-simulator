#include "flight_sim.h"

#include <stdlib.h>

static bool alert_precedes(const FlightAlert *left, const FlightAlert *right)
{
    if (left->priority != right->priority)
    {
        return left->priority > right->priority;
    }
    return left->sequence < right->sequence;
}

static void swap_alerts(FlightAlert *left, FlightAlert *right)
{
    FlightAlert temporary = *left;
    *left = *right;
    *right = temporary;
}

bool alert_buffer_init(AlertBuffer *buffer)
{
    if (buffer == NULL || !sim_lock_init(&buffer->guard))
    {
        return false;
    }

    buffer->capacity = 16;
    buffer->count = 0;
    buffer->next_sequence = 0;
    buffer->items = calloc(buffer->capacity, sizeof(*buffer->items));
    if (buffer->items == NULL)
    {
        sim_lock_destroy(&buffer->guard);
        return false;
    }
    return true;
}

bool alert_buffer_push(AlertBuffer *buffer, FlightAlert alert)
{
    if (buffer == NULL)
    {
        return false;
    }

    sim_lock_acquire(&buffer->guard);
    if (buffer->count == buffer->capacity)
    {
        const size_t expanded_capacity = buffer->capacity * 2;
        FlightAlert *expanded = realloc(
            buffer->items,
            expanded_capacity * sizeof(*expanded));
        if (expanded == NULL)
        {
            sim_lock_release(&buffer->guard);
            return false;
        }
        buffer->items = expanded;
        buffer->capacity = expanded_capacity;
    }

    alert.sequence = buffer->next_sequence++;
    size_t index = buffer->count++;
    buffer->items[index] = alert;

    while (index > 0)
    {
        const size_t parent = (index - 1) / 2;
        if (alert_precedes(&buffer->items[parent], &buffer->items[index]))
        {
            break;
        }
        swap_alerts(&buffer->items[parent], &buffer->items[index]);
        index = parent;
    }

    sim_lock_release(&buffer->guard);
    return true;
}

bool alert_buffer_pop(AlertBuffer *buffer, FlightAlert *alert)
{
    if (buffer == NULL || alert == NULL)
    {
        return false;
    }

    sim_lock_acquire(&buffer->guard);
    if (buffer->count == 0)
    {
        sim_lock_release(&buffer->guard);
        return false;
    }

    *alert = buffer->items[0];
    --buffer->count;
    if (buffer->count > 0)
    {
        buffer->items[0] = buffer->items[buffer->count];
        size_t index = 0;

        for (;;)
        {
            const size_t left = index * 2 + 1;
            const size_t right = left + 1;
            size_t next = index;

            if (left < buffer->count &&
                alert_precedes(&buffer->items[left], &buffer->items[next]))
            {
                next = left;
            }
            if (right < buffer->count &&
                alert_precedes(&buffer->items[right], &buffer->items[next]))
            {
                next = right;
            }
            if (next == index)
            {
                break;
            }

            swap_alerts(&buffer->items[index], &buffer->items[next]);
            index = next;
        }
    }

    sim_lock_release(&buffer->guard);
    return true;
}

void alert_buffer_destroy(AlertBuffer *buffer)
{
    if (buffer == NULL)
    {
        return;
    }
    free(buffer->items);
    buffer->items = NULL;
    buffer->count = 0;
    buffer->capacity = 0;
    sim_lock_destroy(&buffer->guard);
}
