//
// Created by Aquib Nawaz on 06/02/24.
//
#include <stdio.h>
#include<string.h>
#include<ctype.h>
#include <stdlib.h>
#include <assert.h>
#include "message.h"

void toLower(char * p){
    for ( ; *p; ++p) *p = tolower(*p);
}


int serialize_str(char **writeBuffer, char* str, int bulk){
    size_t str_len = strlen(str);
    if(strcmp(nil, str)==0){
        *writeBuffer = calloc(str_len+1, sizeof (char ));
        strcpy(*writeBuffer, str);
        return str_len;
    }
    if(bulk){
        int ret_len = snprintf(NULL, 0, "$%zu\r\n%s\r\n", str_len, str);
        *writeBuffer = calloc(ret_len+1, sizeof (char ));
        return snprintf(*writeBuffer, ret_len+1, "$%zu\r\n%s\r\n",
                 str_len, str);
    }
    *writeBuffer = calloc(str_len + 4, sizeof(char));
    (*writeBuffer)[0] = '+';
    strcpy(*writeBuffer+1, str);
    strcpy(*writeBuffer+1+str_len, "\r\n");
    return str_len+3;
}

int calculate_buffer_len(char ** strs, int size){
    //*size\r\n$len\r\ndir\r\n
    int retLength = 0;
    retLength += snprintf(NULL, 0, "*%d\r\n" ,size);
    for(int i=0; i<size; i++){
        retLength += snprintf(NULL, 0, "$%zu\r\n%s\r\n", strlen(strs[i]), strs[i]);
    }
    return retLength;
}

int serialize_strs(char **writeBuffer, char** strs, int size){
    int writeBufferLen = calculate_buffer_len(strs, size);
    *writeBuffer = malloc(writeBufferLen+1);
    int idx=0;
    idx += snprintf(*writeBuffer, writeBufferLen+1, "*%d\r\n" ,size);
    for(int i=0; i<size; i++){
        idx += snprintf(*writeBuffer + idx, writeBufferLen+1-idx,
                        "$%zu\r\n%s\r\n", strlen(strs[i]), strs[i]);
    }
    return idx;
}

int deCodeRedisMessage(char *message, int msgSize, char ***commands, int *arrayLen){
    int newMsg = 0, newKeyword=0, keywordLen=0, keywordNum=-1;
//    printf("Decoding Message:- %s\n", message);
    int i;
    for(i=0; i<msgSize; i++){
        char c = message[i];
        switch(c){
            case '*':
                if(i!=0)
                    return i;
                newMsg = 1;
                *arrayLen=0;
                printf("New Message\n");
                break;
            case '$':
                newKeyword = 1;
                keywordNum+=1;
                assert(keywordNum<*arrayLen);
                keywordLen = 0;
                break;
            case '\r':
                if(newMsg){
                    newMsg = 0;
                    *commands = (char**)malloc(sizeof(char *)*(*arrayLen));
                }
                else if(newKeyword){
                    (*commands)[keywordNum] = (char*)malloc(keywordLen+1);
                }
                break;
            case ':':
                keywordLen=0;
                if(message[i+1]=='+'||message[i+1]=='-')
                    keywordLen++;
                while(isdigit(message[i+keywordLen+1]))
                    keywordLen++;
                (*commands)[keywordNum] = malloc(keywordLen+1);
//                printf("Copying %.*s from position %d to command pos %d\n", keywordLen,message+i+1, i+1, keywordNum);
                strncpy((*commands)[keywordNum], message+i+1, keywordLen);
                (*commands)[keywordNum][keywordLen]='\0';
                i+=keywordLen;
                keywordNum++;
                break;

            case '\n':
                if(newKeyword){
//                    printf("Copying %.*s from position %d to command pos %d\n", keywordLen,message+i+1, i+1, keywordNum);
                    strncpy((*commands)[keywordNum], message+i+1, keywordLen);
                    (*commands)[keywordNum][keywordLen] = '\0';
                    i+=keywordLen;
                    newKeyword=0;
                }
                break;
            default:
                if(isdigit(c)){
                    if(newMsg){
                        *arrayLen = *arrayLen*10 + c-'0';
                    }
                    else if(newKeyword){
                        keywordLen = keywordLen*10 + c-'0';
                    }
                }
        }
    }
    return i;

}

#if 0
int main(){

    char *message = "*3\r\n$4\r\nECHO\r\n$3\r\nhey\r\n:523";
    int  arrayLen=0;
    char **commands;
    deCodeRedisMessage(message, strlen(message),&commands, &arrayLen);

//    printf("Message length: %d\n", arrayLen);
//    for(int i=0; i<arrayLen; i++){
//        printf("KeyWord %d, Size %d:- %s\n", i,strlen(commands[i]), commands[i]);
//    }
    int l= serialize_str(commands, "pears",1);
    for(int i=0;i<l;i++){
        if(isprint((*commands)[i]))
            printf("%c ", (*commands)[i]);
        else
            printf("%d ", (*commands)[i]);
    }
//    assert(strcmp("$-1\r\n", *commands)==0);
    free(*commands);
}
#endif