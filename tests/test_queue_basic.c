// =============================
// File: tests/test_queue_basic.c
// =============================
#include "courier.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef struct
{
    int a;
    float b;
} Payload;

int main(void)
{
    const char *Q = "/courier_test_q";
    size_t SZ = sizeof(Payload);

    // Reader opens first so attr is defined
    mqd_t r = courier_queue_open_reader(Q, SZ, 4);
    assert(r != (mqd_t)-1);

    // Writer: open-on-demand helper
    Payload out = {42, 3.14f};
    int sret = courier_send_to(Q, &out, SZ);
    assert(sret == 0);

    // Receive
    Payload in = {0};
    ssize_t recvd = mq_receive(r, (char *)&in, SZ, NULL);
    assert(recvd == (ssize_t)SZ);
    assert(in.a == out.a);
    assert(in.b == out.b);

    printf("[test_queue_basic] PASS\n");

    courier_queue_close(r);
    courier_queue_unlink(Q);
    return 0;
}