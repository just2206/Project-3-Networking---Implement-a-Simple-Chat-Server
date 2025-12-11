#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h> // For close()

// Forward declarations
struct user_t;
struct room_t;

/////////////////// DM/ROOM MEMBERSHIP NODES //////////////////////////
// Generic node for linking users in rooms or DMs
typedef struct member_t {
    struct user_t *user_ptr; // Pointer to the actual user node in the global list
    struct member_t *next;
} member_t;

/////////////////// ROOM LIST //////////////////////////
typedef struct room_t {
    char room_name[30];
    member_t *members;   // Head of the list of users in this room
    struct room_t *next; // Next room in the global room list
} room_t;


/////////////////// USER LIST (The global 'head') //////////////////////////
typedef struct user_t {
   char username[30];
   int socket;
   member_t *dm_connections; // List of other users this user is directly connected to
   struct user_t *next;
} user_t;

// Global pointers for the shared data structures (defined in server.c/list.c)
extern user_t *global_users_head;
extern room_t *global_rooms_head;

/////////////////// GLOBAL USER LIST OPERATIONS //////////////////////////
user_t* findUserBySocket(int socket);
user_t* findUserByUsername(char* username);
user_t* insertUser(user_t *head, int socket, char *username);
user_t* removeUserBySocket(user_t *head, int socket);

/////////////////// ROOM & DM OPERATIONS //////////////////////////
room_t* findRoom(char *room_name);
room_t* addRoom(char *room_name);
bool room_add_user(room_t *room, user_t *user);
bool room_remove_user(room_t *room, user_t *user);
bool dm_add_user(user_t *u1, user_t *u2);
bool dm_remove_user(user_t *u1, user_t *u2);
bool is_user_in_room(room_t *room, user_t *user);

/////////////////// UTILITY & CLEANUP //////////////////////////
void cleanup_user_resources(user_t *user);
void free_all_data_structures();
char* getUserListString(char *buffer);
char* getRoomListString(char *buffer);