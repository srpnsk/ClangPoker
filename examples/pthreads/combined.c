#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#define MAX_CLIENTS 1000
#define MAX_ROOMS 10
#define BUFFER_SIZE 1024
#define SERVER_PORT 8080

// Состояния клиента
typedef enum { CLIENT_UNKNOWN, CLIENT_IN_LOBBY, CLIENT_IN_ROOM } ClientState;

// Структура клиента (используется в родителе)
typedef struct {
  int socket;
  ClientState state;
  int room_id;
  char username[32];
} Client;

// Структура процесса комнаты
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
int handle_lobby(int client_fd);
void transfer_to_room(int client_fd, int desired_participants);
int create_room_process(int room_id, int max_participants);
RoomProcess *find_room_by_participants(int desired_participants);
RoomProcess *find_room_process(int room_id);
int find_free_client_slot();
void remove_client(int client_fd);
void send_spam_to_client(int client_fd);
ssize_t send_all(int fd, const void *buf, size_t len);
int run_server();

ssize_t send_all(int fd, const void *buf, size_t len) {
  size_t total = 0;
  const char *p = buf;

  while (total < len) {
    ssize_t n = send(fd, p + total, len - total, 0);

    if (n > 0) {
      total += n;
    } else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      return total; // отправили сколько смогли
    } else {
      return -1; // ошибка или клиент умер
    }
  }

  return total;
}

// Функция для отправки мусора клиенту
void send_spam_to_client(int client_fd) {
  printf("[DEBUG] Отправляем спам клиенту fd=%d\n", client_fd);

  char *spam_messages[] = {"SPAM: Бесплатные деньги!\n",
                           "SPAM: Вы выиграли iPhone!\n",
                           "SPAM: Срочно! Ваш аккаунт взломали!\n",
                           "SPAM: Инвестируйте в криптовалюту!\n",
                           "SPAM: Скидка 90% только сегодня!\n",
                           "SPAM: Ваша карта заблокирована!\n",
                           "SPAM: Наследство из Нигерии!\n",
                           "SPAM: Ваш компьютер заражен!\n",
                           "SPAM: Срочный звонок из банка!\n",
                           "SPAM: Активируйте бонусы!\n"};

  int num_messages = 5 + rand() % 6;

  for (int i = 0; i < num_messages; i++) {
    char msg[BUFFER_SIZE];
    snprintf(msg, sizeof(msg), "[%d/%d] %s", i + 1, num_messages,
             spam_messages[rand() % 10]);

    ssize_t sent = send_all(client_fd, msg, strlen(msg));
    printf("[DEBUG] Отправлено %ld байт спама клиенту fd=%d\n", sent,
           client_fd);

    if (sent <= 0) {
      printf("[DEBUG] Ошибка отправки спама: %s\n", strerror(errno));
    }

    usleep(200000);
  }

  char *limit_msg = "=== КОНЕЦ СПАМА ===\n";
  send_all(client_fd, limit_msg, strlen(limit_msg));
}

int send_fd(int sock, int fd_to_send) {
  struct msghdr msg = {0};

  char buf = 0; // dummy payload
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
  if (n < 0) {
    return -1; // errno снаружи
  }

  struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
  if (!cmsg) {
    errno = EPROTO;
    return -1;
  }

  if (cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS) {
    errno = EPROTO;
    return -1;
  }

  int fd;
  memcpy(&fd, CMSG_DATA(cmsg), sizeof(int));
  return fd;
}

// Создание серверного сокета
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

  printf("Сервер слушает порт %d\n", port);
  return server_fd;
}

// Неблокирующий режим
int set_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0)
    return -1;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// Пара сокетов
int create_socket_pair(int pair[2]) {
  int result = socketpair(AF_UNIX, SOCK_STREAM, 0, pair);
  printf("[DEBUG] socketpair создан: [%d, %d]\n", pair[0], pair[1]);
  return result;
}

// Свободный слот для клиента
int find_free_client_slot() {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].socket == 0) {
      return i;
    }
  }
  return -1;
}

// Обработка UNKNOWN
void handle_unknown(int client_fd) {
  printf("Обработка UNKNOWN для клиента %d\n", client_fd);

  // Простое приветствие
  char *welcome_msg = "Добро пожаловать! Введите ваше имя: ";
  ssize_t sent = send_all(client_fd, welcome_msg, strlen(welcome_msg));
  printf("[DEBUG] Отправлено приветствие: %ld байт\n", sent);

  char buffer[BUFFER_SIZE];
  ssize_t bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

  if (bytes > 0) {
    buffer[bytes] = '\0';
    printf("Клиент %d представился как: %s\n", client_fd, buffer);

    int slot = find_free_client_slot();
    if (slot >= 0) {
      clients[slot].socket = client_fd;
      clients[slot].state = CLIENT_IN_LOBBY;
      strncpy(clients[slot].username, buffer,
              sizeof(clients[slot].username) - 1);
      client_count++;
    }
  }
}

// Обработка LOBBY
int handle_lobby(int client_fd) {
  printf("Обработка LOBBY для клиента %d\n", client_fd);

  char *question =
      "Введите желаемое количество участников в комнате (от 1 до 10): ";
  send_all(client_fd, question, strlen(question));

  char buffer[BUFFER_SIZE];
  ssize_t bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

  if (bytes > 0) {
    buffer[bytes] = '\0';
    int desired_participants = atoi(buffer);

    if (desired_participants >= 1 && desired_participants <= 10) {
      printf("Клиент %d хочет комнату с %d участниками\n", client_fd,
             desired_participants);
      return desired_participants;
    } else {
      send_all(client_fd,
               "Некорректный ввод. Используем значение по умолчанию (3).\n",
               60);
      return 3;
    }
  }

  return 3;
}

// Найти комнату по ID
RoomProcess *find_room_process(int room_id) {
  for (int i = 0; i < MAX_ROOMS; i++) {
    if (rooms[i].pid > 0 && rooms[i].room_id == room_id) {
      return &rooms[i];
    }
  }
  return NULL;
}

// Найти комнату по количеству участников
RoomProcess *find_room_by_participants(int desired_participants) {
  for (int i = 0; i < MAX_ROOMS; i++) {
    if (rooms[i].pid > 0 && rooms[i].max_participants == desired_participants &&
        rooms[i].client_count < desired_participants) {
      printf("[DEBUG] Найдена комната %d с лимитом %d (сейчас %d/%d)\n",
             rooms[i].room_id, desired_participants, rooms[i].client_count,
             rooms[i].max_participants);
      return &rooms[i];
    }
  }
  printf("[DEBUG] Не найдена комната с лимитом %d\n", desired_participants);
  return NULL;
}

// Создать процесс комнаты
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
    // Дочерний процесс - комната
    close(socket_pair[0]);
    printf("[DEBUG] Дочерний процесс запущен для комнаты %d\n", room_id);

    room_process(room_id, socket_pair[1], max_participants);
    exit(0);
  }

  // Родительский процесс
  close(socket_pair[1]);

  for (int i = 0; i < MAX_ROOMS; i++) {
    if (rooms[i].pid == 0) {
      rooms[i].pid = pid;
      rooms[i].control_fd = socket_pair[0];
      rooms[i].room_id = room_id;
      rooms[i].client_count = 0;
      rooms[i].max_participants = max_participants;

      printf("Создана комната %d (PID=%d) с лимитом %d участников\n", room_id,
             pid, max_participants);
      return i;
    }
  }

  return -1;
}

// Передать клиента в комнату
void transfer_to_room(int client_fd, int desired_participants) {
  printf("Поиск комнаты для клиента %d (желает %d участников)\n", client_fd,
         desired_participants);

  // Ищем существующую комнату
  RoomProcess *room = find_room_by_participants(desired_participants);

  if (!room) {
    // Создаем новую комнату
    static int next_room_id = 1;
    int room_id = next_room_id++;

    printf("Создаем новую комнату %d с лимитом %d\n", room_id,
           desired_participants);

    int room_index = create_room_process(room_id, desired_participants);
    if (room_index < 0) {
      printf("Не удалось создать комнату\n");
      close(client_fd);
      return;
    }
    room = &rooms[room_index];
  }

  // Устанавливаем неблокирующий режим для клиентского сокета
  set_nonblocking(client_fd);

  printf("[DEBUG] Передаем клиента fd=%d в комнату %d\n", client_fd,
         room->room_id);

  // Сначала передаем дескриптор
  if (send_fd(room->control_fd, client_fd) < 0) {
    perror("send_fd");
    close(client_fd);
    return;
  }

  room->client_count++;
  printf("Клиент %d передан в комнату %d (клиентов: %d/%d)\n", client_fd,
         room->room_id, room->client_count, room->max_participants);

  remove_client(client_fd);
}

// Удалить клиента
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

// Процесс комнаты (ИСПРАВЛЕННЫЙ)
void room_process(int room_id, int parent_fd, int max_participants) {
  printf("[Комната %d PID=%d] Запущена. Максимум участников: %d\n", room_id,
         getpid(), max_participants);

  // Устанавливаем родительский сокет в неблокирующий режим
  set_nonblocking(parent_fd);

  // Создаем epoll
  int epoll_fd = epoll_create1(0);
  if (epoll_fd < 0) {
    perror("epoll_create1");
    exit(1);
  }

  // Добавляем родительский сокет
  struct epoll_event ev;
  ev.events = EPOLLIN; // Edge-triggered режим
  ev.data.fd = parent_fd;
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, parent_fd, &ev) < 0) {
    perror("epoll_ctl parent");
    exit(1);
  }

  int client_fds[100] = {0};
  int client_count = 0;
  int spam_sent = 0;

  struct epoll_event events[10];

  // Отправляем тестовое сообщение в родительский процесс
  char ready_msg[100];
  snprintf(ready_msg, sizeof(ready_msg),
           "Комната %d готова к работе (PID=%d)\n", room_id, getpid());
  send_all(parent_fd, ready_msg, strlen(ready_msg));

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
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
              break; // fd пока нет — это нормально
            }
            perror("receive_fd");
            break;
          }
          printf("[Комната %d] Получен клиент fd=%d\n", +room_id,
                 new_client_fd); // Проверяем лимит

          if (client_count >= max_participants) {
            printf("[Комната %d] Лимит достигнут! Отказ клиенту fd=%d\n",
                   room_id, new_client_fd);

            char *reject_msg = "Комната заполнена!\n";
            send_all(new_client_fd, reject_msg, strlen(reject_msg));
            close(new_client_fd);
            continue;
          }

          // Настраиваем неблокирующий режим
          set_nonblocking(new_client_fd);

          // Добавляем в epoll
          struct epoll_event client_ev;
          client_ev.events = EPOLLIN | EPOLLET;
          client_ev.data.fd = new_client_fd;

          if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_client_fd, &client_ev) <
              0) {
            perror("epoll_ctl client");
            close(new_client_fd);
            continue;
          }

          client_fds[client_count++] = new_client_fd;

          // Отправляем приветствие
          char welcome[BUFFER_SIZE];
          snprintf(welcome, sizeof(welcome),
                   "Добро пожаловать в комнату %d! Участников: %d/%d\n",
                   room_id, client_count, max_participants);

          ssize_t sent = send_all(new_client_fd, welcome, strlen(welcome));
          printf(
              "[Комната %d] Отправлено приветствие клиенту fd=%d: %ld байт\n",
              room_id, new_client_fd, sent);

          // Проверяем достижение лимита
          if (client_count == max_participants && !spam_sent) {
            printf("[Комната %d] Лимит достигнут! Отправляем спам\n", room_id);

            for (int j = 0; j < client_count; j++) {
              char *limit_msg = "\n=== ЛИМИТ УЧАСТНИКОВ ДОСТИГНУТ! ===\n";
              send_all(client_fds[j], limit_msg, strlen(limit_msg));
              send_spam_to_client(client_fds[j]);
            }
            spam_sent = 1;
          }
        }

      } else {
        // Сообщение от клиента комнаты
        char buffer[BUFFER_SIZE];
        ssize_t bytes = recv(fd, buffer, sizeof(buffer) - 1, 0);

        if (bytes <= 0) {
          // Клиент отключился
          if (bytes == 0) {
            printf("[Комната %d] Клиент fd=%d отключился\n", room_id, fd);
          } else {
            printf("[Комната %d] Ошибка чтения от клиента fd=%d: %s\n", room_id,
                   fd, strerror(errno));
          }

          // Удаляем из epoll
          epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);

          // Удаляем из массива
          for (int j = 0; j < client_count; j++) {
            if (client_fds[j] == fd) {
              client_fds[j] = client_fds[client_count - 1];
              client_count--;
              break;
            }
          }

          // Сбрасываем флаг спама если нужно
          if (client_count < max_participants) {
            spam_sent = 0;
          }

          close(fd);
        } else {
          // Обрабатываем сообщение
          buffer[bytes] = '\0';
          printf("[Комната %d] Сообщение от fd=%d: %s\n", room_id, fd, buffer);

          // Эхо-ответ
          char response[BUFFER_SIZE];
          snprintf(response, sizeof(response), "[Комната %d] Эхо: %.900s",
                   room_id, buffer);

          ssize_t sent = send_all(fd, response, strlen(response));
          printf("[Комната %d] Отправлен ответ клиенту fd=%d: %ld байт\n",
                 room_id, fd, sent);
        }
      }
    }
  }
}

// Главная функция сервера
int run_server() {
  int server_fd = create_server_socket(SERVER_PORT);
  if (server_fd < 0) {
    return 1;
  }

  printf("Сервер запущен. Ожидание подключений...\n");

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
    printf("Новое подключение: %s:%d (fd=%d)\n", client_ip,
           ntohs(client_addr.sin_port), client_fd);

    handle_unknown(client_fd);
    int desired_participants = handle_lobby(client_fd);
    transfer_to_room(client_fd, desired_participants);
  }

  close(server_fd);
  return 0;
}

int main() {
  signal(SIGPIPE, SIG_IGN);
  signal(SIGCHLD, SIG_IGN);

  srand(time(NULL));

  printf("Запуск TCP сервера с процессами...\n");
  printf("Порт: %d\n", SERVER_PORT);
  printf("Отладка включена\n");

  return run_server();
}
