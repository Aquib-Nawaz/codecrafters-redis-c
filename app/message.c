//
// Created by Aquib Nawaz on 06/02/24.
//
#include <stdio.h>
#include<string.h>
#include<ctype.h>
#include <stdlib.h>
#include "message.h"

void toLower(char * p){
    for ( ; *p; ++p) *p = tolower(*p);
}

void deCodeRedisMessage(char *message, int msgSize, char ***commands, int *arrayLen){
    int newMsg = 0, newKeyword=0, keywordLen=0, keywordNum=-1;
    for(int i=0; i<msgSize; i++){
        char c = message[i];
        switch(c){
            case '*':
                newMsg = 1;
                *arrayLen=0;
                printf("New Message\n");
                break;
            case '$':
                newKeyword = 1;
                keywordNum+=1;
                keywordLen = 0;
                printf("New Keyword\n");
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
            case '\n':
                if(newKeyword){
                    printf("Copying %d chars from position %d to command pos %d\n", keywordLen, i+1, keywordNum);
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

}

#if 0
int main(){

    char *message = "*2\r\n$4\r\nECHO\r\n$3\r\nhey\r\n";
    int  arrayLen=0;
    char **commands;
    deCodeRedisMessage(message, strlen(message),&commands, &arrayLen);

    printf("Message length: %d\n", arrayLen);
    for(int i=0; i<arrayLen; i++){
        printf("KeyWord %d, Size %d:- %s\n", i,strlen(commands[i]), commands[i]);
    }
}
#endif