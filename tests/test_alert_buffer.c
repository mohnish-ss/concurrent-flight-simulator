#include "flight_sim.h"

#include <assert.h>
#include <stdio.h>

static void push(AlertBuffer *buffer, int priority, int marker)
{
    const FlightAlert alert = {
        .kind = ALERT_INPUT_LOW,
        .priority = priority,
        .sequence = 0,
        .source = NULL,
        .tank = NULL,
        .observed_level = marker,
    };
    assert(alert_buffer_push(buffer, alert));
}

int main(void)
{
    AlertBuffer buffer;
    assert(alert_buffer_init(&buffer));

    push(&buffer, 1, 10);
    push(&buffer, 3, 20);
    push(&buffer, 3, 30);
    push(&buffer, 2, 40);

    const int expected[] = {20, 30, 40, 10};
    for (size_t index = 0; index < sizeof(expected) / sizeof(expected[0]); ++index)
    {
        FlightAlert alert;
        assert(alert_buffer_pop(&buffer, &alert));
        assert(alert.observed_level == expected[index]);
    }

    FlightAlert unused;
    assert(!alert_buffer_pop(&buffer, &unused));
    alert_buffer_destroy(&buffer);
    puts("Alert buffer tests passed.");
    return 0;
}
