// =============================
// File: include/courier.h
// =============================
#pragma once
#define _GNU_SOURCE
#include <mqueue.h>
#include <pthread.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // --- Message handler signature ---
    typedef void (*CourierMessageHandler)(void *user_data, void *msg);

    // --- Per-message definition owned by an Actor ---
    typedef struct
    {
        const char *queue_name;        // e.g. "/sensor_tick"
        size_t msg_size;               // sizeof(payload)
        CourierMessageHandler handler; // called on receive (in actor thread)
        mqd_t mq;                      // reader descriptor (opened by courier_actor_init)
    } CourierActorMsgDef;

    // --- Actor ---
    typedef struct
    {
        const char *name;
        CourierActorMsgDef *msgs; // array of message defs
        size_t nb_msgs;           // length of msgs[]
        void *user_data;          // opaque pointer passed to handlers
        pthread_t thread;         // actor thread
    } CourierActor;

    // ===== Queue helpers (safe building blocks) =====
    // Open a queue for reading (creates if needed). Returns (mqd_t)-1 on error.
    mqd_t courier_queue_open_reader(const char *queue_name, size_t msg_size, long maxmsg);

    // Open a queue for writing (creates if needed). Returns (mqd_t)-1 on error.
    mqd_t courier_queue_open_writer(const char *queue_name, size_t msg_size, long maxmsg);

    // Send using an already-opened writer descriptor.
    int courier_send_mq(mqd_t mq, const void *msg, size_t msg_size);

    // Convenience: open-on-demand writer, send, then close. Returns 0 on success.
    int courier_send_to(const char *queue_name, const void *msg, size_t msg_size);

    // Close/unlink helpers
    int courier_queue_close(mqd_t mq);
    int courier_queue_unlink(const char *queue_name);

    // ===== Actor API =====
    // Initialize: synchronously create/open all actor queues for reading and start the actor thread.
    // Returns 0 on success, <0 on error.
    int courier_actor_init(CourierActor *actor,
                           const char *name,
                           CourierActorMsgDef *msgs,
                           size_t nb_msgs,
                           void *user_data);

    // Graceful close: cancels and joins the thread; closes & unlinks queues.
    void courier_actor_close(CourierActor *actor);

#ifdef __cplusplus
}
#endif