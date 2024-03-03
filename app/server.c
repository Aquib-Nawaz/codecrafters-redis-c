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
#include <time.h>
#include <stddef.h>
#include "message.h"
#include "hashset.h"
#include "parser.h"
#include "replication.h"

static struct {
	struct HMap db;
} g_data;

char dir[128]="";
char dbfilename[128]="";
int master_fd=-1;

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
        else if(strcmp(ECHO, commands[0])==0 && commandLen>1){

			int value_len = serialize_str(&writeBuffer, commands[1], 0);
			if ((sentBytes = send(connFd, writeBuffer, value_len, 0)) == -1) {
				perror("send\n");
			}
			free(writeBuffer);
		}
		//write command
		else if(strcmp(set, commands[0])==0 && commandLen>=3){
			do_set(commands, commandLen,&g_data.db );
			if(master_fd!=connFd){
				if ((sentBytes = send(connFd, ok, strlen(ok), 0)) == -1) {
					perror("send\n");
				}
			}
			send_to_replicas(commands, commandLen);
		}
        //check if command matches with get
		else if(strcmp(get, commands[0])==0 && commandLen>=2){
			char * ret = do_get(commands, commandLen, &g_data.db);
			int value_len = serialize_str(&writeBuffer, ret, 1);
//			printf("Sending %.*s",value_len,writeBuffer);
			if ((sentBytes = send(connFd, writeBuffer, value_len, 0)) == -1) {
				perror("send\n");
			}
			free(writeBuffer);
		}
        else if(strcmp(config, commands[0])==0 && commandLen>=2){
            toLower(commands[1]);
            if(strcmp(get, commands[1])==0 && commandLen>=3){
				toLower(commands[2]);
                int retStringsLen = 2;
				char * retStrings[2];
				if(strcmp(DIR, commands[2])==0){
                	retStrings[0] = DIR;
                	retStrings[1] = dir;
				}
				else if(strcmp(DBFILENAME, commands[2])==0){
                	retStrings[0] = DBFILENAME;
                	retStrings[1] = dbfilename;
				}
				int value_len = serialize_strs(&writeBuffer, retStrings, retStringsLen);
				do {
					sentBytes = send(connFd, writeBuffer, value_len, 0);
					value_len -= sentBytes;
				}while(sentBytes!=-1 && value_len>0);
				if(sentBytes==-1){
					perror("send\n");
				}
				free(writeBuffer);
            }
        }
        else if(strcmp(KEYS, commands[0])==0 && commandLen>=2){
			char **ret;
			int retLen = hm_scan(&ret, &g_data.db);
            int value_len = serialize_strs(&writeBuffer, ret, retLen);
            do {
                sentBytes = send(connFd, writeBuffer, value_len, 0);
                value_len -= sentBytes;
            }while(sentBytes!=-1 && value_len>0);
        }
		else if(strcmp(commands[0],INFO)==0){
			info_command(connFd);
		}
		else if(strcmp(commands[0], replconf)==0){
			replconf_command(connFd, commands, commandLen);
		}
		else if(strcmp(commands[0], psync)==0){
			psync_command(connFd);
		}
    }
}

void doDbFileStuff(){
    int nameLength = strlen(dir) +  strlen(dbfilename);
	char filename[nameLength+2];
	snprintf(filename, sizeof (filename), "%s/%s", dir, dbfilename);

    FILE *fptr;
	if(nameLength == 0)
		return;
    if((fptr = fopen(filename, "rb"))==NULL){

        perror("fopen: ");
        return;
//        exit(1);
    }
    seekData(fptr);
    int len;
    char **data;
    getData(&data, fptr, &len);
    for(int i=0;i<len-2; i+=3){
        printf("%s->%s\n", data[i+1], data[i+2]);
        struct Entry *entry = calloc(1, sizeof (struct Entry));
		switch(data[i][0]) {
			unsigned long t2;
			unsigned int t1;
			case 0:
				entry->expiry.tv_sec = 0;
				break;
			case 1:
				memcpy(&t1, data[i]+1, sizeof t1);
				entry->expiry.tv_sec = t1;
				break;
			case 2:
				memcpy(&t2, data[i]+1, sizeof t2);
				entry->expiry.tv_sec = t2/1000;
				entry->expiry.tv_usec = (t2%1000)*1000;
				time_t ti= time(NULL);
				printf("%li\n", ti-entry->expiry.tv_sec);
				break;

		}
        entry->key = data[i+1];
        entry->value = data[i+2];
        entry->node.hcode = hash(data[i+1]);
        entry->node.next = NULL;
		free(data[i]); //time allocated
        hm_insert(&g_data.db, &entry->node);
    }
	free(data);
}

int main(int argc, char *argv[]) {
	int replica_of=0;
	char* master_port;
	char* master_host;
    int port=6379;
    for(int cnt=1;cnt<argc-1; cnt+=2){
        if(strcmp(argv[cnt], "--dir")==0){
            strcpy(dir, argv[cnt+1]);
        }
        else if(strcmp(argv[cnt], "--dbfilename")==0){
            strcpy(dbfilename, argv[cnt+1]);
        }
        else if(strcmp(argv[cnt], "--port")==0){
            port = atoi(argv[cnt+1]);
        }
		else if(strcmp(argv[cnt], "--replicaof")==0){

			master_port = argv[cnt+2];
			master_host = argv[cnt+1];
			replica_of=1;
			cnt++;
        }
    }
	setbuf(stdout, NULL);
    doDbFileStuff();
	if(replica_of)
    	master_fd = doReplicaStuff(master_host, master_port, port);

	// You can use print statements as follows for debugging, they'll be visible when running tests.
	printf("Logs from your program will appear here!\n");

	// Uncomment this block to pass the first stage

	 int server_fd;
	 unsigned int client_addr_len;
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
	 	printf("SO_REUSEADDR failed: %s \n", strerror(errno));
	 	return 1;
	 }

	reuse = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
		printf("SO_REUSEPORT failed: %s \n", strerror(errno));
		return 1;
	}

	 struct sockaddr_in serv_addr = { .sin_family = AF_INET ,
	 								 .sin_port = htons(port),
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
	 if(master_fd!=-1)
		 FD_SET(master_fd, &master);
	 int max_socket_fd = server_fd+1;
     int client_fd;
	 char buffer[1024];
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
					 int parsed_len=0;
					 do {
						 commands=NULL;
						 parsed_len += deCodeRedisMessage(buffer+parsed_len, nbytes, &commands, &commandLen);
						 parseMessage(commands, commandLen, i);
						 for (int k = 0; k < commandLen; k++) { free(commands[k]); }
						 if(commands)
						 	free(commands);
					 }
					 while(parsed_len<nbytes);
				 }
			 }
		 }
     }
	 close(server_fd);

	return 0;
}
