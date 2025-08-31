// =============================
// File: tests/test_actor_basic.c
// =============================
#include "courier.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Messages
typedef struct
{
    int tick;
} TickMsg;
typedef struct
{
    float value;
} TempMsg;

// Queues
#define Q_SENSOR_TICK "/sensor_tick"
#define Q_SUP_TEMP "/supervisor_temp"

// Actor state
typedef struct
{
    int ticks_seen;
} SensorState;

typedef struct
{
    int received;
} SupervisorState;

// Handlers
static void sensor_handle_tick(void *user_data, void *msg)
{
    SensorState *st = (SensorState *)user_data;
    (void)msg;
    st->ticks_seen++;
    TempMsg t = {.value = 18.0f + (rand() % 60) / 10.0f};
    courier_send_to(Q_SUP_TEMP, &t, sizeof(t));
}

static void supervisor_handle_temp(void *user_data, void *msg)
{
    SupervisorState *st = (SupervisorState *)user_data;
    TempMsg *t = (TempMsg *)msg;
    st->received++;
    printf("[supervisor] temp=%.1f\n", t->value);
}

int main(void)
{
    srand(1234);

    // Define actors
    SensorState sstate = {0};
    SupervisorState supstate = {0};

    CourierActorMsgDef sensor_defs[] = {
        {Q_SENSOR_TICK, sizeof(TickMsg), sensor_handle_tick, (mqd_t)-1},
    };
    CourierActor sensor = {.name = "Sensor", .msgs = sensor_defs, .nb_msgs = 1, .user_data = &sstate};

    CourierActorMsgDef sup_defs[] = {
        {Q_SUP_TEMP, sizeof(TempMsg), supervisor_handle_temp, (mqd_t)-1},
    };
    CourierActor supervisor = {.name = "Supervisor", .msgs = sup_defs, .nb_msgs = 1, .user_data = &supstate};

    // Init actors (opens queues for reading and starts threads)
    assert(courier_actor_init(&supervisor, supervisor.name, sup_defs, 1, &supstate) == 0);
    assert(courier_actor_init(&sensor, sensor.name, sensor_defs, 1, &sstate) == 0);

    // Drive sensor with a few ticks (writer opens per send)
    for (int i = 0; i < 5; i++)
    {
        TickMsg tm = {.tick = i};
        assert(courier_send_to(Q_SENSOR_TICK, &tm, sizeof(tm)) == 0);
        usleep(100 * 1000);
    }

    // Give the actor time to process
    sleep(1);

    printf("[test_actor_basic] sensor ticks=%d supervisor received=%d\n",
           sstate.ticks_seen, supstate.received);

    // Minimal assertions
    assert(sstate.ticks_seen == 5);
    assert(supstate.received >= 5);

    courier_actor_close(&sensor);
    courier_actor_close(&supervisor);

    printf("[test_actor_basic] PASS\n");
    return 0;
}