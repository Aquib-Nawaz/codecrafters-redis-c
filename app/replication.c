#include <sys/socket.h>
#include "replication.h"
#include "message.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

#if 1
int replica_of=0;

void send_helper(int connFd, char* writeBuffer,int value_len){
    int sentBytes;
    do {
        sentBytes = send(connFd, writeBuffer, value_len, 0);
        value_len -= sentBytes;
    }while(sentBytes!=-1 && value_len>0);
    if(sentBytes==-1){
        perror("send\n");
    }
}

void info_command(int connFd){
    char message[200];
    snprintf(message, 200, "%s:%s\n%s:%s\n%s:%d", ROLE, replica_of ? SLAVE : MASTER, MASTER_REPL_ID,
             REPLICATION_ID, MASTER_REPL_OFFSET, 0);
    char* writeBuffer;
    int value_len = serialize_str(&writeBuffer, message, 1);
    send_helper(connFd, writeBuffer, value_len);
    free(writeBuffer);
}

void doReplicaStuff(char* master_host, char* master_port){
    replica_of=1;
    int master_fd;
    struct sockaddr_in master_addr;
    struct addrinfo hints;
    struct addrinfo *servinfo, *p;  // will point to the results

    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_INET;     // Only IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets

    int status;
    if ((status = getaddrinfo(master_host, master_port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }

    for(p = servinfo; p != NULL; p = p->ai_next) {

        if ((master_fd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("replica: socket");
            continue;
        }
        char addr_buf[INET6_ADDRSTRLEN];
        memset(addr_buf, 0, sizeof(addr_buf));
        inet_ntop(AF_INET, &((struct sockaddr_in *)p->ai_addr)->sin_addr, addr_buf, sizeof(addr_buf));

//        printf("replica: connecting to master server at %s\n", addr_buf);
        if (connect(master_fd, p->ai_addr, p->ai_addrlen) == -1) {
            perror("replica: connect");
            close(master_fd);
            continue;
        }
        break;
    }

    if (p == NULL) {
        fprintf(stderr, "replica: failed to connect to master\n");
//        exit(19);
    }
    char* write_buffer;
    char **msg = (char**)malloc(sizeof(char*));
    *msg = ping;
    int value_len = serialize_strs(&write_buffer, msg, 1);
    free(msg);
//    printf("Message to send and size %s - %d\n", write_buffer, value_len);
    send_helper(master_fd, write_buffer, value_len);
    free(write_buffer);
    freeaddrinfo(servinfo); // all done with this structure

    close(master_fd);
}
#endif
#if 0
int main()
{
    char addr_buf[64];
    struct addrinfo* feed_server = NULL;

    memset(addr_buf, 0, sizeof(addr_buf));

    getaddrinfo("localhost", NULL, NULL, &feed_server);
    struct addrinfo *res;
    for(res = feed_server; res != NULL; res = res->ai_next)
    {
        if ( res->ai_family == AF_INET )
        {
            inet_ntop(AF_INET, &((struct sockaddr_in *)res->ai_addr)->sin_addr, addr_buf, sizeof(addr_buf));
        }
        else
        {
            inet_ntop(AF_INET6, &((struct sockaddr_in6 *)res->ai_addr)->sin6_addr, addr_buf, sizeof(addr_buf));
        }

        printf("hostname: %s\n", addr_buf);
    }

    return 0;
}
#endif