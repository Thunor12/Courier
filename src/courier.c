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

// ----- Queue helpers -----
static void fill_attr(struct mq_attr *attr, size_t msg_size, long maxmsg)
{
    memset(attr, 0, sizeof(*attr));
    attr->mq_flags = 0; // blocking by default
    attr->mq_maxmsg = (maxmsg > 0 ? maxmsg : 10);
    attr->mq_msgsize = msg_size;
}

mqd_t courier_queue_open_reader(const char *queue_name, size_t msg_size, long maxmsg)
{
    if (!queue_name || msg_size == 0)
    {
        errno = EINVAL;
        return (mqd_t)-1;
    }
    struct mq_attr attr;
    fill_attr(&attr, msg_size, maxmsg);
    // Clean old instance to ensure msg_size matches what we expect
    mq_unlink(queue_name);
    mqd_t mq = mq_open(queue_name, O_RDONLY | O_CREAT, 0644, &attr);
    if (mq == (mqd_t)-1)
    {
        perror("mq_open(reader)");
    }
    return mq;
}

mqd_t courier_queue_open_writer(const char *queue_name, size_t msg_size, long maxmsg)
{
    if (!queue_name || msg_size == 0)
    {
        errno = EINVAL;
        return (mqd_t)-1;
    }
    struct mq_attr attr;
    fill_attr(&attr, msg_size, maxmsg);
    mqd_t mq = mq_open(queue_name, O_WRONLY | O_CREAT, 0644, &attr);
    if (mq == (mqd_t)-1)
    {
        perror("mq_open(writer)");
    }
    return mq;
}

int courier_send_mq(mqd_t mq, const void *msg, size_t msg_size)
{
    if (mq == (mqd_t)-1 || !msg || msg_size == 0)
    {
        errno = EINVAL;
        return -1;
    }
    int ret = mq_send(mq, (const char *)msg, msg_size, 0);
    if (ret < 0)
        perror("mq_send");
    return ret;
}

int courier_send_to(const char *queue_name, const void *msg, size_t msg_size)
{
    if (!queue_name || !msg || msg_size == 0)
    {
        errno = EINVAL;
        return -1;
    }
    mqd_t mq = courier_queue_open_writer(queue_name, msg_size, 10);
    if (mq == (mqd_t)-1)
        return -1;
    int ret = courier_send_mq(mq, msg, msg_size);
    courier_queue_close(mq);
    return ret;
}

int courier_queue_close(mqd_t mq) { return mq_close(mq); }
int courier_queue_unlink(const char *queue_name) { return mq_unlink(queue_name); }

// ----- Actor thread loop -----
static void *actor_loop(void *arg)
{
    CourierActor *actor = (CourierActor *)arg;

    // Prepare poll fds (Linux-specific: mqd_t is a file descriptor)
    struct pollfd *fds = calloc(actor->nb_msgs, sizeof(struct pollfd));
    if (!fds)
        return NULL;

    for (size_t i = 0; i < actor->nb_msgs; i++)
    {
        // We opened the queues in courier_actor_init; just attach to poll
        fds[i].fd = (int)actor->msgs[i].mq; // Linux: mqd_t is an FD (non-portable)
        fds[i].events = POLLIN;
    }

    // TODO add a shutdown mechanism instead of relying on pthread_cancel
    for (;;)
    {
        int ret = poll(fds, actor->nb_msgs, -1); // block
        if (ret < 0)
        {
            if (errno == EINTR)
                continue;
            perror("poll");
            break;
        }
        for (size_t i = 0; i < actor->nb_msgs; i++)
        {
            if (fds[i].revents & POLLIN)
            {
                size_t sz = actor->msgs[i].msg_size;
                char *buf = (char *)malloc(sz);
                if (!buf)
                    continue;
                ssize_t r = mq_receive(actor->msgs[i].mq, buf, sz, NULL);
                if (r < 0)
                {
                    perror("mq_receive");
                    free(buf);
                    continue;
                }
                // Optional size check
                if ((size_t)r != sz)
                {
                    fprintf(stderr,
                            "[Courier %s] Warn: received %zd bytes on %s (expected %zu)\n",
                            actor->name, r, actor->msgs[i].queue_name, sz);
                }
                // Dispatch
                actor->msgs[i].handler(actor->user_data, buf);
                free(buf);
            }
        }
    }

    free(fds);
    return NULL;
}

int courier_actor_init(CourierActor *actor,
                       const char *name,
                       CourierActorMsgDef *msgs,
                       size_t nb_msgs,
                       void *user_data)
{
    if (!actor || !name || !msgs || nb_msgs == 0)
    {
        errno = EINVAL;
        return -1;
    }
    actor->name = name;
    actor->msgs = msgs;
    actor->nb_msgs = nb_msgs;
    actor->user_data = user_data;

    // Open all queues for reading synchronously *before* starting thread to avoid races
    for (size_t i = 0; i < nb_msgs; i++)
    {
        mqd_t mq = courier_queue_open_reader(msgs[i].queue_name, msgs[i].msg_size, 10);
        if (mq == (mqd_t)-1)
        {
            // Cleanup previously opened
            for (size_t j = 0; j < i; j++)
            {
                mq_close(msgs[j].mq);
                mq_unlink(msgs[j].queue_name);
            }
            return -1;
        }
        msgs[i].mq = mq;
    }

    int rc = pthread_create(&actor->thread, NULL, actor_loop, actor);
    if (rc != 0)
    {
        perror("pthread_create");
        for (size_t i = 0; i < nb_msgs; i++)
        {
            mq_close(msgs[i].mq);
            mq_unlink(msgs[i].queue_name);
        }
        return -1;
    }
    return 0;
}

void courier_actor_close(CourierActor *actor)
{
    if (!actor)
        return;
    // Stop the thread the simple way for now
    pthread_cancel(actor->thread);
    pthread_join(actor->thread, NULL);

    for (size_t i = 0; i < actor->nb_msgs; i++)
    {
        mq_close(actor->msgs[i].mq);
        mq_unlink(actor->msgs[i].queue_name);
    }
}