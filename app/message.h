//
// Created by Aquib Nawaz on 06/02/24.
//

#ifndef REDIS_MESSAGE_H
#define REDIS_MESSAGE_H

void deCodeRedisMessage(char*, int, char ***, int *);
void toLower(char*);
int serialize_str(char **writeBuffer, char* str, int bulk);
int serialize_strs(char **writeBuffer, char** strs,int size);
int calculate_buffer_len(char **, int);

#define KEYS "keys"
#define INFO "info"
#define ECHO "echo"
#define ping "ping"
#define pingMessage "+PONG\r\n"
#define get "get"
static char* set = "set";
static char* ok = "+OK\r\n";
static char* nil = "$-1\r\n";
static char* nullBulk = "";
static char* config = "config";
static char* DIR = "dir";
static char* DBFILENAME = "dbfilename";

#endif //REDIS_MESSAGE_H

