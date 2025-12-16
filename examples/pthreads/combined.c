#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>  // Добавляем для генерации случайных чисел

#define MAX_CLIENTS 1000
#define MAX_ROOMS 10
#define BUFFER_SIZE 1024
#define SERVER_PORT 8080

// Состояния клиента
typedef enum {
    CLIENT_UNKNOWN,
    CLIENT_IN_LOBBY,
    CLIENT_IN_ROOM
} ClientState;

// Структура клиента (используется в родителе)
typedef struct {
    int socket;
    ClientState state;
    int room_id;           // назначенная комната
    char username[32];     // имя клиента (если нужно)
} Client;

// Структура процесса комнаты
typedef struct {
    pid_t pid;             // PID дочернего процесса
    int control_fd;        // сокет для общения с родителем
    int client_count;
    int room_id;
    int max_participants;  // Максимальное количество участников
} RoomProcess;

// Глобальные переменные сервера
RoomProcess rooms[MAX_ROOMS] = {0};
Client clients[MAX_CLIENTS] = {0};
int client_count = 0;

// Прототипы функций
void room_process(int room_id, int parent_fd, int max_participants);
int send_fd(int socket, int fd_to_send);
int receive_fd(int socket);
int create_server_socket(int port);
int set_nonblocking(int fd);
int create_socket_pair(int pair[2]);
void handle_unknown(int client_fd);
int handle_lobby(int client_fd);
void transfer_to_room(int client_fd, int room_id, int max_participants);
int create_room_process(int room_id, int max_participants);
RoomProcess *find_room_by_participants(int desired_participants);
RoomProcess *find_room_process(int room_id);
int find_free_client_slot();
void remove_client(int client_fd);
void send_spam_to_client(int client_fd);  // Функция для отправки мусора
int run_server();

// Функция для отправки мусора клиенту
void send_spam_to_client(int client_fd) {
    printf("Отправляем мусор клиенту fd=%d\n", client_fd);
    
    // Генерируем случайное количество сообщений (от 3 до 10)
    srand(time(NULL) + client_fd);
    int num_messages = 3 + rand() % 8;
    
    for (int i = 0; i < num_messages; i++) {
        // Генерируем случайную длину сообщения (от 20 до 100 символов)
        int msg_len = 20 + rand() % 81;
        char spam_msg[msg_len + 1];
        
        // Заполняем сообщение случайными символами
        for (int j = 0; j < msg_len; j++) {
            spam_msg[j] = 32 + rand() % 95;  // Печатные символы ASCII
        }
        spam_msg[msg_len] = '\0';
        
        // Добавляем номер сообщения в начало
        char full_msg[BUFFER_SIZE];
        snprintf(full_msg, sizeof(full_msg), "[SPAM %d/%d] %s\n", 
                 i + 1, num_messages, spam_msg);
        
        send(client_fd, full_msg, strlen(full_msg), 0);
        
        // Небольшая задержка между сообщениями
        usleep(100000 + rand() % 200000);  // 100-300ms
    }
    
    send(client_fd, "=== КОНЕЦ СПАМА ===\n", 21, 0);
}

// Передача файлового дескриптора между процессами
int send_fd(int socket, int fd_to_send) {
  struct msghdr msg = {0};
  struct iovec iov[1];
  char buf[1] = {'!'};

  union {
    struct cmsghdr cm;
    char control[CMSG_SPACE(sizeof(int))];
  } control_un;

  msg.msg_control = control_un.control;
  msg.msg_controllen = sizeof(control_un.control);

  struct cmsghdr *cmptr = CMSG_FIRSTHDR(&msg);
  cmptr->cmsg_len = CMSG_LEN(sizeof(int));
  cmptr->cmsg_level = SOL_SOCKET;
  cmptr->cmsg_type = SCM_RIGHTS;
  *((int *)CMSG_DATA(cmptr)) = fd_to_send;

  iov[0].iov_base = buf;
  iov[0].iov_len = 1;
  msg.msg_iov = iov;
  msg.msg_iovlen = 1;

  return sendmsg(socket, &msg, 0);
}

// Прием дескриптора
int receive_fd(int socket) {
  struct msghdr msg = {0};
  struct iovec iov[1];
  char buf[1];

  union {
    struct cmsghdr cm;
    char control[CMSG_SPACE(sizeof(int))];
  } control_un;

  msg.msg_control = control_un.control;
  msg.msg_controllen = sizeof(control_un.control);

  iov[0].iov_base = buf;
  iov[0].iov_len = 1;
  msg.msg_iov = iov;
  msg.msg_iovlen = 1;

  if (recvmsg(socket, &msg, 0) < 0) {
    return -1;
  }

  struct cmsghdr *cmptr = CMSG_FIRSTHDR(&msg);
  if (cmptr && cmptr->cmsg_len == CMSG_LEN(sizeof(int))) {
    if (cmptr->cmsg_level != SOL_SOCKET || cmptr->cmsg_type != SCM_RIGHTS) {
      return -1;
    }
    return *((int *)CMSG_DATA(cmptr));
  }

  return -1;
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

// Установка неблокирующего режима
int set_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0)
    return -1;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// Создание пары сокетов
int create_socket_pair(int pair[2]) {
  return socketpair(AF_UNIX, SOCK_STREAM, 0, pair);
}

// Найти свободный слот для клиента
int find_free_client_slot() {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].socket == 0) {
      return i;
    }
  }
  return -1;
}

// Обработка состояния UNKNOWN
void handle_unknown(int client_fd) {
  printf("Обработка UNKNOWN для клиента %d\n", client_fd);

  // Заглушка: просто отправляем приветствие
  char *welcome_msg = "Добро пожаловать! Введите ваше имя: ";
  send(client_fd, welcome_msg, strlen(welcome_msg), 0);

  // Читаем имя (упрощенно)
  char buffer[BUFFER_SIZE];
  ssize_t bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

  if (bytes > 0) {
    buffer[bytes] = '\0';
    printf("Клиент %d представился как: %s\n", client_fd, buffer);

    // Находим или создаем запись клиента
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

// Обработка состояния LOBBY (изменено: спрашиваем количество участников)
int handle_lobby(int client_fd) {
  printf("Обработка LOBBY для клиента %d\n", client_fd);

  // Теперь спрашиваем желаемое количество участников
  char *question = "Введите желаемое количество участников в комнате (от 1 до 10): ";
  send(client_fd, question, strlen(question), 0);

  // Читаем ответ
  char buffer[BUFFER_SIZE];
  ssize_t bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

  if (bytes > 0) {
    buffer[bytes] = '\0';
    int desired_participants = atoi(buffer);

    // Проверяем валидность ввода
    if (desired_participants >= 1 && desired_participants <= 10) {
      printf("Клиент %d хочет комнату с %d участниками\n", 
             client_fd, desired_participants);
      return desired_participants;
    } else {
      // Если ввод некорректен, используем значение по умолчанию
      send(client_fd, "Некорректный ввод. Используем значение по умолчанию (3).\n", 
           60, 0);
      return 3;
    }
  }

  // По умолчанию 3 участника
  return 3;
}

// Найти процесс комнаты по ID
RoomProcess *find_room_process(int room_id) {
  for (int i = 0; i < MAX_ROOMS; i++) {
    if (rooms[i].pid > 0 && rooms[i].room_id == room_id) {
      return &rooms[i];
    }
  }
  return NULL;
}

// Найти комнату с заданным количеством участников
RoomProcess *find_room_by_participants(int desired_participants) {
  for (int i = 0; i < MAX_ROOMS; i++) {
    if (rooms[i].pid > 0 && 
        rooms[i].max_participants == desired_participants &&
        rooms[i].client_count < desired_participants) {
      // Комната с нужным лимитом участников и есть свободные места
      return &rooms[i];
    }
  }
  return NULL;
}

// Создать новый процесс комнаты с указанным лимитом участников
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
    close(socket_pair[0]); // закрываем родительский конец

    // Запускаем логику комнаты с указанием лимита участников
    room_process(room_id, socket_pair[1], max_participants);

    // room_process не должен возвращаться
    exit(0);
  }

  // Родительский процесс
  close(socket_pair[1]); // закрываем комнатный конец

  // Находим свободный слот для комнаты
  for (int i = 0; i < MAX_ROOMS; i++) {
    if (rooms[i].pid == 0) {
      rooms[i].pid = pid;
      rooms[i].control_fd = socket_pair[0];
      rooms[i].room_id = room_id;
      rooms[i].client_count = 0;
      rooms[i].max_participants = max_participants;

      printf("Создана комната %d (PID=%d) с лимитом %d участников\n", 
             room_id, pid, max_participants);
      return i;
    }
  }

  return -1;
}

// Передать клиента в комнату с указанным лимитом участников
void transfer_to_room(int client_fd, int desired_participants) {
  printf("Поиск комнаты для клиента %d (желает %d участников)\n", 
         client_fd, desired_participants);

  // Ищем существующую комнату с таким лимитом участников
  RoomProcess *room = find_room_by_participants(desired_participants);
  
  if (!room) {
    // Если нет подходящей комнаты, создаем новую
    // Генерируем ID комнаты (просто увеличиваем на 1 от предыдущей)
    static int next_room_id = 1;
    int room_id = next_room_id++;
    
    printf("Комната с лимитом %d не найдена, создаем новую комнату %d\n", 
           desired_participants, room_id);
    
    int room_index = create_room_process(room_id, desired_participants);
    if (room_index < 0) {
      printf("Не удалось создать комнату\n");
      close(client_fd);
      return;
    }
    room = &rooms[room_index];
  }

  // Отправляем команду "добавить клиента"
  char cmd[] = "ADD_CLIENT";
  send(room->control_fd, cmd, strlen(cmd), 0);

  // Передаем файловый дескриптор клиента
  if (send_fd(room->control_fd, client_fd) < 0) {
    perror("send_fd");
    close(client_fd);
    return;
  }

  room->client_count++;
  printf("Клиент %d передан в комнату %d (клиентов: %d/%d)\n", 
         client_fd, room->room_id, room->client_count, room->max_participants);

  // Удаляем клиента из списка родителя
  remove_client(client_fd);
}

// Удалить клиента из списка родителя
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

// Процесс комнаты (добавлена логика ограничения участников и спама)
void room_process(int room_id, int parent_fd, int max_participants) {
  printf("[Комната %d PID=%d] Запущена. Максимум участников: %d\n", 
         room_id, getpid(), max_participants);

  // Создаем epoll для мониторинга клиентов
  int epoll_fd = epoll_create1(0);
  if (epoll_fd < 0) {
    perror("epoll_create1");
    exit(1);
  }

  // Добавляем родительский сокет в epoll
  struct epoll_event ev;
  ev.events = EPOLLIN;
  ev.data.fd = parent_fd;
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, parent_fd, &ev) < 0) {
    perror("epoll_ctl parent");
    exit(1);
  }

  // Массив клиентов комнаты
  int client_fds[100] = {0};
  int client_count = 0;
  int spam_sent = 0;  // Флаг: отправляли ли уже спам при достижении лимита

  struct epoll_event events[10];

  while (1) {
    int n = epoll_wait(epoll_fd, events, 10, 3000);
    if (n < 0) {
      perror("epoll_wait");
      continue;
    }

    for (int i = 0; i < n; i++) {
      int fd = events[i].data.fd;

      if (fd == parent_fd) {
        // Сообщение от родителя
        char buffer[BUFFER_SIZE];
        ssize_t bytes = recv(parent_fd, buffer, sizeof(buffer) - 1, 0);

        if (bytes <= 0) {
          printf("[Комната %d] Родитель отключился\n", room_id);
          exit(0);
        }

        buffer[bytes] = '\0';

        if (strcmp(buffer, "ADD_CLIENT") == 0) {
          // Проверяем, не превышен ли лимит участников
          if (client_count >= max_participants) {
            printf("[Комната %d] Лимит участников достигнут (%d/%d). Отказываем в подключении.\n",
                   room_id, client_count, max_participants);
            
            // Получаем дескриптор, но сразу закрываем его
            int new_client_fd = receive_fd(parent_fd);
            if (new_client_fd >= 0) {
              char *reject_msg = "Комната заполнена. Невозможно подключиться.\n";
              send(new_client_fd, reject_msg, strlen(reject_msg), 0);
              close(new_client_fd);
            }
            continue;
          }

          // Получаем дескриптор клиента
          int new_client_fd = receive_fd(parent_fd);
          if (new_client_fd < 0) {
            printf("[Комната %d] Ошибка получения дескриптора\n", room_id);
            continue;
          }

          // Добавляем клиента в epoll
          struct epoll_event client_ev;
          client_ev.events = EPOLLIN;
          client_ev.data.fd = new_client_fd;

          if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_client_fd, &client_ev) < 0) {
            perror("epoll_ctl client");
            close(new_client_fd);
            continue;
          }

          client_fds[client_count++] = new_client_fd;
          printf("[Комната %d] Добавлен клиент fd=%d (всего: %d/%d)\n", 
                 room_id, new_client_fd, client_count, max_participants);

          // Приветствуем клиента в комнате
          char welcome_msg[100];
          snprintf(welcome_msg, sizeof(welcome_msg),
                   "Добро пожаловать в комнату %d! Участников: %d/%d\n",
                   room_id, client_count, max_participants);
          send(new_client_fd, welcome_msg, strlen(welcome_msg), 0);

          // Если достигнут лимит участников, отправляем спам всем клиентам
          if (client_count == max_participants && !spam_sent) {
            printf("[Комната %d] Достигнут лимит участников! Отправляем спам всем клиентам.\n", 
                   room_id);
            
            for (int j = 0; j < client_count; j++) {
              char *limit_msg = "\n=== ЛИМИТ УЧАСТНИКОВ ДОСТИГНУТ! ===\n";
              send(client_fds[j], limit_msg, strlen(limit_msg), 0);
              send_spam_to_client(client_fds[j]);
            }
            spam_sent = 1;  // Помечаем, что спам уже отправлен
          }
        }
      } else {
        // Сообщение от клиента комнаты
        char buffer[BUFFER_SIZE];
        ssize_t bytes = recv(fd, buffer, sizeof(buffer) - 1, 0);

        if (bytes <= 0) {
          // Клиент отключился
          printf("[Комната %d] Клиент fd=%d отключился\n", room_id, fd);

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

          // Сбрасываем флаг спама, если количество участников упало ниже лимита
          if (client_count < max_participants) {
            spam_sent = 0;
          }

          close(fd);
        } else {
          // Обрабатываем сообщение
          buffer[bytes] = '\0';
          printf("[Комната %d] Сообщение от fd=%d: %s\n", room_id, fd, buffer);

          // Эхо-ответ с информацией о комнате
          char response[BUFFER_SIZE + 100];
          snprintf(response, sizeof(response), 
                  "[Комната %d, участников: %d/%d] Ваше сообщение: %s",
                  room_id, client_count, max_participants, buffer);
          send(fd, response, strlen(response), 0);
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
    // Принимаем новое подключение
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

    // Обрабатываем клиента
    handle_unknown(client_fd);

    // Переводим в лобби и определяем желаемое количество участников
    int desired_participants = handle_lobby(client_fd);

    // Передаем в подходящую комнату
    transfer_to_room(client_fd, desired_participants);
  }

  close(server_fd);
  return 0;
}

int main() {
  // Игнорируем SIGPIPE (чтобы не падать при закрытых сокетах)
  signal(SIGPIPE, SIG_IGN);

  // Игнорируем SIGCHLD (зомби-процессы будут автоматически убираться)
  signal(SIGCHLD, SIG_IGN);

  printf("Запуск TCP сервера с процессами...\n");
  printf("Порт: %d\n", SERVER_PORT);
  printf("Максимум комнат: %d\n", MAX_ROOMS);
  printf("Максимум клиентов: %d\n", MAX_CLIENTS);
  printf("Клиенты теперь выбирают желаемое количество участников в комнате!\n");

  return run_server();
}
