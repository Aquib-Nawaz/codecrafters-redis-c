#include <sys/socket.h>
#include "replication.h"
#include "message.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


int replica_of=0;

void info_command(int connFd){
    char message[200];
    snprintf(message, 200, "%s:%s\n%s:%s\n%s:%d", ROLE, replica_of ? SLAVE : MASTER, MASTER_REPL_ID,
             REPLICATION_ID, MASTER_REPL_OFFSET, 0);
    char* writeBuffer;
    int value_len = serialize_str(&writeBuffer, message, 1);
    int sentBytes;
    do {
        sentBytes = send(connFd, writeBuffer, value_len, 0);
        value_len -= sentBytes;
    }while(sentBytes!=-1 && value_len>0);
    if(sentBytes==-1){
        perror("send\n");
    }
    free(writeBuffer);
}