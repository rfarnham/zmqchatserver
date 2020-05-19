#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zmq.h>

static void join_channel(void *messages_in, const char* name) {
  int rc = zmq_setsockopt(messages_in, ZMQ_SUBSCRIBE, name, strlen(name));
  assert(rc == 0);
}

static void leave_channel(void *messages_in, const char* name) {
  int rc = zmq_setsockopt(messages_in, ZMQ_UNSUBSCRIBE, name, strlen(name));
  assert(rc == 0);
}

static int message_number;
static void display_messages(void *socket) {
  printf("\n[%d] ", message_number++);
  zmq_msg_t message;
  do {
    zmq_msg_init(&message);
    zmq_msg_recv(&message, socket, 0);
    fwrite(zmq_msg_data(&message), sizeof(char), zmq_msg_size(&message), stdout);
    zmq_msg_close(&message);
  } while (zmq_msg_more(&message));
  printf("\n> ");
  fflush(stdout);
}

static void handle_user_input(void *user_input, void *messages_in, void *messages_out) {
  char buf[4096];
  int cap = sizeof(buf);
  int len = zmq_recv(user_input, buf, cap - 1, 0);
  buf[len < cap ? len : cap - 1] = '\0';

  const char* join = "/join ";
  const char* leave = "/leave ";
  if (strncmp(buf, join, strlen(join)) == 0) {
    join_channel(messages_in, buf + strlen(join));
  } else if (strncmp(buf, leave, strlen(leave)) == 0) {
    leave_channel(messages_in, buf + strlen(leave));
  } else {
    int size = len < cap ? len : cap;
    zmq_send(messages_out, buf, size, 0);
  }
}

static void *input_reader(void *context) {
  void *socket = zmq_socket(context, ZMQ_PAIR);
  zmq_connect(socket, "inproc://user_input");
  
  char* line = NULL;
  size_t linecap = 0;
  while (1) {
    fputs("> ", stdout);
    ssize_t linelen = getline(&line, &linecap, stdin);
    line[linelen - 1] = '\0';
    zmq_send(socket, line, linelen, 0);
  }

  // Just for cleanliness
  free(line);
  zmq_close(socket);

  return NULL;
}

int main(void) {
  void *context = zmq_ctx_new();
  void *messages_in = zmq_socket(context, ZMQ_SUB);
  void *messages_out = zmq_socket(context, ZMQ_PUB);
  void *user_input = zmq_socket(context, ZMQ_PAIR);

  int rc;
  rc = zmq_connect(messages_in, "tcp://localhost:8889");
  assert(rc == 0);
  
  rc = zmq_connect(messages_out, "tcp://localhost:8888");
  assert(rc == 0);

  rc = zmq_bind(user_input, "inproc://user_input");
  assert(rc == 0);

  pthread_t input_thread;
  pthread_create(&input_thread, NULL, &input_reader, context);

  while (1) {
    zmq_pollitem_t pollitems[2] = {
      {messages_in, 0, ZMQ_POLLIN, 0},
      {user_input, 0, ZMQ_POLLIN, 0}
    };
    zmq_poll(pollitems, 2, -1);

    if (pollitems[0].revents & ZMQ_POLLIN) {
      display_messages(pollitems[0].socket);
    }

    if (pollitems[1].revents & ZMQ_POLLIN) {
      handle_user_input(pollitems[1].socket, messages_in, messages_out);
    }
  }

  pthread_join(input_thread, NULL);
  zmq_close(user_input);
  zmq_close(messages_out);
  zmq_close(messages_in);
  zmq_ctx_destroy(context);
  
  return 0;
}
