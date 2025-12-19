#ifndef CLIENT_UI_H
#define CLIENT_UI_H

#include <stdbool.h>

// Инициализация UI (после подключения к серверу)
bool client_ui_init(int socket_fd);

// Основной цикл обработки UI
void client_ui_run(void);

// Очистка ресурсов UI
void client_ui_cleanup(void);

// Функция для отображения системных сообщений
void ui_display_system_message(const char *message);

// Функция для отображения чат сообщений
void ui_display_chat_message(const char *username, const char *message);

// Функция для отображения сообщений об ошибках
void ui_display_error(const char *error);

// Получение ввода от пользователя (публичная функция для callback)
char *ui_get_input_buffer(void);
int ui_get_input_mode(void);
void ui_set_input_mode(int mode);
void ui_clear_input_buffer(void);

#endif
