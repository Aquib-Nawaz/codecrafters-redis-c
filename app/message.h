//
// Created by Aquib Nawaz on 06/02/24.
//

#ifndef REDIS_MESSAGE_H
#define REDIS_MESSAGE_H

void deCodeRedisMessage(char*, int, char ***, int *);
void toLower(char*);
static char* echo = "echo";
static char* ping = "ping";
static char* pingMessage = "+PONG\r\n";
static char* get = "get";
static char* set = "set";
static char* ok = "+OK\r\n";
static char* nil = "+nil\r\n";

#endif //REDIS_MESSAGE_H

