#include "common.h"
#include "utils.h"
#include <sys/epoll.h>

// Процесс комнаты
void room_process(int room_id, int parent_fd) {
    printf("[Комната %d PID=%d] Запущена\n", room_id, getpid());
    
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
                // Сообщение от родителя
                char buffer[BUFFER_SIZE];
                ssize_t bytes = recv(parent_fd, buffer, sizeof(buffer) - 1, 0);
                
                if (bytes <= 0) {
                    printf("[Комната %d] Родитель отключился\n", room_id);
                    exit(0);
                }
                
                buffer[bytes] = '\0';
                
                if (strcmp(buffer, "ADD_CLIENT") == 0) {
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
                    printf("[Комната %d] Добавлен клиент fd=%d (всего: %d)\n", 
                           room_id, new_client_fd, client_count);
                    
                    // Приветствуем клиента в комнате
                    char* welcome = "Добро пожаловать в комнату!";
                    send(new_client_fd, welcome, strlen(welcome), 0);
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
                    
                    close(fd);
                } else {
                    // Обрабатываем сообщение (заглушка)
                    buffer[bytes] = '\0';
                    printf("[Комната %d] Сообщение от fd=%d: %s\n", room_id, fd, buffer);
                    
                    // Эхо-ответ
                    char response[BUFFER_SIZE + 50];
                    snprintf(response, sizeof(response), 
                            "Комната %d получила: %s", room_id, buffer);
                    send(fd, response, strlen(response), 0);
                }
            }
        }
    }
}
