#include "utils.h"
#include <fcntl.h>
#include <sys/un.h>
// а, ну наверное типа родитель отправляет ребенку дескриптор
// Отправка дескриптора через сокет
int send_fd(int socket, int fd_to_send) { // родитель этим пользуется
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
// крч, дескриптор -- это такая параша, через которую ты передаешь все данные
// Прием дескриптора
int receive_fd(int socket) { // а этим -- ребенок
  // аааааа
  // я поняла задумку
  // крч, я говорила дикпику, что надо делать так, чтобы родитель не слушал
  // клиентов из комнаты и чтобы их слушал только ребенок
  //
  // а, ну смари
  // клиент выходит -- ребенок обрабатывает это, выкидывает и передает родителю
  //
  // !!! некорректно называть родителя сервером: они оба сервера, просто один
  // старший, другой -- младший)))
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
