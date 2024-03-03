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
#define HANDSHAKE_MESSAGE_3 "*3\r\n$5\r\nPSYNC\r\n$1\r\n?\r\n$2\r\n-1\r\n"
#define EMPTY_RDB "empty.rdb"
#define MAX_REPLICAS 100
#define GETACK_REPLY "*3\r\n$8\r\nREPLCONF\r\n$3\r\nACK\r\n$1\r\n%d\r\n"
#define GET_ACK "GETACK"

void info_command(int);
int doReplicaStuff(char*,char*,int);
void psync_command(int);
void replconf_command(int , char **, int);
void send_to_replicas(char **, int );

#endif //CODECRAFTERS_REDIS_C_REPLICATION_H
