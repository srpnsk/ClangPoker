#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
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

// Состояния клиента
typedef enum { CLIENT_UNKNOWN, CLIENT_IN_LOBBY, CLIENT_IN_ROOM } ClientState;

// Протокол сообщений (должен соответствовать протоколу клиента)
typedef struct {
  int socket;
  ClientState state;
  int room_id;
  char username[32];
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

// Прототипы
void room_process(int room_id, int parent_fd, int max_participants);
int send_fd(int socket, int fd_to_send);
int receive_fd(int socket);
int create_server_socket(int port);
int set_nonblocking(int fd);
int create_socket_pair(int pair[2]);
void handle_unknown(int client_fd);
void handle_lobby(int client_fd);
void transfer_to_room(int client_fd, int desired_participants);
int create_room_process(int room_id, int max_participants);
RoomProcess *find_room_by_participants(int desired_participants);
int find_free_client_slot();
void remove_client(int client_fd);
void send_spam_to_client(int client_fd);
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

  if (sendmsg(sock, &msg, 0) < 0) {
    perror("sendmsg");
    return -1;
  }

  return 0;
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

int create_socket_pair(int pair[2]) {
  return socketpair(AF_UNIX, SOCK_STREAM, 0, pair);
}

int find_free_client_slot() {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].socket == 0) {
      return i;
    }
  }
  return -1;
}

void handle_unknown(int client_fd) {
  printf("Handling UNKNOWN for client %d\n", client_fd);

  Message msg = {0};
  msg.type = MSG_SYSTEM;
  strcpy(msg.text, "Welcome! Enter your name:");
  send_json_message(client_fd, MSG_SYSTEM, &msg);

  Message response;
  if (receive_json_message(client_fd, &response)) {
    if (response.type == MSG_HELLO && response.username[0]) {
      printf("Client %d introduced themselves as: %s\n", client_fd,
             response.username);

      int slot = find_free_client_slot();
      if (slot >= 0) {
        clients[slot].socket = client_fd;
        clients[slot].state = CLIENT_IN_LOBBY;
        strncpy(clients[slot].username, response.username,
                sizeof(clients[slot].username) - 1);
        client_count++;
      }
    }
  }
}

void handle_lobby(int client_fd) {
  printf("Handling LOBBY for client %d\n", client_fd);

  Message msg = {0};
  msg.type = MSG_SYSTEM;
  strcpy(msg.text,
         "Enter the desired number of participants in the room (from 1 to 10):");
  send_json_message(client_fd, MSG_SYSTEM, &msg);

  Message response;
  if (receive_json_message(client_fd, &response)) {
    if (response.type == MSG_JOIN_ROOM) {
      printf("Client %d wants a room with %d participants\n", client_fd,
             response.players);

      // Save desired number
      for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket == client_fd) {
          clients[i].desired_participants = response.players;
          break;
        }
      }

      // Send confirmation
      msg.type = MSG_SYSTEM;
      snprintf(msg.text, sizeof(msg.text),
               "Looking for a room with %d participants...",
               response.players);
      send_json_message(client_fd, MSG_SYSTEM, &msg);
    }
  }
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
  if (create_socket_pair(socket_pair) < 0) {
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

  int epoll_fd = epoll_create1(0);
  if (epoll_fd < 0) {
    perror("epoll_create1");
    exit(1);
  }

  struct epoll_event ev;
  ev.events = EPOLLIN;
  ev.data.fd = parent_fd;
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, parent_fd, &ev) < 0) {
    perror("epoll_ctl parent");
    exit(1);
  }

  int client_fds[100] = {0};
  int client_count = 0;
  int spam_sent = 0;
  struct epoll_event events[10];

  while (1) {
    int n = epoll_wait(epoll_fd, events, 10, -1);
    if (n < 0) {
      perror("epoll_wait");
      continue;
    }

    for (int i = 0; i < n; i++) {
      int fd = events[i].data.fd;

      if (fd == parent_fd) {
        while (1) {
          int new_client_fd = receive_fd(parent_fd);
          if (new_client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
              break;
            perror("receive_fd");
            break;
          }

          printf("[Room %d] Received client fd=%d\n", room_id, new_client_fd);

          if (client_count >= max_participants) {
            printf("[Room %d] Limit reached! Rejecting client fd=%d\n",
                   room_id, new_client_fd);

            Message msg = {0};
            msg.type = MSG_ERROR;
            strcpy(msg.text, "Room is full!");
            send_json_message(new_client_fd, MSG_ERROR, &msg);
            close(new_client_fd);
            continue;
          }

          set_nonblocking(new_client_fd);

          struct epoll_event client_ev;
          client_ev.events = EPOLLIN | EPOLLET;
          client_ev.data.fd = new_client_fd;

          if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_client_fd, &client_ev) < 0) {
            perror("epoll_ctl client");
            close(new_client_fd);
            continue;
          }

          client_fds[client_count++] = new_client_fd;

          Message msg = {0};
          msg.type = MSG_ROOM_READY;
          snprintf(msg.text, sizeof(msg.text),
                   "Welcome to room %d! Participants: %d/%d", room_id,
                   client_count, max_participants);
          send_json_message(new_client_fd, MSG_ROOM_READY, &msg);

          if (client_count == max_participants && !spam_sent) {
            printf("[Room %d] Limit reached! Sending spam\n", room_id);

            Message limit_msg = {0};
            limit_msg.type = MSG_SYSTEM;
            strcpy(limit_msg.text,
                   "=== PARTICIPANT LIMIT REACHED! ===");

            for (int j = 0; j < client_count; j++) {
              send_json_message(client_fds[j], MSG_SYSTEM, &limit_msg);
              send_spam_to_client(client_fds[j]);
            }
            spam_sent = 1;
          }
        }
      } else {
        Message msg;
        if (receive_json_message(fd, &msg)) {
          printf("[Room %d] Message from fd=%d: %s\n", room_id, fd, msg.text);

          // Broadcast to everyone in the room
          Message response = {0};
          response.type = MSG_ROOM_FORWARD;
          snprintf(response.text, sizeof(response.text),
                   "[Room %d] %s: %.458s", room_id,
                   msg.username[0] ? msg.username : "Anonymous", msg.text);

          for (int j = 0; j < client_count; j++) {
            if (client_fds[j] != fd) {
              send_json_message(client_fds[j], MSG_ROOM_FORWARD, &response);
            }
          }

          // Echo back to sender
          response.type = MSG_CHAT;
          snprintf(response.text, sizeof(response.text), "[Echo] %.502s",
                   msg.text);
          send_json_message(fd, MSG_CHAT, &response);
        } else {
          printf("[Room %d] Client fd=%d disconnected\n", room_id, fd);

          epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);

          for (int j = 0; j < client_count; j++) {
            if (client_fds[j] == fd) {
              client_fds[j] = client_fds[client_count - 1];
              client_count--;
              break;
            }
          }

          if (client_count < max_participants) {
            spam_sent = 0;
          }

          close(fd);
        }
      }
    }
  }
}

int run_server() {
  int server_fd = create_server_socket(SERVER_PORT);
  if (server_fd < 0) {
    return 1;
  }

  printf("Server started. Waiting for connections...\n");

  while (1) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    int client_fd =
        accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
    if (client_fd < 0) {
      perror("accept");
      continue;
    }

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
    printf("New connection: %s:%d (fd=%d)\n", client_ip,
           ntohs(client_addr.sin_port), client_fd);

    handle_unknown(client_fd);

    for (int i = 0; i < MAX_CLIENTS; i++) {
      if (clients[i].socket == client_fd) {
        handle_lobby(client_fd);
        transfer_to_room(client_fd, clients[i].desired_participants);
        break;
      }
    }
  }

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
