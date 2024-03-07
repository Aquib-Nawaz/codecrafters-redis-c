//
// Created by Aquib Nawaz on 05/03/24.
//
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <sys/errno.h>
#include "stream.h"
#include "message.h"

pthread_mutex_t xread_block_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t xread_block_cond = PTHREAD_COND_INITIALIZER;

static void send_helper(int connFd, char* writeBuffer,int value_len){
    int sentBytes;
    do {
        sentBytes = (int)send(connFd, writeBuffer, value_len, 0);
        value_len -= sentBytes;
        writeBuffer += sentBytes;
    }while(sentBytes!=-1 && value_len>0);
    if(sentBytes==-1){
        perror("send\n");
    }
}

int compare_ids(char* id1, char* id2) {
    if(strcmp(id2,"+")==0)
        return -1;
    else if(strcmp(id2,"-")==0)
        return 1;
    long id1_ms,id2_ms, id1_seq=0, id2_seq=0;
    char * sep=0;

    id1_ms = strtol(id1, &sep, 10);
    if(sep)
        id1_seq = strtol(sep+1, NULL, 10);

    id2_ms = strtol(id2, &sep, 10);
    if(sep)
        id2_seq = strtol(sep+1, NULL, 10);

    if(id1_ms>id2_ms || (id1_ms==id2_ms && id1_seq>id2_seq)){
        return 1;
    }
    if((id1_ms==id2_ms && id1_seq==id2_seq))
        return 0;
    return -1;
}

int64_t currentTimeMillis() {
    struct timeval time;
    gettimeofday(&time, NULL);
    int64_t s1 = (int64_t)(time.tv_sec) * 1000;
    int64_t s2 = (time.tv_usec / 1000);
    return s1 + s2;
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

char* validate_and_generate_key(char* new_id_str, char* top_id, int* error){
    char* id_start;
    long new_millis,new_id;
    long curr_millis = strtol(top_id, &id_start, 10);
    long curr_id = strtol(id_start+1, NULL, 10);
    *error=1;
    if(new_id_str[0]!='*') {
        new_millis = strtol(new_id_str, &id_start, 10);
    }
    else{
        new_millis = currentTimeMillis();
        id_start = strchr(new_id_str, '-');
    }

    if(id_start && *(id_start+1)!='*') {
        new_id = strtol(id_start + 1, NULL, 10);
    }
    else {
        if(new_millis == curr_millis )
            new_id = curr_id+1;
        else if(new_millis > curr_millis)
            new_id = 0;
        else
            assert(0);
    }

    if(new_millis < 0 || new_id < 0 || (new_millis == 0 && new_id == 0))
        return ID_00_ERROR;
    if((new_millis < curr_millis) || new_millis==curr_millis && new_id <= curr_id)
        return ID_GREATER_THAN_ERR;

    *error = 0;
    int id_len = snprintf(NULL, 0, "%ld-%ld", new_millis, new_id);
    char* ret = malloc(id_len+1);
    snprintf(ret, id_len+1, "%ld-%ld", new_millis, new_id);
    return ret;
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

    if(!entry){
        entry = malloc(sizeof *entry);
        entry->key = malloc(strlen (commands[1])+1);
        sprintf(entry->key, "%s", commands[1]);

        entry->node.type = ENTRY_STREAM;
        entry->node.hcode = searchKey.hcode;
        hm_insert(hmap, &entry->node);
        entry->expiry.tv_sec=0;
        entry->len = 0;
        entry->data = NULL;
    }

    int error;
    char * id_or_error = validate_and_generate_key(commands[2], entry->data?entry->data->id:"0-0", &error);

    if(error){
        send_helper(connFd, id_or_error, (int)strlen(id_or_error));
        return;
    }

    struct StreamData* data = create_stream_data(commands+2, commandLen-2);

    data->id = id_or_error;
    data->prev = entry->data;

    pthread_mutex_lock(&xread_block_mutex);
    entry->data = data;
    entry->len++;
    pthread_cond_broadcast(&xread_block_cond);
    pthread_mutex_unlock(&xread_block_mutex);

    char* writeBuffer;
    int value_len;
    value_len = serialize_str(&writeBuffer, data->id, 0);
    send_helper(connFd, writeBuffer, value_len);
    free(writeBuffer);

}

int calculate_stream_data_len (struct StreamData* it){
    int value_len = 0;
    value_len+= snprintf(NULL, 0, ARRAY_PREFIX, 2);
    value_len+= snprintf(NULL, 0, STRING_DATA_FORMAT, strlen(it->id),it->id);
    value_len+= snprintf(NULL, 0, ARRAY_PREFIX, 2*it->len);
    value_len+= calculate_buffer_len(it->keys, it->len);
    value_len+= calculate_buffer_len(it->values, it->len);
    return value_len;
}

int serialize_stream(struct StreamData* start,  struct StreamData* end, char** writeBuffer){

    int value_len=0, num_entries=0;
    struct StreamData* it;
    for(it = start; it!=end; it=it->prev){
        value_len+= calculate_stream_data_len(it);
        num_entries++;
    }

    value_len+= snprintf(NULL, 0, ARRAY_PREFIX, num_entries);
    * writeBuffer = malloc(value_len+1);
    snprintf(*writeBuffer, value_len+1, ARRAY_PREFIX, num_entries);

    int current_pos = value_len;
    for(it = start; it!=end; it=it->prev) {
        int it_data_len = calculate_stream_data_len(it);
        current_pos-=it_data_len;

        current_pos += snprintf(*writeBuffer + current_pos, value_len + 1 - current_pos, ARRAY_PREFIX, 2);
        current_pos += snprintf(*writeBuffer + current_pos, value_len + 1 - current_pos, STRING_DATA_FORMAT,
                                strlen(it->id), it->id);
        current_pos += snprintf(*writeBuffer + current_pos, value_len + 1 - current_pos, ARRAY_PREFIX, 2 * it->len);
        for (int i = 0; i < it->len; i++) {
            current_pos += snprintf(*writeBuffer + current_pos, value_len + 1 - current_pos, STRING_DATA_FORMAT,
                                    strlen(it->keys[i]), it->keys[i]);
            current_pos += snprintf(*writeBuffer + current_pos, value_len + 1 - current_pos, STRING_DATA_FORMAT,
                                    strlen(it->values[i]), it->values[i]);
        }
        if(current_pos!=value_len)
            (*writeBuffer)[current_pos]='*';
        current_pos-=it_data_len;
    }
    return value_len;

}

void xrange_command(int connFd, char** commands, int commandLen, struct HMap* hmap){
    if(commandLen<4){
        return;
    }
    SearchKey searchKey = {
            .key=commands[1],
            .hcode=hash(commands[1]),
            };
    struct HNode *node = hm_lookup(hmap, &searchKey, entry_eq);
    if(!node || node->type==ENTRY_STR){
        send_helper(connFd, EMPTY_ARRAY, (int)strlen(EMPTY_ARRAY));
        return;
    }

    struct Entry_Stream* stream = get_stream_container(node);
    struct StreamData* cur = stream->data;
    char* st_id = commands[2];
    char* end_id = commands[3];

    while(cur){
        if(compare_ids(cur->id, end_id)<=0)
            break;
        cur = cur->prev;
    }

    if(!cur){
        send_helper(connFd, EMPTY_ARRAY, (int)strlen(EMPTY_ARRAY));
        return;
    }
    struct StreamData* end = cur;
    while(end && compare_ids(end->id, st_id)>=0){
        end = end->prev;
    }
    char* writeBuffer;
    int value_len = serialize_stream(cur, end, &writeBuffer);
    send_helper(connFd, writeBuffer, value_len);
    free(writeBuffer);

}

void xread_command(int connFd, char** commands, int commandLen, struct HMap* hmap, int *error){
    *error=0;
    if(commandLen<2)
        return;
    int numStreams = commandLen/2;
    struct StreamData* stream_data;
    SearchKey searchKey;
    char send_buffer[200];
    struct Entry_Stream* unblocked_streams [numStreams];
    int found=0;

    for(int i=0; i<numStreams; i++) {
        searchKey.hcode = hash(commands[i]);
        searchKey.key = commands[i];
        struct HNode *node = hm_lookup(hmap, &searchKey, entry_eq);
        if(node && node->type==ENTRY_STREAM){
            struct Entry_Stream* stream = get_stream_container(node);
            if(stream->data && compare_ids(stream->data->id, commands[i+numStreams])>0){
                unblocked_streams[found++] = stream;
            }
        }
    }

    if(!found){
        *error=1;
        return;
    }

    snprintf(send_buffer, 200, ARRAY_PREFIX, found);
    send_helper(connFd, send_buffer, (int)strlen(send_buffer));

    for(int i=0; i<found; i++){
        struct Entry_Stream* stream = unblocked_streams[i];
        snprintf(send_buffer, 200, ARRAY_PREFIX, 2);
        snprintf(send_buffer+ strlen(send_buffer), 200- strlen(send_buffer), STRING_DATA_FORMAT,
                 strlen(stream->key), stream->key);
        send_helper(connFd, send_buffer, (int)strlen(send_buffer));
        stream_data = stream->data;
        struct StreamData* it;
        char* st = commands[numStreams+i];
        for(it = stream_data; it && compare_ids(it->id, st)>0; it=it->prev){
        }
        char* writeBuffer;
        int value_len = serialize_stream(stream_data, it,  &writeBuffer);
        send_helper(connFd, writeBuffer, value_len);
        free(writeBuffer);
    }
}

void *xread_block(void* args){
    BlockThread * arguments = (BlockThread*)args;
    int error;
    int rt;
    struct timespec expiry;
    struct timeval time;
    gettimeofday(&time, NULL);
    expiry.tv_sec = time.tv_sec ;
    expiry.tv_nsec = (time.tv_usec + arguments->timeout*1000L)* 1000L;
    if(expiry.tv_nsec>=1000000000L){
        expiry.tv_sec++;
        expiry.tv_nsec -= 1000000000L;
    }
    while(1) {
        xread_command(arguments->connFd, arguments->commands, arguments->commandLen, arguments->hmap, &error);
        if(!error)
            break;
        if(arguments->timeout!=0){
            pthread_mutex_lock(&xread_block_mutex);
            rt = pthread_cond_timedwait(&xread_block_cond, &xread_block_mutex, &expiry);
            pthread_mutex_unlock(&xread_block_mutex);

            if(rt == ETIMEDOUT){
                send_helper(arguments->connFd, nil, (int)strlen(nil));
                break;
            }
        }
        else {
            pthread_mutex_lock(&xread_block_mutex);
            pthread_cond_timedwait(&xread_block_cond, &xread_block_mutex, &expiry);
            pthread_mutex_unlock(&xread_block_mutex);
        }
    }
    for(int i=0;i<arguments->commandLen; i++){
        free(arguments->commands[i]);
    }
    free(arguments->commands);
    free(args);
    return NULL;
}
