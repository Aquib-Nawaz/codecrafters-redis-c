//
// Created by Aquib Nawaz on 06/02/24.
//

#ifndef REDIS_MESSAGE_H
#define REDIS_MESSAGE_H

void deCodeRedisMessage(char*, int, char ***, int *);
void toLower(char*);
char* echo = "echo";
char* ping = "ping";
char* pingMessage = "+PONG\r\n";
char* get = "get";
char* set = "set";
char* ok = "+OK\r\n";
char* nil = "+nil\r\n";

#endif //REDIS_MESSAGE_H

