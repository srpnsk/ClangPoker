// util.c
#include <stdio.h>
#include <sys/socket.h>

// Отправка дескриптора через сокет
void send_fd(int socket, int fd_to_send) {
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

  sendmsg(socket, &msg, 0);
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

  ssize_t n = recvmsg(socket, &msg, 0);
  if (n < 0) {
    perror("recvmsg");
    // обработка ошибки
  }

  // Обработка данных
  if (n > 0) {
    buf[n] = '\0'; // если это строка
    // используем данные из buffer
    printf("Получено %zd байт: %s\n", n, buf);
  }

  struct cmsghdr *cmptr = CMSG_FIRSTHDR(&msg);
  int received_fd;
  received_fd = *((int *)CMSG_DATA(cmptr));

  return received_fd;
}
