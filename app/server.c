#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/select.h>
#include <ctype.h>
#include "message.h"

int main() {
	// Disable output buffering
	setbuf(stdout, NULL);

	// You can use print statements as follows for debugging, they'll be visible when running tests.
	printf("Logs from your program will appear here!\n");

	// Uncomment this block to pass the first stage

	 int server_fd, client_addr_len;
	 struct sockaddr_in client_addr;

	 server_fd = socket(AF_INET, SOCK_STREAM, 0);
	 if (server_fd == -1) {
	 	printf("Socket creation failed: %s...\n", strerror(errno));
	 	return 1;
	 }

	 // Since the tester restarts your program quite often, setting REUSE_PORT
	 // ensures that we don't run into 'Address already in use' errors
	 int reuse = 1;
	 if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0) {
	 	printf("SO_REUSEPORT failed: %s \n", strerror(errno));
	 	return 1;
	 }

	 struct sockaddr_in serv_addr = { .sin_family = AF_INET ,
	 								 .sin_port = htons(6379),
	 								 .sin_addr = { htonl(INADDR_ANY) },
	 								};

	 if (bind(server_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
	 	printf("Bind failed: %s \n", strerror(errno));
	 	return 1;
	 }

	 int connection_backlog = 5;
	 if (listen(server_fd, connection_backlog) != 0) {
	 	printf("Listen failed: %s \n", strerror(errno));
	 	return 1;
	 }

	 printf("Waiting for a client to connect...\n");
     char* pingMessage = "+PONG\r\n";

	 client_addr_len = sizeof(client_addr);

	 fd_set master, current;
	 FD_SET(server_fd, &master);
	 int max_socket_fd = server_fd+1;
     int client_fd;
	 char buffer[128];
	 int nbytes, sentBytes;
	 char ** commands;
	 int commandLen;

	 char* echo = "echo";
	 char* ping = "ping";

	 for(;;){
		 current = master;
		 if(select(max_socket_fd,&current,NULL,NULL,NULL)==-1){
			 perror("select\n");
		 }
		 for(int i=0; i<=max_socket_fd; i++){
			 if(FD_ISSET(i, &current)){
				 if(i==server_fd){
					 client_fd = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);
					 if(client_fd==-1){
						 perror("accept\n");
					 }
					 FD_SET(client_fd, &master);
					 if(client_fd>=max_socket_fd){
						 max_socket_fd = client_fd+1;
					 }
					 printf("Client connected at socket %d\n", client_fd);
				 }
			 	else{
					 nbytes = recv(i, buffer, sizeof buffer, 0);
					 if(nbytes<=0){
						 if(nbytes==0){
							 printf("Connection closed at socket %d\n", i);
						 }
						 else{
							 perror("receive\n");
						 }
						 close(i); // bye!
						 FD_CLR(i, &master);
						 break;
					 }
					 deCodeRedisMessage(buffer, nbytes, &commands, &commandLen);
					 if(commandLen>0){
						 toLower(commands[0]);
						 if(strcmp(ping, commands[0])==0){
							 if( (sentBytes = send(i, pingMessage, strlen( pingMessage ), 0))==-1){
								 perror("send\n");
							 }
						 }
						 else if(strcmp(echo, commands[0])==0 && commandLen>1){
							 int msgLen = strlen(commands[1]);
							 char* writeBuffer = (char*)malloc(msgLen+3);
							 *writeBuffer = '+';
							 strcpy(writeBuffer+1,commands[1]);
							 writeBuffer[msgLen+1]='\r';
							 writeBuffer[msgLen+2]='\n';
							 if( (sentBytes = send(i, writeBuffer, msgLen+3, 0))==-1){
								 perror("send\n");
							 }
						 }
					 }
				 }
			 }
		 }
     }
	 close(server_fd);

	return 0;
}
