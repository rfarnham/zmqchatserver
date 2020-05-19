#include <assert.h>
#include <pthread.h>
#include <zmq.h>

void *tailer(void *context) {
  void *socket = zmq_socket(context, ZMQ_PAIR);
  zmq_connect(socket, "inproc://tailer");

  for (int i = 0; ; i++) {
    char buf[4096];
    int cap = sizeof(buf);
    int len = zmq_recv(socket, buf, cap - 1, 0);
    buf[cap - 1] = '\0';

    if (i % 1000 == 0) {
      printf("[tailer] [%d] %.*s\n", i, len < cap ? len : cap, buf);
    }
  }

  zmq_close(socket);
}

int main(void) {
  void *context = zmq_ctx_new();
  void *clients_in = zmq_socket(context, ZMQ_SUB);
  void *clients_out = zmq_socket(context, ZMQ_PUB);
  void *msgs = zmq_socket(context, ZMQ_PAIR);
    
  int rc;
  zmq_setsockopt(clients_in, ZMQ_SUBSCRIBE, NULL, 0);
  rc = zmq_bind(clients_in, "tcp://*:8888");
  assert(rc == 0);

  rc = zmq_bind(clients_out, "tcp://*:8889");
  assert(rc == 0);

  rc = zmq_bind(msgs, "inproc://tailer");
  assert(rc == 0);

  pthread_t tailer_thread;
  pthread_create(&tailer_thread, NULL, &tailer, context);
  
  zmq_proxy(clients_in, clients_out, msgs);

  // zmq_proxy never returns, but just for cleanliness...
  pthread_join(tailer_thread, NULL);
  zmq_close(clients_in);
  zmq_close(clients_out);
  zmq_ctx_destroy(context);
  
  return 0;
}
