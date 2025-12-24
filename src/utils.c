#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// client all_clients[MAX_CLIENTS] = {[0 ... MAX_CLIENTS - 1] = {"", -1, -1}};
// room all_rooms[MAX_ROOMS] = {[0 ... MAX_ROOMS - 1] = {-1, 0}};

int create_room(int max_participants) {
  if ((max_participants <= 1) || (max_participants > MAX_PARTICIPANTS))
    return -1;
  for (int i = 0; i < MAX_ROOMS; i++) {
    if (all_rooms[i].max_participants == -1) {
      all_rooms[i].max_participants = max_participants;
      all_rooms[i].current_participants = 0;
      printf("created a new room with %d capacity\n", max_participants);
      return i;
    }
  }
  return -1;
}

static void remove_room(int room_ind) {
  /* должна быть гарантия того, что НИКАКОЙ КЛИЕНТ не принадлежит комнате */
  if ((room_ind < 0) || (room_ind >= MAX_ROOMS))
    return;
  all_rooms[room_ind].current_participants = 0;
  printf("removed room %d with capacity %d\n", room_ind,
         all_rooms[room_ind].max_participants);
  all_rooms[room_ind].max_participants = -1;
}

client *find_client_by_fd(int fd) {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (all_clients[i].fd == fd) {
      return &all_clients[i];
    }
  }
  return NULL;
}

int set_room(client *cl, int room) {
  if (!cl)
    return -1;
  if (room < 0) {
    if (cl->room_ind != -1) {
      int backup = cl->room_ind;
      all_rooms[backup].current_participants--;
      cl->room_ind = -1;
      printf("client %d moved from room %d to lobby\n", cl->fd, backup);
      if (all_rooms[backup].current_participants == 0) {
        remove_room(backup);
      }
    }
    return 0;
  } else if ((room >= 0) && (room < MAX_ROOMS) &&
             (all_rooms[room].current_participants <
              all_rooms[room].max_participants)) {
    cl->room_ind = room;
    all_rooms[room].current_participants++;
    printf("client %d now in room %d\n", cl->fd, room);
    return 0;
  }
  return -1;
}

static int find_free_slot(void) {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (all_clients[i].fd == -1) {
      return i;
    }
  }
  printf("no more free client slots!\n");
  return -1;
}

int add_client(int fd) {
  if (find_client_by_fd(fd))
    return -1;
  int slot = find_free_slot();
  if (slot == -1)
    return -1;
  all_clients[slot].fd = fd;
  all_clients[slot].room_ind = -1;
  all_clients[slot].username[0] = '\0';
  pfds[nfds].fd = fd;
  pfds[nfds].events = POLLIN;
  nfds++;
  printf("created client %d\n", fd);
  return slot;
}

void remove_client(int fd) {
  client *cl = find_client_by_fd(fd);
  if (cl != NULL) {
    cl->fd = -1;
    memset(cl->username, 0, 16);
    if (cl->room_ind != -1) {
      int backup = cl->room_ind;
      all_rooms[backup].current_participants--;
      cl->room_ind = -1;
      if (all_rooms[backup].current_participants == 0) {
        remove_room(backup);
      }
    }
    for (int i = 1; i < nfds; i++) {
      if (pfds[i].fd == fd) {
        pfds[i] = pfds[nfds - 1];
        pfds[nfds - 1].fd = -1;
        pfds[nfds - 1].events = 0;
        pfds[nfds - 1].revents = 0;
        nfds--;
        break;
      }
    }
    close(fd);
    printf("successfully removed client and closed their fd %d\n", fd);
  }
}

int get_clients_in_room(int room, int *result_fds) {
  int count = 0;
  if (!((room >= 0) && (room < MAX_ROOMS)))
    return -1;
  for (int i = 0;
       (i < MAX_CLIENTS) && (count < all_rooms[room].max_participants); i++) {
    if (all_clients[i].fd != -1 && all_clients[i].room_ind == room) {
      if (result_fds != NULL) {
        result_fds[count] = all_clients[i].fd;
      }
      count++;
    }
  }
  printf("there are %d clients in room %d\n", count, room);
  return count;
}
