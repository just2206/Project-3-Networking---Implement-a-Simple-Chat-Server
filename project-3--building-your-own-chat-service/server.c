#include "server.h" 
// ... (global vars, locks, etc. as defined in your provided server.c) ...

// Define the global lists (must be initialized to NULL)
user_t *global_users_head = NULL; 
room_t *global_rooms_head = NULL;

// ... (get_server_socket, start_server, accept_client functions) ...

int main(int argc, char **argv) {

   signal(SIGINT, sigintHandler);
    
   //////////////////////////////////////////////////////
   // create the default room for all clients to join when 
   // initially connecting
   //  
   start_write(); // Writer lock for global_rooms_head modification
   addRoom(DEFAULT_ROOM); // Assuming "Lobby"
   end_write();
   // The addRoom function internally updates global_rooms_head
   //////////////////////////////////////////////////////

   // Open server socket
   chat_serv_sock_fd = get_server_socket();

   // step 3: get ready to accept connections
   if(start_server(chat_serv_sock_fd, BACKLOG) == -1) {
      printf("start server error\n");
      exit(1);
   }
   
   printf("Server Launched! Listening on PORT: %d\n", PORT);
    
   //Main execution loop
   while(1) {
      //Accept a connection, start a thread
      int new_client_fd = accept_client(chat_serv_sock_fd);
      if(new_client_fd != -1) {
         // CRITICAL FIX: Allocate memory for the socket descriptor to pass to the thread
         int *client_sock_ptr = (int *)malloc(sizeof(int));
         *client_sock_ptr = new_client_fd;
         
         pthread_t new_client_thread;
         // Pass the allocated pointer to the thread function
         pthread_create(&new_client_thread, NULL, client_receive, (void*)client_sock_ptr);
      }
   }
   
   return 0; // Should never be reached
}

/* Handle SIGINT (CTRL+C) */
void sigintHandler(int sig_num) {
   printf("\nServer received SIGINT. Shutting down gracefully...\n");

   // 1. Notify and close all client sockets 
   start_read();
   user_t *current_user = global_users_head;
   while(current_user != NULL) {
       send(current_user->socket, "\nServer is shutting down. Goodbye!\n", 35, 0);
       close(current_user->socket);
       current_user = current_user->next;
   }
   end_read();
   
   printf("Closed all client connections.\n");
   
   // 2. Free all data structures (WRITE lock required)
   start_write();
   free_all_data_structures(); 
   end_write();
   
   printf("Freed all memory resources.\n");

   // 3. Close server socket and exit
   close(chat_serv_sock_fd);
   exit(0);
}