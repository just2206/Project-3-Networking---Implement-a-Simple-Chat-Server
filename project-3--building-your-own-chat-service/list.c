#include "list.h"

user_t *global_users_head = NULL;
room_t *global_rooms_head = NULL;

// Helper function to allocate a membership node
member_t *member_alloc(user_t *user) {
    member_t *new_member = (member_t*)malloc(sizeof(member_t));
    new_member->user_ptr = user;
    new_member->next = NULL;
    return new_member;
}

// ---------------- USER LIST MANIPULATION ----------------

user_t* insertUser(user_t *head, int socket, char *username) {
    user_t *newUser = (user_t*)malloc(sizeof(user_t));
    strncpy(newUser->username, username, 30);
    newUser->socket = socket;
    newUser->dm_connections = NULL;
    
    newUser->next = head;
    return newUser; // New head
}

user_t* findUserBySocket(int socket) {
    user_t *current = global_users_head;
    while(current) {
        if (current->socket == socket) return current;
        current = current->next;
    }
    return NULL;
}

user_t* findUserByUsername(char* username) {
    user_t *current = global_users_head;
    while(current) {
        if (strcmp(current->username, username) == 0) return current;
        current = current->next;
    }
    return NULL;
}

user_t* removeUserBySocket(user_t *head, int socket) {
    user_t *current = head;
    user_t *prev = NULL;
    
    while(current) {
        if (current->socket == socket) {
            if (prev == NULL) { // Removing the head
                head = current->next;
            } else {
                prev->next = current->next;
            }
            
            // Clean up resources related to this user before freeing the node
            cleanup_user_resources(current);
            free(current);
            return head;
        }
        prev = current;
        current = current->next;
    }
    return head;
}


// ---------------- ROOM MANIPULATION ----------------

room_t* findRoom(char *room_name) {
    room_t *current = global_rooms_head;
    while(current) {
        if (strcmp(current->room_name, room_name) == 0) return current;
        current = current->next;
    }
    return NULL;
}

room_t* addRoom(char *room_name) {
    if (findRoom(room_name)) return NULL; 

    room_t *newRoom = (room_t*)malloc(sizeof(room_t));
    strncpy(newRoom->room_name, room_name, 30);
    newRoom->members = NULL;
    
    newRoom->next = global_rooms_head;
    global_rooms_head = newRoom;
    
    return newRoom;
}

bool is_user_in_room(room_t *room, user_t *user) {
    member_t *current = room->members;
    while(current) {
        if (current->user_ptr == user) return true;
        current = current->next;
    }
    return false;
}

bool room_add_user(room_t *room, user_t *user) {
    if (is_user_in_room(room, user)) return false; 
    
    member_t *new_member = member_alloc(user);
    new_member->next = room->members;
    room->members = new_member;
    return true;
}

bool room_remove_user(room_t *room, user_t *user) {
    member_t *current = room->members;
    member_t *prev = NULL;

    while(current) {
        if (current->user_ptr == user) {
            if (prev == NULL) {
                room->members = current->next;
            } else {
                prev->next = current->next;
            }
            free(current);
            return true;
        }
        prev = current;
        current = current->next;
    }
    return false;
}

// ---------------- DM MANIPULATION ----------------

bool dm_add_user(user_t *u1, user_t *u2) {
    member_t *current = u1->dm_connections;
    while(current) {
        if (current->user_ptr == u2) return false; // Already connected
        current = current->next;
    }
    
    member_t *new_member = member_alloc(u2);
    new_member->next = u1->dm_connections;
    u1->dm_connections = new_member;
    return true;
}

bool dm_remove_user(user_t *u1, user_t *u2) {
    member_t *current = u1->dm_connections;
    member_t *prev = NULL;

    while(current) {
        if (current->user_ptr == u2) {
            if (prev == NULL) {
                u1->dm_connections = current->next;
            } else {
                prev->next = current->next;
            }
            free(current);
            return true;
        }
        prev = current;
        current = current->next;
    }
    return false;
}

// ---------------- CLEANUP & UTILITY ----------------

void cleanup_user_resources(user_t *user) {
    // 1. Remove user from all rooms
    room_t *current_room = global_rooms_head;
    while (current_room) {
        // Must remove user from room's member list (safe to call even if not present)
        room_remove_user(current_room, user); 
        current_room = current_room->next;
    }
    
    // 2. Remove user from all other users' DM lists
    user_t *other_user = global_users_head;
    while(other_user) {
        if (other_user != user) {
            dm_remove_user(other_user, user);
        }
        other_user = other_user->next;
    }
    
    // 3. Free user's own DM list
    member_t *dm_current = user->dm_connections;
    while(dm_current) {
        member_t *temp = dm_current;
        dm_current = dm_current->next;
        free(temp);
    }
    user->dm_connections = NULL;
    
    // Note: The user node itself is freed in removeUserBySocket/removeUserByUsername
}

void free_all_data_structures() {
    // 1. Free all rooms and their member lists
    room_t *current_room = global_rooms_head;
    while (current_room) {
        room_t *room_temp = current_room;
        member_t *member_current = current_room->members;
        
        while (member_current) {
            member_t *member_temp = member_current;
            member_current = member_current->next;
            free(member_temp);
        }
        current_room = current_room->next;
        free(room_temp);
    }
    global_rooms_head = NULL;
    
    // 2. Free all user nodes (DM lists were cleaned in cleanup_user_resources during sequential removal)
    user_t *current_user = global_users_head;
    while(current_user) {
        user_t *user_temp = current_user;
        current_user = current_user->next;
        // Need to ensure cleanup_user_resources is called on the users still in the list,
        // but for a full shutdown, we can just free the user and assume all other member_t 
        // nodes pointing to it are being freed in room/DM list cleanup above.
        // For simplicity during full shutdown:
        free(user_temp);
    }
    global_users_head = NULL;
}

char* getUserListString(char *buffer) {
    user_t *current = global_users_head;
    char *ptr = buffer;
    ptr += sprintf(ptr, "Connected Users: ");
    
    if (!current) {
        ptr += sprintf(ptr, "None.");
        return buffer;
    }
    
    while(current) {
        ptr += sprintf(ptr, "%s (Sock:%d)%s", current->username, current->socket, current->next ? ", " : "");
        current = current->next;
    }
    return buffer;
}

char* getRoomListString(char *buffer) {
    room_t *current = global_rooms_head;
    char *ptr = buffer;
    ptr += sprintf(ptr, "Available Rooms: ");
    
    if (!current) {
        ptr += sprintf(ptr, "None.");
        return buffer;
    }
    
    while(current) {
        ptr += sprintf(ptr, "%s%s", current->room_name, current->next ? ", " : "");
        current = current->next;
    }
    return buffer;
}
