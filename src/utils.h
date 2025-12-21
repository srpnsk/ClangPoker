#include <poll.h>
#define MAX_CLIENTS 100
#define MAX_PARTICIPANTS 7
#define MAX_ROOMS 10
typedef struct {
  char username[16];
  int room_ind;
  int fd;
} client;

typedef struct {
  int max_participants;
  int current_participants;
} room;

extern client all_clients[MAX_CLIENTS];
extern room all_rooms[MAX_ROOMS];
extern struct pollfd pfds[MAX_CLIENTS + 1];
extern int nfds;

int create_room(int max_participants);
client *find_client_by_fd(int fd);
char *get_username(int fd);
int get_room(int fd);
int set_room(client *cl, int room);
int add_client(int fd);
void remove_client(int fd);
int get_clients_in_room(int room, int *result_fds);
