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
#include <stddef.h>
#include "message.h"
#include "hashset.h"

static struct {
	struct HMap db;
} g_data;

void do_set (char **commands, int commandLen){
	if(commandLen<3)
		return;
	struct Entry keyEntry;
	char* key = commands[1], *value = commands[2];
	keyEntry.node.hcode = hash(key);
	keyEntry.key=key;
	struct HNode* node = hm_lookup(&g_data.db, &keyEntry.node, entry_eq);
	if(node){
		struct Entry * existing = container_of(node, struct Entry, node);
		free(existing->value);
		existing->value = malloc(strlen(value)+1);
		strcpy(existing->value, value);
		if(commandLen==5){
			long ms = strtol(commands[4],NULL, 10);
			gettimeofday(&existing->expiry,NULL);
			existing->expiry.tv_sec += ms/1000;  existing->expiry.tv_usec += ms*1000;
		}
		return;
	}
	struct Entry *entry = calloc(1, sizeof (struct Entry));
	entry->key = malloc(strlen(key)+1);
	entry->value = malloc(strlen(value)+1);
	strcpy(entry->key, key);
	strcpy(entry->value, value);
	entry->node.hcode = hash(key);
	entry->node.next = NULL;
	entry->expiry.tv_sec=0;
	if(commandLen==5){
		long ms = strtol(commands[4],NULL, 10);
		gettimeofday(&entry->expiry,NULL);
		entry->expiry.tv_sec += ms/1000;  entry->expiry.tv_usec += ms*1000;
	}
	hm_insert(&g_data.db, &entry->node);
}

char* do_get(char** commands, int commandLen){
	if(commandLen<2)
		return NULL;
	struct Entry key;
	key.node.hcode = hash(commands[1]);
	key.key = commands[1];
	struct HNode* node = hm_lookup(&g_data.db, &key.node, entry_eq);
	if(!node)
		return nil;
	struct Entry * entry = container_of( node, struct Entry,node);
	//check if expiry is set and is expired
	if(entry->expiry.tv_sec!=0 && check_expired(&entry->expiry)){
		hm_pop(&g_data.db, node, entry_eq);
		delete_entry(entry);
		return nil;
	}
	return entry->value;
}

void parseMessage(char **commands, int commandLen, int connFd){

    size_t sentBytes;
	char* writeBuffer;
    if(commandLen>0){
        toLower(commands[0]);
        if(strcmp(ping, commands[0])==0){
            if( (sentBytes = send(connFd, pingMessage, strlen( pingMessage ), 0))==-1){
                perror("send\n");
            }
        }
        else if(strcmp(echo, commands[0])==0 && commandLen>1){

			int value_len = serialize_str(&writeBuffer, commands[1]);
			if ((sentBytes = send(connFd, writeBuffer, value_len, 0)) == -1) {
				perror("send\n");
			}
			free(writeBuffer);
		}
		else if(strcmp(set, commands[0])==0 && commandLen>=3){
			do_set(commands, commandLen);
			if ((sentBytes = send(connFd, ok, strlen(ok), 0)) == -1) {
				perror("send\n");
			}
		}
		else if(strcmp(get, commands[0])==0 && commandLen>=2){
			char * ret = do_get(commands, commandLen);
			int value_len = serialize_str(&writeBuffer, ret);
			if ((sentBytes = send(connFd, writeBuffer, value_len, 0)) == -1) {
				perror("send\n");
			}
			free(writeBuffer);
		}
    }
}

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

	 client_addr_len = sizeof(client_addr);

	 fd_set master, current;
	 FD_SET(server_fd, &master);
	 int max_socket_fd = server_fd+1;
     int client_fd;
	 char buffer[128];
	 char ** commands;
	 int commandLen;
	 size_t nbytes;

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
						 close(i);
						 FD_CLR(i, &master);
						 break;
					 }
					 deCodeRedisMessage(buffer, nbytes, &commands, &commandLen);
                     parseMessage(commands, commandLen, i);
//					 for(int k=0; k<commandLen; k++){free(commands[i]);}
					 free(commands);
				 }
			 }
		 }
     }
	 close(server_fd);

	return 0;
}
