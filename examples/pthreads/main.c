#include "server.h"

int main() {
  // Игнорируем SIGPIPE (чтобы не падать при закрытых сокетах)
  signal(SIGPIPE, SIG_IGN);

  // Игнорируем SIGCHLD (зомби-процессы будут автоматически убираться)
  signal(SIGCHLD, SIG_IGN);

  printf("Запуск TCP сервера с процессами...\n");
  printf("Порт: %d\n", SERVER_PORT);
  printf("Максимум комнат: %d\n", MAX_ROOMS);
  printf("Максимум клиентов: %d\n", MAX_CLIENTS);

  return run_server();
}
