
#include "platform.h"

// ----- Queue helpers -----
static void fill_attr(struct mq_attr *attr, size_t msg_size, long maxmsg)
{
    memset(attr, 0, sizeof(*attr));
    attr->mq_flags   = 0; // blocking by default
    attr->mq_maxmsg  = (maxmsg > 0 ? maxmsg : 10);
    attr->mq_msgsize = msg_size;
}

courrier_mq_t courier_queue_open_reader(const char *queue_name, size_t msg_size, long maxmsg)
{
    if(!queue_name || (msg_size == 0))
    {
        errno = EINVAL;

        return (courrier_mq_t)-1;
    }
    struct mq_attr attr;
    fill_attr(&attr, msg_size, maxmsg);
    // Clean old instance to ensure msg_size matches what we expect
    mq_unlink(queue_name);
    courrier_mq_t mq = mq_open(queue_name, O_RDONLY | O_CREAT, 0644, &attr);

    if(mq == (courrier_mq_t)-1)
    {
        perror("mq_open(reader)");
    }

    return mq;
}

courrier_mq_t courier_queue_open_writer(const char *queue_name, size_t msg_size, long maxmsg)
{
    if(!queue_name || (msg_size == 0))
    {
        errno = EINVAL;

        return (courrier_mq_t)-1;
    }
    struct mq_attr attr;
    fill_attr(&attr, msg_size, maxmsg);
    courrier_mq_t mq = mq_open(queue_name, O_WRONLY | O_CREAT, 0644, &attr);

    if(mq == (courrier_mq_t)-1)
    {
        perror("mq_open(writer)");
    }

    return mq;
}

int courier_send_mq(courrier_mq_t mq, const void *msg, size_t msg_size)
{
    if((mq == (courrier_mq_t)-1) || !msg || (msg_size == 0))
    {
        errno = EINVAL;

        return -1;
    }
    int ret = mq_send(mq, (const char *)msg, msg_size, 0);

    if(ret < 0)
    {
        perror("mq_send");
    }

    return ret;
}

int courier_send_to(const char *queue_name, const void *msg, size_t msg_size)
{
    if(!queue_name || !msg || (msg_size == 0))
    {
        errno = EINVAL;

        return -1;
    }
    courrier_mq_t mq = courier_queue_open_writer(queue_name, msg_size, 10);

    if(mq == (courrier_mq_t)-1)
    {
        return -1;
    }
    int ret = courier_send_mq(mq, msg, msg_size);
    courier_queue_close(mq);

    return ret;
}

int courier_queue_close(courrier_mq_t mq)
{
    return mq_close(mq);
}

int courier_queue_unlink(const char *queue_name)
{
    return mq_unlink(queue_name);
}
