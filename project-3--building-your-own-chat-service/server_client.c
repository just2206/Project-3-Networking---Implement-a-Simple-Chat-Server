#include "server.h"
#include "list.h" // For access to user_t, room_t, etc.

// USE THESE LOCKS AND COUNTER TO SYNCHRONIZE
extern int numReaders;
extern pthread_mutex_t rw_lock;
extern pthread_mutex_t mutex;

// Synchronization wrappers (define them here or include from server.c)
void start_read() { pthread_mutex_lock(&mutex); numReaders++; if (numReaders == 1) { pthread_mutex_lock(&rw_lock); } pthread_mutex_unlock(&mutex); }
void end_read() { pthread_mutex_lock(&mutex); numReaders--; if (numReaders == 0) { pthread_mutex_unlock(&rw_lock); } pthread_mutex_unlock(&mutex); }
void start_write() { pthread_mutex_lock(&rw_lock); }
void end_write() { pthread_mutex_unlock(&rw_lock); }


// Global list pointers
extern user_t *global_users_head;
extern room_t *global_rooms_head;


// server_client.c (inside client_receive)

void *client_receive(void *ptr) {
   // CRITICAL FIX: Capture the socket and free the pointer allocated in main.c
   int client = *(int *) ptr;
   free(ptr); 
   
   int received, i;
   char buffer[MAXBUFF], sbuffer[MAXBUFF];  
   char tmpbuf[MAXBUFF];
   char cmd_response[MAXBUFF];
   char *arguments[80];
   user_t *currentUser;

   // 1. Initial Guest Setup
   char guest_username[30];
   sprintf(guest_username,"guest%d", client);
   
   start_write(); 
   global_users_head = insertUser(global_users_head, client , guest_username);
   currentUser = findUserBySocket(client); // Get the actual node pointer
   
   room_t *lobby = findRoom(DEFAULT_ROOM); // Find Lobby (must exist from main)
   if (lobby) {
       room_add_user(lobby, currentUser); 
   }
   end_write();
    
   // Send Welcome Message and prompt
   send(client  , server_MOTD , strlen(server_MOTD) , 0 ); 

   while (1) {
      // ... (command parsing and tokenization logic) ...
      
      // Get current user (READ lock required for every loop iteration, but we rely on the 
      // fact that currentUser's socket descriptor is constant and we retrieve the pointer here.)
      start_read();
      currentUser = findUserBySocket(client);
      end_read();
      
      // Handle the commands (login, create, join, etc.) using the new functions...
      // ...
      
      else if (strcmp(arguments[0], "users") == 0) {
          start_read(); 
          getUserListString(cmd_response);
          end_read();
          strcat(cmd_response, "\nchat>");
      }                           
      else if (strcmp(arguments[0], "rooms") == 0) {
          start_read(); 
          getRoomListString(cmd_response);
          end_read();
          strcat(cmd_response, "\nchat>");
      }
      else if (strcmp(arguments[0], "exit") == 0 || strcmp(arguments[0], "logout") == 0) {
          send(client, "\nLogging out and closing connection...\n", 37, 0); 
          
          start_write();
          // Call helper to clean up all membership/DM links
          global_users_head = removeUserBySocket(global_users_head, client); 
          end_write();
          
          close(client);
          free(command_copy);
          return NULL; 
      }                         
      else { 
           // 3. Selective Sending Logic
           
           // Array to store unique sockets of recipients
           int recipients[max_clients]; 
           memset(recipients, 0, sizeof(recipients));
           int num_recipients = 0;
           
           // Helper functions (defined locally or globally)
           #define is_recipient(sock) ({ bool found = false; for (int j = 0; j < num_recipients; j++) { if (recipients[j] == sock) { found = true; break; } } found; })
           #define add_recipient(sock) ({ if (sock != client && !is_recipient(sock) && num_recipients < max_clients) { recipients[num_recipients++] = sock; } })

           sprintf(tmpbuf, "\n::%s> %s\nchat>", currentUser->username, sbuffer);

           // Selective Send Logic (READ lock required for iterating all lists)
           start_read();
           
           // A. Add users from DM list
           member_t *dm = currentUser->dm_connections;
           while (dm) {
               add_recipient(dm->user_ptr->socket);
               dm = dm->next;
           }

           // B. Find all rooms the currentUser is in and add all members of those rooms
           room_t *current_room = global_rooms_head;
           while (current_room) {
               if (is_user_in_room(current_room, currentUser)) {
                   // If the current user is in the room, add ALL members of that room
                   member_t *member = current_room->members; 
                   while (member) {
                       add_recipient(member->user_ptr->socket);
                       member = member->next;
                   }
               }
               current_room = current_room->next;
           }

           // C. Send message to all unique, verified recipients
           for (int j = 0; j < num_recipients; j++) {
               send(recipients[j], tmpbuf, strlen(tmpbuf), 0); 
           }

           end_read();
           
           sprintf(cmd_response, "\nchat>");
      }
      
      // Send command response back to the initiating client
      send(client, cmd_response, strlen(cmd_response), 0);
      free(command_copy);
   }
   return NULL;
}
