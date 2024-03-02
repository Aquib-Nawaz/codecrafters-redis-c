//
// Created by Aquib Nawaz on 02/03/24.
//

#ifndef CODECRAFTERS_REDIS_C_REPLICATION_H
#define CODECRAFTERS_REDIS_C_REPLICATION_H

#define REPLICATION_ID "QXF1aWJOYXdhelJhZ2luaVJhbmphbmhhaGFoYWhh"
#define MASTER "master"
#define SLAVE "slave"
#define ROLE "role"
#define MASTER_REPL_ID "master_replid"
#define MASTER_REPL_OFFSET "master_repl_offset"
#define REPLCONF "REPLCONF"
#define LISTENING_PORT "listening-port"
#define REPLCONF_MESSAGE_2 "*3\r\n$8\r\nREPLCONF\r\n$4\r\ncapa\r\n$6\r\npsync2\r\n"

void info_command(int);
void doReplicaStuff(char*,char*,int);

#endif //CODECRAFTERS_REDIS_C_REPLICATION_H
