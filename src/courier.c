// =============================
// File: src/courier.c
// =============================
#include "courier.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef COURIER_MAX_MSG_SIZE
#define COURIER_MAX_MSG_SIZE 256
#endif /* ifndef COURIER_MAX_MSG_SIZE */

// ----- Actor thread loop -----
static void* actor_loop(void *arg)
{
    CourierActor *actor = (CourierActor *)arg;
    char buf[COURIER_MAX_MSG_SIZE];

    // Prepare poll fds (Linux-specific: mqd_t is a file descriptor)
    struct pollfd fds[actor->nb_msgs];

    for(size_t i = 0; i < actor->nb_msgs; i++)
    {
        // We opened the queues in courier_actor_init; just attach to poll
        fds[i].fd     = (int)actor->msgs[i].mq; // Linux: mqd_t is an FD (non-portable)
        fds[i].events = POLLIN;
    }

    // TODO add a shutdown mechanism instead of relying on pthread_cancel
    for(;;)
    {
        int ret = poll(fds, actor->nb_msgs, -1); // block

        if(ret < 0)
        {
            if(errno == EINTR)
            {
                continue;
            }
            perror("poll");
            break;
        }

        for(size_t i = 0; i < actor->nb_msgs; i++)
        {
            if(fds[i].revents & POLLIN)
            {
                const size_t sz = actor->msgs[i].msg_size;

                ssize_t r = mq_receive(actor->msgs[i].mq, buf, sz, NULL);

                if(r < 0)
                {
                    perror("mq_receive");
                    continue;
                }

                // Optional size check
                if((size_t)r != sz)
                {
                    fprintf(stderr, "[Courier %s] Warn: received %zd bytes on %s (expected %zu)\n", actor->name, r, actor->msgs[i].queue_name, sz);
                }
                // Dispatch
                actor->msgs[i].handler(actor->user_data, buf);
            }
        }
    }

    return NULL;
}

int courier_actor_init(CourierActor *actor, const char *name, CourierActorMsgDef *msgs, size_t nb_msgs, void *user_data)
{
    if(!actor || !name || !msgs || (nb_msgs == 0))
    {
        errno = EINVAL;

        return -1;
    }
    actor->name      = name;
    actor->msgs      = msgs;
    actor->nb_msgs   = nb_msgs;
    actor->user_data = user_data;

    // Open all queues for reading synchronously *before* starting thread to avoid races
    for(size_t i = 0; i < nb_msgs; i++)
    {
        courrier_mq_t mq = courier_queue_open_reader(msgs[i].queue_name, msgs[i].msg_size, 10);

        if(mq == (courrier_mq_t)-1)
        {
            // Cleanup previously opened
            for(size_t j = 0; j < i; j++)
            {
                courier_queue_close(msgs[j].mq);
                courier_queue_unlink(msgs[j].queue_name);
            }

            return -1;
        }
        msgs[i].mq = mq;
    }

    int rc = pthread_create(&actor->thread, NULL, actor_loop, actor);

    if(rc != 0)
    {
        perror("pthread_create");

        for(size_t i = 0; i < nb_msgs; i++)
        {
            courier_queue_close(msgs[i].mq);
            courier_queue_unlink(msgs[i].queue_name);
        }

        return -1;
    }

    return 0;
}

void courier_actor_close(CourierActor *actor)
{
    if(!actor)
    {
        return;
    }
    // Stop the thread the simple way for now
    pthread_cancel(actor->thread);
    pthread_join(actor->thread, NULL);

    for(size_t i = 0; i < actor->nb_msgs; i++)
    {
        courier_queue_close(actor->msgs[i].mq);
        courier_queue_unlink(actor->msgs[i].queue_name);
    }
}
