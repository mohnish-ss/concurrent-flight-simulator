#ifndef FLIGHT_SIM_H
#define FLIGHT_SIM_H

#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FLIGHT_TANK_COUNT 4
#define FLIGHT_UNIT_COUNT 4
#define FLIGHT_LABEL_SIZE 32

typedef struct SimLock
{
#ifdef __APPLE__
    pthread_mutex_t native;
#else
    sem_t native;
#endif
} SimLock;

bool sim_lock_init(SimLock *lock);
void sim_lock_acquire(SimLock *lock);
void sim_lock_release(SimLock *lock);
void sim_lock_destroy(SimLock *lock);

typedef enum FlightMode
{
    FLIGHT_MODE_STOPPED,
    FLIGHT_MODE_ECO,
    FLIGHT_MODE_NOMINAL,
    FLIGHT_MODE_BOOST
} FlightMode;

typedef enum AlertKind
{
    ALERT_INPUT_EMPTY,
    ALERT_INPUT_LOW,
    ALERT_OUTPUT_FULL
} AlertKind;

typedef struct SharedTank
{
    char label[FLIGHT_LABEL_SIZE];
    int level;
    int limit;
    SimLock guard;
} SharedTank;

struct FlightUnit;

typedef struct FlightAlert
{
    AlertKind kind;
    int priority;
    uint64_t sequence;
    struct FlightUnit *source;
    SharedTank *tank;
    int observed_level;
} FlightAlert;

typedef struct AlertBuffer
{
    FlightAlert *items;
    size_t count;
    size_t capacity;
    uint64_t next_sequence;
    SimLock guard;
} AlertBuffer;

bool alert_buffer_init(AlertBuffer *buffer);
bool alert_buffer_push(AlertBuffer *buffer, FlightAlert alert);
bool alert_buffer_pop(AlertBuffer *buffer, FlightAlert *alert);
void alert_buffer_destroy(AlertBuffer *buffer);

typedef struct FlightUnit
{
    char label[FLIGHT_LABEL_SIZE];
    SharedTank *input;
    SharedTank *output;
    int input_rate;
    int output_rate;
    unsigned delay_ms;
    atomic_int mode;
    AlertBuffer *alerts;
    atomic_bool *running;
} FlightUnit;

typedef struct FlightSimulation
{
    SharedTank tanks[FLIGHT_TANK_COUNT];
    FlightUnit units[FLIGHT_UNIT_COUNT];
    AlertBuffer alerts;
    atomic_bool running;
    pthread_t manager_thread;
    pthread_t unit_threads[FLIGHT_UNIT_COUNT];
} FlightSimulation;

bool flight_simulation_init(FlightSimulation *simulation);
bool flight_simulation_run(FlightSimulation *simulation);
void flight_simulation_destroy(FlightSimulation *simulation);

#endif
