//
// Created by Aquib Nawaz on 06/02/24.
//

#ifndef REDIS_MESSAGE_H
#define REDIS_MESSAGE_H

void deCodeRedisMessage(char*, int, char ***, int *);
void toLower(char*);
int serialize_str(char **writeBuffer, char* str);

static char* echo = "echo";
static char* ping = "ping";
static char* pingMessage = "+PONG\r\n";
static char* get = "get";
static char* set = "set";
static char* ok = "+OK\r\n";
static char* nil = "$-1\r\n";
static char* nullBulk = "";

#endif //REDIS_MESSAGE_H

