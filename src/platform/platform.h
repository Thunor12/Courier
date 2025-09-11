#ifndef PLATFORM_H
#define PLATFORM_H

#include "platform_linux_mq.h"

courrier_mq_t courier_queue_open_reader(const char *queue_name, size_t msg_size, long maxmsg);
courrier_mq_t courier_queue_open_writer(const char *queue_name, size_t msg_size, long maxmsg);
int courier_send_mq(courrier_mq_t mq, const void *msg, size_t msg_size);
int courier_send_to(const char *queue_name, const void *msg, size_t msg_size);
int courier_queue_close(courrier_mq_t mq);
int courier_queue_unlink(const char *queue_name);

#endif // ifndef PLATFORM_H
