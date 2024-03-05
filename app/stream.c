//
// Created by Aquib Nawaz on 05/03/24.
//
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include "stream.h"
#include "message.h"

void send_helper(int connFd, char* writeBuffer,int value_len){
    int sentBytes;
    do {
        sentBytes = send(connFd, writeBuffer, value_len, 0);
        value_len -= sentBytes;
        writeBuffer += sentBytes;
    }while(sentBytes!=-1 && value_len>0);
    if(sentBytes==-1){
        perror("send\n");
    }
}

void type_command(int connFd, char* key, struct HMap* hmap) {
    SearchKey searchKey = {
        .hcode = hash(key),
        .key = key
    };
    struct HNode *node = hm_lookup(hmap, &searchKey, entry_eq);
    char* writeBuffer, *to_send;
    int value_len;
    if(node==NULL){
        to_send = "none";
    }
    else if(node->type==ENTRY_STR){
        to_send = "string";
    }
    else if(node->type==ENTRY_STREAM){
        to_send = "stream";
    }
    value_len = serialize_str(&writeBuffer, to_send, 0);
    send_helper(connFd, writeBuffer, value_len);
    free(writeBuffer);
}