//
// Created by Aquib Nawaz on 05/03/24.
//
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include "stream.h"
#include "message.h"

static void send_helper(int connFd, char* writeBuffer,int value_len){
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
    else{
        printf("Unknown type\n");
        return;
    }
    value_len = serialize_str(&writeBuffer, to_send, 0);
    send_helper(connFd, writeBuffer, value_len);
    free(writeBuffer);
}

struct StreamData* create_stream_data(char ** commands, int commandLen){

    struct StreamData* data = malloc(sizeof *data);
    data->id = malloc(strlen(commands[0])+1);
    sprintf(data->id, "%s", commands[0]);
    data->len = 0;

    data->keys = malloc(sizeof(char*)*(commandLen-3)/2);
    data->values = malloc(sizeof(char*)*(commandLen-3)/2);

    for(int i=1;i+1<commandLen;i+=2){
        data->keys[data->len] = malloc(strlen(commands[i]));
        sprintf(data->keys[data->len], "%s", commands[i]);
        data->values[data->len] = malloc(strlen(commands[i+1]));
        sprintf(data->values[data->len], "%s", commands[i+1]);
        data->len++;
    }
    return data;
}


void xadd_command(int connFd, char** commands, int commandLen, struct HMap* hmap){
    if(commandLen<3){
        return;
    }
    SearchKey searchKey = {
            .key=commands[1],
            .hcode=hash(commands[1]),
            };
    struct HNode *node = hm_lookup(hmap, &searchKey, entry_eq);
    struct Entry_Stream* entry = NULL;

    if(node){
        if(node->type==ENTRY_STR){
            delete_entry(get_string_container(node),node->type);
        }
        else if(node->type==ENTRY_STREAM){
            entry = get_stream_container(node);
        }
    }

    char* id_start;
    long new_millis = strtol(commands[2], &id_start, 10);
    long new_id = strtol(id_start+1, NULL, 10);

    if(new_millis < 0 || new_id <0 ||(new_millis==0 && new_id==0)){
        send_helper(connFd, ID_00_ERROR, strlen(ID_00_ERROR));
        return;
    }

    if(!entry){
        entry = malloc(sizeof *entry);
        entry->key = malloc(strlen (commands[1])+1);
        sprintf(entry->key, "%s", commands[1]);

        entry->node.type = ENTRY_STREAM;
        entry->node.hcode = searchKey.hcode;
        hm_insert(hmap, &entry->node);

        entry->len = 0;
        entry->data = NULL;
    }
    else{
        //Validate that id is smaller than current id
        long curr_millis = strtol(entry->data->id, &id_start, 10);
        long curr_id = strtol(id_start+1, NULL, 10);

        if(strcmp("*",commands[2])!=0){

           if(new_millis<curr_millis || (new_millis==curr_millis && new_id<=curr_id)){
                printf("Greater Than Error\n");
               fflush(stdout);
                send_helper(connFd, ID_GREATER_THAN_ERR, strlen(ID_GREATER_THAN_ERR));
                return;
            }
        }
    }

    struct StreamData* data = create_stream_data(commands+2, commandLen-2);

    data->prev = entry->data;
    entry->data = data;
    entry->len++;

    char* writeBuffer;
    int value_len;
    value_len = serialize_str(&writeBuffer, data->id, 0);
    send_helper(connFd, writeBuffer, value_len);
    free(writeBuffer);

}
