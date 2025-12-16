#include "server_utils.h"

volatile sig_atomic_t running = 1;

int main() {
  int server_fd = -1, epoll_fd = -1;
  struct sockaddr_in server_addr;
  struct epoll_event event, events[MAX_EVENTS];

  for (int i = 0; i < MAX_CONNECTIONS; i++) {
    clients[i].fd = -1;
  }

  struct sigaction sa;
  sa.sa_handler = handle_signal;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
  signal(SIGPIPE, SIG_IGN);

  printf("=== UNO Game Server (Multi-room) ===\n");
  printf("Port: %d\n", PORT);
  printf("Max rooms: %d\n", MAX_ROOMS);
  printf("Players per room: %d-%d\n", MIN_PLAYERS_PER_ROOM,
         MAX_PLAYERS_PER_ROOM);
  printf("Turn timeout: %d seconds\n", TURN_TIMEOUT);
  printf("====================================\n\n");

  srand(time(NULL));

  pthread_t timeout_thread;
  pthread_create(&timeout_thread, NULL, timeout_monitor, NULL);
  pthread_detach(timeout_thread);

  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    perror("Socket creation failed");
    exit(EXIT_FAILURE);
  }

  int opt_reuse = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt_reuse,
             sizeof(opt_reuse));
  set_keepalive(server_fd);

  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(PORT);

  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) ==
      -1) {
    perror("Bind failed");
    close(server_fd);
    exit(EXIT_FAILURE);
  }

  if (listen(server_fd, 128) == -1) {
    perror("Listen failed");
    close(server_fd);
    exit(EXIT_FAILURE);
  }

  printf("Server listening on port %d\n", PORT);

  if ((epoll_fd = epoll_create1(0)) == -1) {
    perror("epoll_create1 failed");
    close(server_fd);
    exit(EXIT_FAILURE);
  }

  event.events = EPOLLIN;
  event.data.fd = server_fd;
  epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event);

  while (running) {
    int event_count = epoll_wait(epoll_fd, events, MAX_EVENTS, 1000);

    if (event_count == -1) {
      if (errno == EINTR)
        continue;
      perror("epoll_wait failed");
      break;
    }

    for (int i = 0; i < event_count; i++) {
      if (events[i].events & EPOLLIN) {
        int client_fd = events[i].data.fd;

        if (client_fd == server_fd) {
          struct sockaddr_in client_addr;
          socklen_t client_len = sizeof(client_addr);

          client_fd =
              accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
          if (client_fd == -1) {
            perror("accept failed");
            continue;
          }

          if (client_count >= MAX_CONNECTIONS) {
            printf("Too many connections, rejecting client\n");
            close(client_fd);
            continue;
          }

          int saved = 0;
          for (int j = 0; j < MAX_CONNECTIONS; j++) {
            if (clients[j].fd == -1) {
              clients[j].fd = client_fd;
              clients[j].addr = client_addr;
              saved = 1;
              break;
            }
          }

          if (!saved) {
            printf("No space in clients array, rejecting\n");
            close(client_fd);
            continue;
          }

          set_keepalive(client_fd);

          int flags = fcntl(client_fd, F_GETFL, 0);
          fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

          struct epoll_event client_event;
          client_event.events = EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLHUP;
          client_event.data.fd = client_fd;

          if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_event) ==
              -1) {
            perror("epoll_ctl failed");
            close(client_fd);
            continue;
          }

          client_count++;

          char client_ip[INET_ADDRSTRLEN];
          inet_ntop(AF_INET, &client_addr.sin_addr, client_ip,
                    sizeof(client_ip));
          printf("New client: FD %d, IP: %s:%d, Total: %d\n", client_fd,
                 client_ip, ntohs(client_addr.sin_port), client_count);

          char *welcome = handle_client_command(client_fd, "/help");
          send(client_fd, welcome, strlen(welcome), 0);
          free(welcome);

        } else {
          char buffer[BUFFER_SIZE];
          ssize_t bytes_received;

          bytes_received = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);

          if (bytes_received > 0) {
            buffer[bytes_received] = '\0';

            size_t len = strlen(buffer);
            if (len > 0 && buffer[len - 1] == '\n') {
              buffer[len - 1] = '\0';
            }

            printf("Client %d: %s\n", client_fd, buffer);

            char *json_response = handle_client_command(client_fd, buffer);
            if (json_response) {
              send(client_fd, json_response, strlen(json_response), 0);
              free(json_response);
            }

          } else if (bytes_received == 0) {
            printf("Client disconnected: FD %d\n", client_fd);
            close_client(client_fd, epoll_fd);
          } else {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
              perror("recv failed");
              close_client(client_fd, epoll_fd);
            }
          }
        }
      }

      if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
        int client_fd = events[i].data.fd;
        if (client_fd != server_fd) {
          printf("Client disconnected (event): FD %d\n", client_fd);
          close_client(client_fd, epoll_fd);
        }
      }
    }

    static time_t last_cleanup = 0;
    time_t now = time(NULL);
    if (now - last_cleanup > 60) {
      cleanup_rooms();
      last_cleanup = now;
    }
  }

  printf("\nShutting down server...\n");

  pthread_mutex_lock(&rooms_mutex);
  Room *current = rooms;
  while (current) {
    if (current->is_active) {
      save_game_state(current);
    }
    current = current->next;
  }
  pthread_mutex_unlock(&rooms_mutex);

  for (int i = 0; i < MAX_CONNECTIONS; i++) {
    if (clients[i].fd > 0) {
      close(clients[i].fd);
    }
  }

  if (epoll_fd > 0)
    close(epoll_fd);
  if (server_fd > 0)
    close(server_fd);

  cleanup_rooms();

  printf("Server stopped. Total clients served: %d\n", client_count);
  return 0;
}
