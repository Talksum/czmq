#include "czmq.h"

#define TEST_MSG "thisisatestoftheemergencybroadcastsystem"

static void
s_consumer_task (void *args, zctx_t *ctx, void *pipe)
{
    free (zstr_recv (pipe));
    zstr_send (pipe, "pong");

    void *back_s = zsocket_new (ctx, ZMQ_ROUTER);
    assert (back_s);
    zsocket_connect (back_s, "inproc://proxy_back");

    int i;
    for (i=0; i<1000000; i++) {
        free (zmsg_recv (back_s));
    }

    fprintf (stderr, "DONE\n");
}

static void 
s_producer_task (void *args, zctx_t *ctx, void *pipe)
{
    void *front_s = zsocket_new (ctx, ZMQ_DEALER);
    assert (front_s);

    free (zstr_recv (pipe));
    zstr_send (pipe, "pong");

    zsocket_connect (front_s, "inproc://proxy_front");

    zclock_sleep (1);

    for (int i=0; i<1000000; i++) 
        zstr_send (front_s, TEST_MSG);
}

int
main (void)
{

    zctx_t *ctx = zctx_new ();

    // start the proxy
    
    char *frontend_addr = "inproc://proxy_front";
    char *backend_addr = "inproc://proxy_back";
    char *capture_addr = "inproc://proxy_capture";
    char *control_addr = "inproc://proxy_control";

    zproxy_t *proxy = zproxy_new (ctx, ZPROXY_QUEUE);

    int rc = zproxy_bind (proxy, frontend_addr, backend_addr, 
            capture_addr, control_addr);

    assert (rc == 0);

    zclock_sleep (1);

    // start the consumer

    void *consumer = zthread_fork (ctx, s_consumer_task, NULL);
    zstr_send (consumer, "START");
    zclock_sleep (1);
    char *ack = zstr_recv (consumer);
    assert (streq (ack, "pong"));

    // start the producer

    void *producer = zthread_fork (ctx, s_producer_task, NULL);
    zclock_sleep (1);
    zstr_send (producer, "START");
    ack = zstr_recv (producer);
    assert (streq (ack, "pong"));

    // Destroy the proxy
    zproxy_terminate (proxy);
    zctx_destroy (&ctx);
    zproxy_destroy (&proxy);

    zctx_destroy (&ctx);
    return EXIT_SUCCESS;

}
