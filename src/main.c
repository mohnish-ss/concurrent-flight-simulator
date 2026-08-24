#include "flight_sim.h"

#include <stdio.h>

int main(void)
{
    FlightSimulation simulation;
    if (!flight_simulation_init(&simulation))
    {
        fputs("Unable to initialize the flight simulation.\n", stderr);
        return 1;
    }

    const bool completed = flight_simulation_run(&simulation);
    flight_simulation_destroy(&simulation);
    return completed ? 0 : 1;
}
