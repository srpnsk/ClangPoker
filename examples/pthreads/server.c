#include "server.h"

// Глобальные переменные
RoomProcess rooms[MAX_ROOMS] = {0};
Client clients[MAX_CLIENTS] = {0};
int client_count = 0;

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
    char* welcome_msg = "Добро пожаловать! Введите ваше имя: ";
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
            strncpy(clients[slot].username, buffer, sizeof(clients[slot].username) - 1);
            client_count++;
        }
    }
}

// Обработка состояния LOBBY (заглушка)
int handle_lobby(int client_fd) {
    printf("Обработка LOBBY для клиента %d\n", client_fd);
    
    // Заглушка: отправляем вопрос и получаем ответ
    char* question = "Выберите комнату (1, 2 или 3): ";
    send(client_fd, question, strlen(question), 0);
    
    // Читаем ответ
    char buffer[BUFFER_SIZE];
    ssize_t bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes > 0) {
        buffer[bytes] = '\0';
        int room_choice = atoi(buffer);
        
        // Проверяем валидность выбора
        if (room_choice >= 1 && room_choice <= 3) {
            printf("Клиент %d выбрал комнату %d\n", client_fd, room_choice);
            return room_choice;
        }
    }
    
    // По умолчанию комната 1
    return 1;
}

// Найти процесс комнаты
RoomProcess* find_room_process(int room_id) {
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (rooms[i].pid > 0 && rooms[i].room_id == room_id) {
            return &rooms[i];
        }
    }
    return NULL;
}

// Создать новый процесс комнаты
int create_room_process(int room_id) {
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
        
        // Запускаем логику комнаты
        room_process(room_id, socket_pair[1]);
        
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
            
            printf("Создана комната %d (PID=%d)\n", room_id, pid);
            return i;
        }
    }
    
    return -1;
}

// Передать клиента в комнату
void transfer_to_room(int client_fd, int room_id) {
    printf("Передача клиента %d в комнату %d\n", client_fd, room_id);
    
    // Находим или создаем процесс комнаты
    RoomProcess* room = find_room_process(room_id);
    if (!room) {
        printf("Комната %d не найдена, создаем новую\n", room_id);
        int room_index = create_room_process(room_id);
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
    printf("Клиент %d передан в комнату %d (всего клиентов: %d)\n", 
           client_fd, room_id, room->client_count);
    
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
        
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }
        
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        printf("Новое подключение: %s:%d (fd=%d)\n", 
               client_ip, ntohs(client_addr.sin_port), client_fd);
        
        // Обрабатываем клиента
        handle_unknown(client_fd);
        
        // Переводим в лобби и определяем комнату
        int room_id = handle_lobby(client_fd);
        
        // Передаем в комнату
        transfer_to_room(client_fd, room_id);
    }
    
    close(server_fd);
    return 0;
}
