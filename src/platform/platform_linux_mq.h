#ifndef PLATFORM_LINUX_MQ_H
#define PLATFORM_LINUX_MQ_H

#define _GNU_SOURCE

#include <mqueue.h>
#include <pthread.h>
#include <stddef.h>

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef mqd_t courrier_mq_t;

#endif // ifndef PLATFORM_LINUX_MQ_H
