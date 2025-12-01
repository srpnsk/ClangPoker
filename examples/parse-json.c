#include <cjson/cJSON.h>
#include <stdio.h>
#include <string.h>

void handle_json_message(const char *json_string) {
  cJSON *root = cJSON_Parse(json_string);
  if (root == NULL) {
    // Ошибка парсинга: возможно, сообщение было повреждено
    const char *error_ptr = cJSON_GetErrorPtr();
    if (error_ptr != NULL) {
      fprintf(stderr, "Error before: %s\n", error_ptr);
    }
    return;
  }

  cJSON *type_obj = cJSON_GetObjectItemCaseSensitive(root, "type");

  if (cJSON_IsString(type_obj) && (type_obj->valuestring != NULL)) {
    if (strcmp(type_obj->valuestring, "GAME_STATE") == 0) {
      // Обработка полного состояния игры
      // Извлечение: cJSON *hand = cJSON_GetObjectItemCaseSensitive(root,
      // "my_hand"); И т.д.
      // ... Call update_ascii_interface(...)
    } else if (strcmp(type_obj->valuestring, "ERROR") == 0) {
      // Обработка ошибок
      cJSON *msg = cJSON_GetObjectItemCaseSensitive(root, "message");
      if (cJSON_IsString(msg)) {
        printf("\n[SERVER ERROR] %s\n", msg->valuestring);
      }
    }
    // Добавьте обработку других типов сообщений: WINNER, TIMEOUT и т.д.
  }

  cJSON_Delete(root); // Освобождаем память!
}
