// example_thermostat.c
#include "courier.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

/* Message types */
typedef struct
{
    float value;
} TempMsg;

typedef struct
{
    int on;
} HeaterCmdMsg;

typedef struct
{
    int tick;
} TickMsg;

/* Shared state for demo visibility */
static int g_heater_state = 0;

/* Sensor: receives TickMsg on /sensor_tick, sends TempMsg to /supervisor_temp */
void sensor_handle_tick(void *user_data, void *msg)
{
    (void)user_data;
    TickMsg *tick = (TickMsg *)msg;
    /* simulate a temperature reading (18.0 .. 25.0) */
    TempMsg t;
    t.value = 18.0f + (rand() % 70) / 10.0f;
    printf("[Sensor] tick=%d -> temp=%.1f°C\n", tick->tick, t.value);

    if (courier_send_to("/supervisor_temp", &t, sizeof(t)) != 0)
    {
        fprintf(stderr, "[Sensor] failed to send TempMsg to /supervisor_temp\n");
    }
}

/* Supervisor: receives TempMsg on /supervisor_temp, sends HeaterCmdMsg to /heater_cmd */
void supervisor_handle_temp(void *user_data, void *msg)
{
    (void)user_data;
    TempMsg *t = (TempMsg *)msg;
    printf("[Supervisor] received temp=%.1f°C\n", t->value);

    HeaterCmdMsg cmd;
    if (t->value < 19.0f && !g_heater_state)
    {
        cmd.on = 1;
        printf("[Supervisor] temp low -> request heater ON\n");
        courier_send_to("/heater_cmd", &cmd, sizeof(cmd));
    }
    else if (t->value > 22.0f && g_heater_state)
    {
        cmd.on = 0;
        printf("[Supervisor] temp high -> request heater OFF\n");
        courier_send_to("/heater_cmd", &cmd, sizeof(cmd));
    }
}

/* Heater: receives HeaterCmdMsg on /heater_cmd */
void heater_handle_cmd(void *user_data, void *msg)
{
    (void)user_data;
    HeaterCmdMsg *c = (HeaterCmdMsg *)msg;
    g_heater_state = c->on;
    printf("[Heater] state -> %s\n", g_heater_state ? "ON" : "OFF");
}

/* Main: create actors, send ticks to sensor, then shutdown */
int main(void)
{
    srand((unsigned)time(NULL));

    /* Define message queues for each actor (these are reader-side definitions) */
    CourierActorMsgDef sensor_defs[] = {
        {"/sensor_tick", sizeof(TickMsg), sensor_handle_tick, (mqd_t)-1}};
    CourierActorMsgDef supervisor_defs[] = {
        {"/supervisor_temp", sizeof(TempMsg), supervisor_handle_temp, (mqd_t)-1}};
    CourierActorMsgDef heater_defs[] = {
        {"/heater_cmd", sizeof(HeaterCmdMsg), heater_handle_cmd, (mqd_t)-1}};

    /* Actors (structs) */
    CourierActor sensor = {.name = "Sensor", .msgs = sensor_defs, .nb_msgs = 1, .user_data = NULL};
    CourierActor sup = {.name = "Supervisor", .msgs = supervisor_defs, .nb_msgs = 1, .user_data = NULL};
    CourierActor heater = {.name = "Heater", .msgs = heater_defs, .nb_msgs = 1, .user_data = NULL};

    /* Initialize actors: this opens reader queues and starts their threads */
    if (courier_actor_init(&sup, "Supervisor", supervisor_defs, 1, NULL) != 0)
    {
        fprintf(stderr, "Failed to init Supervisor\n");
        return 1;
    }
    if (courier_actor_init(&heater, "Heater", heater_defs, 1, NULL) != 0)
    {
        fprintf(stderr, "Failed to init Heater\n");
        courier_actor_close(&sup);
        return 1;
    }
    if (courier_actor_init(&sensor, "Sensor", sensor_defs, 1, NULL) != 0)
    {
        fprintf(stderr, "Failed to init Sensor\n");
        courier_actor_close(&heater);
        courier_actor_close(&sup);
        return 1;
    }

    /* Drive the sensor by sending TickMsg to /sensor_tick (writer opens on-demand) */
    const int N = 20;
    for (int i = 0; i < N; ++i)
    {
        TickMsg tm = {.tick = i};
        if (courier_send_to("/sensor_tick", &tm, sizeof(tm)) != 0)
        {
            fprintf(stderr, "failed to send tick %d\n", i);
        }
        sleep(1);
    }

    /* Shutdown */
    courier_actor_close(&sensor);
    courier_actor_close(&heater);
    courier_actor_close(&sup);

    return 0;
}
