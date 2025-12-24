#include "msg.h"
#include "utils.h"
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

volatile sig_atomic_t running = 1;
client all_clients[MAX_CLIENTS] = {[0 ... MAX_CLIENTS - 1] = {"", -1, -1}};
room all_rooms[MAX_ROOMS] = {[0 ... MAX_ROOMS - 1] = {-1, 0}};
struct pollfd pfds[MAX_CLIENTS + 1] = {[0 ... MAX_CLIENTS] = {-1, 0, 0}};
int nfds = 1;
int anons = 0;

bool need_lobby_refresh = false;

void handle_signal(int sig) {
  if (sig == SIGINT || sig == SIGTERM) {
    printf("\nReceived SIGINT or SIGTERM. Goodbye!\n");
    running = 0;
  }
}

void build_room_list(message *empty_message) {
  char *p = empty_message->text;
  uint8_t room_count = 0;
  int fact_count = 0;
  for (int i = 0; i < MAX_ROOMS; i++) {
    if (all_rooms[i].max_participants != -1) {
      fact_count++;
      if (all_rooms[i].current_participants < all_rooms[i].max_participants)
        room_count++;
    }
  }

  *p++ = room_count;
  if (room_count == 0) {
    if (fact_count == MAX_ROOMS) {
      *p++ = 0; // 0 означает, что все, нет шансов
      goto success;
    } else {
      *p++ = 1; // 1 означает, что еще есть шансы на присоединение: надо создать
                // комнату
      goto success;
    }
  }

  for (int r = 0; r < MAX_ROOMS; r++) {
    if (all_rooms[r].max_participants == -1)
      continue;
    if (all_rooms[r].current_participants == all_rooms[r].max_participants)
      continue;
    if ((size_t)(p - empty_message->text) + 3 > TEXT)
      goto overflow;
    *p++ = (uint8_t)r;
    *p++ = (uint8_t)all_rooms[r].current_participants;
    *p++ = (uint8_t)all_rooms[r].max_participants;

    for (int i = 0; i < MAX_CLIENTS; i++) {
      if (all_clients[i].fd != -1 && all_clients[i].room_ind == r) {
        if ((size_t)(p - empty_message->text) + USERNAME > TEXT)
          goto overflow;
        memcpy(p, all_clients[i].username, USERNAME);
        p += USERNAME;
      }
    }
  }
  goto success;
success:
  empty_message->type = MSG_LIST_ROOMS;
  return;
overflow:
  snprintf(empty_message->text, TEXT, "Room list overflow");
  return;
}

void refresh_lobby() {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (all_clients[i].fd == -1)
      continue;

    if (all_clients[i].username[0] && all_clients[i].room_ind == -1) {
      message msg;
      init_msg(&msg);
      build_room_list(&msg);
      if (write(all_clients[i].fd, &msg, sizeof(msg)) != sizeof(msg)) {
        remove_client(all_clients[i].fd);
        need_lobby_refresh = true;
      }
    }
  }
}

void handle_message(client *cli, message *request) {
  message *reply = malloc(sizeof(message));
  init_msg(reply);
  if (request->type == MSG_EXIT) {
    remove_client(cli->fd);
    need_lobby_refresh = true;
    goto cleanup;
  }
  if (!(cli->username[0])) {
    if (request->type != MSG_HELLO) {
      remove_client(cli->fd);
      need_lobby_refresh = true;
      goto cleanup;
    }
    if (!(request->username[0])) {
      snprintf(cli->username, USERNAME, "Anonymous%d", anons);
      anons++;
    } else
      snprintf(cli->username, USERNAME, "%s", request->username);
    build_room_list(reply);
    if (write(cli->fd, reply, sizeof(message)) == -1) {
      remove_client(cli->fd);
      need_lobby_refresh = true;
      goto cleanup;
    }
    goto cleanup;
  }
  if (cli->room_ind == -1) {
    if ((request->type != MSG_JOIN_ROOM) ||
        ((request->room_id == -1) && (request->participants == -1))) {
      remove_client(cli->fd);
      need_lobby_refresh = true;
      goto cleanup;
    }
    if (request->room_id != -1) {
      set_room(cli,
               request->room_id); // в случае ошибки мы по сути просто не
                                  // добавляем клиента в комнату и он остается в
                                  // лобби, но он получает обновленный (на самом
                                  // деле ничего не изменилось) список комнат
      need_lobby_refresh = true;
      goto cleanup;
    }
    if (request->participants !=
        -1) { // мы создаем новую комнату ТОЛЬКО если нет свободного места в той
              // комнате, в которой такая же вместимость, как и в запрашиваемой
      for (int i = 0; i < MAX_ROOMS; i++) {
        if ((all_rooms[i].max_participants == request->participants) &&
            (all_rooms[i].current_participants <
             all_rooms[i].max_participants)) {
          set_room(cli, i);
          need_lobby_refresh = true;
          goto cleanup;
        }
      }
      set_room(cli, create_room(request->participants));
      if (cli->room_ind == -1) {
        snprintf(
            reply->text, TEXT,
            "Failed to create room for client [%s]: no more available rooms!",
            cli->username); // прекрасно, мы не удаляем клиента при ошибке
        if (write(cli->fd, reply, sizeof(message)) != sizeof(message)) {
          remove_client(cli->fd);
          need_lobby_refresh = true;
          goto cleanup;
        }
      }
      need_lobby_refresh = true;
    }
    if (cli->room_ind != -1) { // успешно добавили клиента в комнату -> должны
                               // уведомить участников
      int fds[all_rooms[cli->room_ind].max_participants];
      int count = get_clients_in_room(cli->room_ind, fds);
      init_msg(reply);
      reply->type = MSG_CHAT;
      snprintf(reply->text, TEXT, "Server: user %s joined this room",
               cli->username);
      for (int i = 0; i < count; i++) {
        if (write(fds[i], reply, sizeof(message)) != sizeof(message)) {
          remove_client(fds[i]);
          need_lobby_refresh = true;
          continue;
        }
      }
    }
    goto cleanup;
  }
  if (cli->room_ind != -1) {
    if (request->type == MSG_LEAVE_ROOM) {
      int backup = cli->room_ind;
      set_room(cli, -1);
      need_lobby_refresh = true;
      int fds[all_rooms[backup].max_participants];
      int count = get_clients_in_room(backup, fds);
      init_msg(reply);
      reply->type = MSG_CHAT;
      snprintf(reply->text, TEXT, "Server: user %s left this room",
               cli->username);
      for (int i = 0; i < count; i++) {
        if (write(fds[i], reply, sizeof(message)) != sizeof(message)) {
          remove_client(fds[i]);
          need_lobby_refresh = true;
          continue;
        }
      }
      goto cleanup;
    } else if (request->type == MSG_CHAT) {
      int fds[all_rooms[cli->room_ind].max_participants];
      int count = get_clients_in_room(cli->room_ind, fds);
      reply->type = MSG_CHAT;
      snprintf(reply->text, TEXT, "%s: %.*s", cli->username,
               (TEXT - 1) - (int)(strlen(cli->username) + 2), request->text);
      for (int i = 0; i < count; i++) {
        if (write(fds[i], reply, sizeof(message)) != sizeof(message)) {
          remove_client(fds[i]);
          need_lobby_refresh = true;
          continue;
        }
      }
      goto cleanup;
    } else {
      remove_client(cli->fd);
      need_lobby_refresh = true;
      goto cleanup;
    }
  }

cleanup:
  free(reply);
  reply = NULL;
  free(request);
  request = NULL;
  return;
}

void server_loop(void) {
  while (running) {
    int ret = poll(pfds, nfds, 50);
    if (ret < 0) {
      if (errno == EINTR)
        continue;
      perror("poll");
      continue;
    }

    if (pfds[0].revents & (POLLHUP | POLLERR)) {
      fprintf(stderr, "listening socket is dead\n");
      break;
    }
    if (pfds[0].revents & POLLIN) {
      while (running) {
        int client_fd = accept(pfds[0].fd, NULL, NULL);
        if (client_fd < 0) {
          if (errno == EAGAIN || errno == EWOULDBLOCK)
            break;
          perror("accept");
          break;
        }

        fcntl(client_fd, F_SETFL, fcntl(client_fd, F_GETFL, 0) | O_NONBLOCK);

        message *msg = malloc(sizeof(message));
        init_msg(msg);
        if (add_client(client_fd) < 0) {
          strcpy(msg->text, "No more slots for clients, sorry");
          write(client_fd, msg, sizeof(message));
          free(msg);
          msg = NULL;
          continue;
        }
        msg->type = MSG_HELLO;
        if (write(client_fd, msg, sizeof(message)) != sizeof(message)) {
          remove_client(client_fd);
          need_lobby_refresh = true;
        }
        free(msg);
        msg = NULL;
      }
    }
    for (int i = 1; i < nfds; i++) {
      if (pfds[i].revents & (POLLHUP | POLLERR)) {
        remove_client(pfds[i].fd);
        need_lobby_refresh = true;
      }
      if (pfds[i].revents & POLLIN) {
        client *c = find_client_by_fd(pfds[i].fd);
        if (!c) {
          continue;
        }
        message *msg = malloc(sizeof(message));
        ssize_t r = read(c->fd, msg, sizeof(message));
        if (r != sizeof(message)) {
          remove_client(c->fd);
          need_lobby_refresh = true;
          free(msg);
          msg = NULL;
          continue;
        }
        handle_message(c, msg);
      }
    }
    if (need_lobby_refresh) {
      need_lobby_refresh = false;
      refresh_lobby();
    }
  }
}

int main(void) {
  struct sigaction act;
  sigemptyset(&act.sa_mask);
  act.sa_handler = handle_signal;
  act.sa_flags = 0;
  sigaction(SIGINT, &act, NULL);
  sigaction(SIGTERM, &act, NULL);

  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    perror("socket");
    return EXIT_FAILURE;
  }

  int opt = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  struct sockaddr_in addr = (struct sockaddr_in){.sin_family = AF_INET,
                                                 .sin_port = htons(PORT),
                                                 .sin_addr.s_addr = INADDR_ANY};

  if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("bind");
    close(server_fd);
    return EXIT_FAILURE;
  }

  if (listen(server_fd, 10) < 0) {
    perror("listen");
    close(server_fd);
    return EXIT_FAILURE;
  }

  printf("Server is listening on port %d\n", PORT);

  fcntl(server_fd, F_SETFL, fcntl(server_fd, F_GETFL, 0) | O_NONBLOCK);

  pfds[0] = (struct pollfd){.fd = server_fd, .events = POLLIN, .revents = 0};

  server_loop();

  for (int i = 0; i < nfds; i++) {
    if (pfds[i].fd != -1)
      close(pfds[i].fd);
  }

  return EXIT_SUCCESS;
}
