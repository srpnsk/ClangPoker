#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "json_utils.h"

#define MAX_CLIENTS 1000
#define MAX_ROOMS 10
#define SERVER_PORT 8080

typedef enum { CLIENT_UNKNOWN, CLIENT_IN_LOBBY, CLIENT_IN_ROOM } ClientState;

typedef struct {
  int socket;
  ClientState state;
  int room_id;
  int desired_participants;
} Client;

typedef struct {
  pid_t pid;
  int control_fd;
  int client_count;
  int room_id;
  int max_participants;
} RoomProcess;

RoomProcess rooms[MAX_ROOMS] = {0};
Client clients[MAX_CLIENTS] = {0};
int client_count = 0;

struct pollfd pfds[MAX_CLIENTS + 1];
int nfds = 1;

void room_process(int room_id, int parent_fd, int max_participants);
void server_loop(int server_fd);
int send_fd(int socket, int fd_to_send);
int receive_fd(int socket);
int create_server_socket(int port);
int set_nonblocking(int fd);
void transfer_to_room(int client_fd, int desired_participants);
Client *find_client(int fd);
void add_client(int fd);
int create_room_process(int room_id, int max_participants);
RoomProcess *find_room_by_participants(int desired_participants);
int find_free_client_slot();
void remove_client(int client_fd);
void send_spam_to_client(int client_fd);
void handle_message(Client *c, Message *msg);
int run_server();

void send_spam_to_client(int client_fd) {
  Message msg = {0};
  msg.type = MSG_SYSTEM;

  char *spam_messages[] = {"SPAM: Free money!",
                           "SPAM: You won an iPhone!",
                           "SPAM: Urgent! Your account was hacked!",
                           "SPAM: Invest in cryptocurrency!",
                           "SPAM: 90% discount today only!",
                           "SPAM: Your card has been blocked!",
                           "SPAM: Inheritance from Nigeria!",
                           "SPAM: Your computer is infected!",
                           "SPAM: Urgent call from the bank!",
                           "SPAM: Activate bonuses!"};

  int num_messages = 5 + rand() % 6;

  for (int i = 0; i < num_messages; i++) {
    snprintf(msg.text, sizeof(msg.text), "[%d/%d] %s", i + 1, num_messages,
             spam_messages[rand() % 10]);
    send_json_message(client_fd, MSG_SYSTEM, &msg);
    usleep(200000);
  }

  strcpy(msg.text, "=== END OF SPAM ===");
  send_json_message(client_fd, MSG_SYSTEM, &msg);
}

int send_fd(int sock, int fd_to_send) {
  struct msghdr msg = {0};
  char buf = 0;
  struct iovec iov = {.iov_base = &buf, .iov_len = sizeof(buf)};
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  char cmsgbuf[CMSG_SPACE(sizeof(int))];
  msg.msg_control = cmsgbuf;
  msg.msg_controllen = sizeof(cmsgbuf);

  struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_RIGHTS;
  cmsg->cmsg_len = CMSG_LEN(sizeof(int));

  memcpy(CMSG_DATA(cmsg), &fd_to_send, sizeof(int));

  return sendmsg(sock, &msg, 0) == 1 ? 0 : -1;
}

int receive_fd(int sock) {
  struct msghdr msg = {0};
  char buf;
  struct iovec iov = {.iov_base = &buf, .iov_len = sizeof(buf)};
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  char cmsgbuf[CMSG_SPACE(sizeof(int))];
  msg.msg_control = cmsgbuf;
  msg.msg_controllen = sizeof(cmsgbuf);

  ssize_t n = recvmsg(sock, &msg, 0);
  if (n < 0)
    return -1;

  struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
  if (!cmsg || cmsg->cmsg_level != SOL_SOCKET ||
      cmsg->cmsg_type != SCM_RIGHTS) {
    errno = EPROTO;
    return -1;
  }

  int fd;
  memcpy(&fd, CMSG_DATA(cmsg), sizeof(int));
  return fd;
}

int create_server_socket(int port) {
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    perror("socket");
    return -1;
  }

  int opt = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  struct sockaddr_in addr = {0};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("bind");
    close(server_fd);
    return -1;
  }

  if (listen(server_fd, 10) < 0) {
    perror("listen");
    close(server_fd);
    return -1;
  }

  printf("Server is listening on port %d\n", port);
  return server_fd;
}

int set_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0)
    return -1;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int find_free_client_slot() {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].socket == 0) {
      return i;
    }
  }
  return -1;
}

RoomProcess *find_room_by_participants(int desired_participants) {
  for (int i = 0; i < MAX_ROOMS; i++) {
    if (rooms[i].pid > 0 && rooms[i].max_participants == desired_participants &&
        rooms[i].client_count < desired_participants) {
      return &rooms[i];
    }
  }
  return NULL;
}

int create_room_process(int room_id, int max_participants) {
  int socket_pair[2];
  if (socketpair(AF_UNIX, SOCK_DGRAM, 0, socket_pair) < 0) {
    perror("socketpair");
    return -1;
  }

  pid_t pid = fork();
  if (pid < 0) {
    perror("fork");
    return -1;
  }

  if (pid == 0) {
    close(socket_pair[0]);
    room_process(room_id, socket_pair[1], max_participants);
    exit(0);
  }

  close(socket_pair[1]);

  for (int i = 0; i < MAX_ROOMS; i++) {
    if (rooms[i].pid == 0) {
      rooms[i].pid = pid;
      rooms[i].control_fd = socket_pair[0];
      rooms[i].room_id = room_id;
      rooms[i].client_count = 0;
      rooms[i].max_participants = max_participants;

      printf("Room %d created (PID=%d) with a limit of %d participants\n",
             room_id, pid, max_participants);
      return i;
    }
  }

  return -1;
}

void transfer_to_room(int client_fd, int desired_participants) {
  printf("Searching for a room for client %d (wants %d participants)\n",
         client_fd, desired_participants);

  RoomProcess *room = find_room_by_participants(desired_participants);

  if (!room) {
    static int next_room_id = 1;
    int room_id = next_room_id++;

    printf("Creating a new room %d with a limit of %d\n", room_id,
           desired_participants);

    int room_index = create_room_process(room_id, desired_participants);
    if (room_index < 0) {
      printf("Failed to create room\n");
      Message msg = {0};
      msg.type = MSG_ERROR;
      strcpy(msg.text, "Room creation error");
      send_json_message(client_fd, MSG_ERROR, &msg);
      close(client_fd);
      return;
    }
    room = &rooms[room_index];
  }

  set_nonblocking(client_fd);

  if (send_fd(room->control_fd, client_fd) < 0) {
    perror("send_fd");
    close(client_fd);
    return;
  }

  room->client_count++;
  printf("Client %d transferred to room %d (clients: %d/%d)\n", client_fd,
         room->room_id, room->client_count, room->max_participants);

  remove_client(client_fd);
}

void remove_client(int client_fd) {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].socket == client_fd) {
      clients[i].socket = 0;
      clients[i].state = CLIENT_UNKNOWN;
      client_count--;
      break;
    }
  }
}

void room_process(int room_id, int parent_fd, int max_participants) {
  printf("[Room %d PID=%d] Started. Maximum participants: %d\n", room_id,
         getpid(), max_participants);

  set_nonblocking(parent_fd);

  struct pollfd pfds[1 + max_participants];
  int nfds = 1;
  int spam_sent = 0;

  pfds[0].fd = parent_fd;
  pfds[0].events = POLLIN;

  while (1) {
    int ready = poll(pfds, nfds, -1);
    if (ready < 0) {
      perror("poll");
      continue;
    }

    if (pfds[0].revents & POLLIN) {
      while (1) {
        int new_fd = receive_fd(parent_fd);
        if (new_fd < 0) {
          if (errno == EAGAIN || errno == EWOULDBLOCK)
            break;
          perror("receive_fd");
          break;
        }

        int clients = nfds - 1;

        if (clients >= max_participants) {
          Message err = {0};
          err.type = MSG_ERROR;
          strcpy(err.text, "Room is full!");
          send_json_message(new_fd, MSG_ERROR, &err);
          close(new_fd);
          continue;
        }

        set_nonblocking(new_fd);

        pfds[nfds].fd = new_fd;
        pfds[nfds].events = POLLIN;
        nfds++;

        Message welcome = {0};
        welcome.type = MSG_ROOM_READY;
        snprintf(welcome.text, sizeof(welcome.text),
                 "Welcome to room %d! Participants: %d/%d", room_id,
                 clients + 1, max_participants);
        send_json_message(new_fd, MSG_ROOM_READY, &welcome);

        if (clients + 1 == max_participants && !spam_sent) {
          Message limit = {0};
          limit.type = MSG_SYSTEM;
          strcpy(limit.text, "=== PARTICIPANT LIMIT REACHED! ===");

          for (int i = 1; i < nfds; i++) {
            send_json_message(pfds[i].fd, MSG_SYSTEM, &limit);
            send_spam_to_client(pfds[i].fd);
          }
          spam_sent = 1;
        }
      }
    }

    for (int i = 1; i < nfds; i++) {
      if (!(pfds[i].revents & POLLIN))
        continue;

      Message msg;
      int fd = pfds[i].fd;

      if (receive_json_message(fd, &msg)) {
        Message forward = {0};
        forward.type = MSG_ROOM_FORWARD;
        snprintf(forward.text, sizeof(forward.text), "[Room %d] %s: %.469s",
                 room_id, msg.username[0] ? msg.username : "Anonymous",
                 msg.text);

        for (int j = 1; j < nfds; j++) {
          if (pfds[j].fd != fd) {
            send_json_message(pfds[j].fd, MSG_ROOM_FORWARD, &forward);
          }
        }

        Message echo = {0};
        echo.type = MSG_CHAT;
        snprintf(echo.text, sizeof(echo.text), "[Echo] %.504s", msg.text);
        send_json_message(fd, MSG_CHAT, &echo);

      } else {
        close(fd);

        pfds[i] = pfds[nfds - 1];
        nfds--;
        i--;

        if (nfds - 1 < max_participants)
          spam_sent = 0;
      }
    }
  }
}

Client *find_client(int fd) {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].socket == fd)
      return &clients[i];
  }
  return NULL;
}

void server_loop(int server_fd) {
  while (1) {
    int ret = poll(pfds, nfds, -1);
    if (ret < 0) {
      perror("poll");
      continue;
    }

    if (pfds[0].revents & POLLIN) {
      while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
          if (errno == EAGAIN || errno == EWOULDBLOCK)
            break;
          perror("accept");
          break;
        }

        set_nonblocking(client_fd);
        add_client(client_fd);
        Message msg = {0};
        msg.type = MSG_SYSTEM_HELLO;
        strcpy(msg.text, "Enter your username");
        send_json_message(client_fd, MSG_SYSTEM_HELLO, &msg);
      }
    }
    for (int i = 1; i < nfds; i++) {
      if (!(pfds[i].revents & POLLIN))
        continue;

      Client *c = find_client(pfds[i].fd);
      if (!c) {
        remove_client(pfds[i].fd);
        continue;
      }

      Message msg;
      int r = receive_json_message(c->socket, &msg);
      if (r <= 0) {
        remove_client(c->socket);
        continue;
      }

      handle_message(c, &msg);
    }
  }
}

void handle_message(Client *c, Message *msg) {
  Message reply = {0};

  switch (c->state) {

  case CLIENT_UNKNOWN:
    if (msg->type != MSG_HELLO) {
      return;
    }

    c->state = CLIENT_IN_LOBBY;

    reply.type = MSG_SYSTEM_INVITE;
    strcpy(reply.text, "Enter the desired number of participants (2-6)");
    send_json_message(c->socket, reply.type, &reply);
    break;

  case CLIENT_IN_LOBBY:
    if (msg->type != MSG_JOIN_ROOM || msg->players < 1 || msg->players > 10) {
      reply.type = MSG_ERROR;
      strcpy(reply.text, "Expected JOIN_ROOM with value 1–10");
      send_json_message(c->socket, MSG_ERROR, &reply);
      return;
    }

    c->desired_participants = msg->players;

    reply.type = MSG_SYSTEM;
    snprintf(reply.text, sizeof(reply.text),
             "Looking for room with %d participants...", msg->players);
    send_json_message(c->socket, reply.type, &reply);

    transfer_to_room(c->socket, c->desired_participants);
    break;

  case CLIENT_IN_ROOM:
    break;
  }
}

void add_client(int fd) {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].socket == 0) {
      clients[i].socket = fd;
      clients[i].state = CLIENT_UNKNOWN;
      clients[i].desired_participants = 0;

      pfds[nfds].fd = fd;
      pfds[nfds].events = POLLIN;
      nfds++;
      return;
    }
  }
  close(fd);
}

int run_server() {
  int server_fd = create_server_socket(SERVER_PORT);
  if (server_fd < 0) {
    return 1;
  }

  set_nonblocking(server_fd);

  pfds[0].fd = server_fd;
  pfds[0].events = POLLIN;
  nfds = 1;

  printf("Server started. Waiting for connections...\n");

  server_loop(server_fd);

  close(server_fd);
  return 0;
}

int main() {
  signal(SIGPIPE, SIG_IGN);
  signal(SIGCHLD, SIG_IGN);

  srand(time(NULL));

  printf("Starting TCP server with JSON messages...\n");
  printf("Port: %d\n", SERVER_PORT);

  return run_server();
}
