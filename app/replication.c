#include <sys/socket.h>
#include "replication.h"
#include "message.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>

#if 1
int replica_of=0;
int  replicas_fd[MAX_REPLICAS];
int num_replicas=0;
int master_fd=-1;
int master_offset=0;
int replicas_acknowledged=0;
pthread_t replica_thread[MAX_REPLICAS];
pthread_mutex_t ack_mutex = PTHREAD_MUTEX_INITIALIZER;
int replicas_offset[MAX_REPLICAS]={0};

void send_helper(int connFd, char* writeBuffer,int value_len){
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

void* handle_replica_ack(void *args){
    int thread_num = *((int*)args);
    int connFd = replicas_fd[thread_num];
    assert(connFd>0);
    char read_buffer[100];
    ssize_t nbytes;
    do {
        nbytes=recv(connFd, read_buffer, sizeof read_buffer, 0);
        fflush(stdout);
        if(nbytes>0){
            int i=(int)nbytes-3;
            int replica_offset=0, pow=1;
            while(read_buffer[i]<='9'&&read_buffer[i]>='0'){
                replica_offset+=(read_buffer[i--]-'0')*pow;
                pow*=10;
            }

            printf("Given offset %d, Expected offset %d\n",replica_offset,
                   replicas_offset[thread_num]);

            if(replica_offset>=replicas_offset[thread_num]- strlen(GET_ACK_COMMAND)){
                pthread_mutex_lock(&ack_mutex);
                replicas_acknowledged++;
                pthread_mutex_unlock(&ack_mutex);
                printf("Received Acknowledgement\n");
                fflush(stdout);
            }
        }
    }while(nbytes!=-1);
    printf("Can't Receive Ack anymore\n");
    fflush(stdout);
    return NULL;

}

void replconf_command(int connFd, char** commands, int commandLen){
    if(commandLen!=3)
        return;
    toLower(commands[1]);
    if(strcmp(commands[1], LISTENING_PORT)==0){
        if(num_replicas==MAX_REPLICAS)
            return;
        replicas_fd[num_replicas] = connFd;
        pthread_mutex_lock(&ack_mutex);
        replicas_acknowledged++;
        pthread_mutex_unlock(&ack_mutex);
    }

    else if(strcmp(commands[1], GET_ACK)==0){
        printf("Received ack from master, current-offset %d\n", master_offset);
        char writeBuffer[100];
        int offset_size = snprintf(NULL, 0, "%d", master_offset);
        int value_len = snprintf(writeBuffer, sizeof writeBuffer, GETACK_REPLY,
                                offset_size, master_offset);
        send_helper(connFd, writeBuffer, value_len);
        return;
    }
    send(connFd, ok, strlen(ok), 0);
}

void info_command(int connFd){
    char message[200];
    snprintf(message, 200, "%s:%s\n%s:%s\n%s:%d", ROLE, replica_of ? SLAVE : MASTER, MASTER_REPL_ID,
             REPLICATION_ID, MASTER_REPL_OFFSET, 0);
    char* writeBuffer;
    int value_len = serialize_str(&writeBuffer, message, 1);
    send_helper(connFd, writeBuffer, value_len);
    free(writeBuffer);
}

void psync_command(int connFd){
    if(replica_of)
        return;
    char * writeBuffer;
    int value_len = snprintf(NULL, 0 ,"+FULLRESYNC %s 0\r\n", REPLICATION_ID);
    writeBuffer = malloc(value_len+1);
    sprintf(writeBuffer, "+FULLRESYNC %s 0\r\n", REPLICATION_ID);
    send(connFd, writeBuffer, value_len, 0);
    free(writeBuffer);
    FILE* fptr = fopen(EMPTY_RDB, "rb");
    if(!fptr){
        perror("fopen");
        return;
    }
    fseek(fptr, 0, SEEK_END);
    int file_size = (int)ftell(fptr);
    fseek(fptr, 0, SEEK_SET);
    int prefix_size = snprintf(NULL, 0 ,"$%d\r\n", file_size);
    value_len = prefix_size+file_size;
    writeBuffer = malloc(value_len+1);
    snprintf(writeBuffer, value_len+1, "$%d\r\n", file_size);
    fread(writeBuffer+prefix_size, 1,file_size, fptr);
    send_helper(connFd, writeBuffer, value_len);
    free(writeBuffer);
    fclose(fptr);
    int *cur_thread_num=malloc(sizeof (int));
    *cur_thread_num = num_replicas;
    pthread_create(&replica_thread[*cur_thread_num], NULL, handle_replica_ack, cur_thread_num);
    num_replicas++;
}

void* wait_command(void*args){

    struct WaitThreadArgs* waitThreadArgs = (struct WaitThreadArgs*)args;
    int connFd = waitThreadArgs->connFd;
    int expected_replicas = waitThreadArgs->expected_replica;
    long waitTime = waitThreadArgs->waitTime;

    char send_buffer[100];
    struct timeval start_time, cur_time, time_difference;
    gettimeofday(&start_time, NULL);
    uint64_t millis;
    while(1) {
        gettimeofday(&cur_time,NULL);
        timersub(&cur_time, &start_time, &time_difference);
        millis = (time_difference.tv_sec * (uint64_t)1000) + (time_difference.tv_usec / 1000);
        if(millis > waitTime || replicas_acknowledged>=expected_replicas){
            snprintf(send_buffer, sizeof send_buffer, WAIT_RESPONSE, replicas_acknowledged);
            send(connFd, send_buffer, strlen(send_buffer), 0);
            printf("WAIT Response %s sent\n", send_buffer);
            fflush(stdout);
            return NULL;
        }
    }
}

int doReplicaStuff(char* master_host, char* master_port, int my_port){
    replica_of=1;
    struct addrinfo hints;
    struct addrinfo *servinfo, *p;  // will point to the results

    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_INET;     // Only IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets

    int status;
    if ((status = getaddrinfo(master_host, master_port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }

    for(p = servinfo; p != NULL; p = p->ai_next) {

        if ((master_fd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("replica: socket");
            continue;
        }
        char addr_buf[INET6_ADDRSTRLEN];
        memset(addr_buf, 0, sizeof(addr_buf));
        inet_ntop(AF_INET, &((struct sockaddr_in *)p->ai_addr)->sin_addr, addr_buf, sizeof(addr_buf));

//        printf("replica: connecting to master server at %s\n", addr_buf);
        if (connect(master_fd, p->ai_addr, p->ai_addrlen) == -1) {
            perror("replica: connect");
            close(master_fd);
            continue;
        }
        break;
    }

    if (p == NULL) {
        fprintf(stderr, "replica: failed to connect to master\n");
//        exit(19);
    }
    char* write_buffer;
    char **msg = (char**)malloc(sizeof(char*));
    *msg = ping;
    int value_len = serialize_strs(&write_buffer, msg, 1);
    free(msg);
//    printf("Message to send and size %s - %d\n", write_buffer, value_len);
    send_helper(master_fd, write_buffer, value_len);
    free(write_buffer);
    freeaddrinfo(servinfo); // all done with this structure

    char read_buffer[200];
    ssize_t nbytes = recv(master_fd, read_buffer, strlen(pingMessage), 0);
    if(nbytes<=0)
        return -1;
    assert(strncmp(read_buffer, pingMessage, nbytes)==0);
    msg = malloc(3*sizeof (char*));
    msg[0]=REPLCONF;
    msg[1]=LISTENING_PORT;
    msg[2]=malloc(10);
    sprintf(msg[2], "%d", my_port);
    value_len = serialize_strs(&write_buffer, msg, 3);
    send_helper(master_fd, write_buffer, value_len);
    free(write_buffer);
    free(msg);
    nbytes = recv(master_fd, read_buffer, sizeof read_buffer, 0);
    if(nbytes<=0)
        return -1;
    assert(strncmp(read_buffer, ok, nbytes)==0);
    send_helper(master_fd, REPLCONF_MESSAGE_2, strlen(REPLCONF_MESSAGE_2));
    nbytes = recv(master_fd, read_buffer, sizeof read_buffer, 0);
    if(nbytes<=0)
        return -1;
    assert(strncmp(read_buffer, ok, nbytes)==0);
    send_helper(master_fd, HANDSHAKE_MESSAGE_3, strlen(HANDSHAKE_MESSAGE_3));
    nbytes = recv(master_fd, read_buffer, 55, 0);
    char c;
    do{
        recv(master_fd, &c, 1, 0);
    }
    while(c!='\n');
//    recv(master_fd, &c, 1, 0);
    recv(master_fd, &c, 1, 0);
    assert(c=='$');
    recv(master_fd, &c, 1, 0);
    int file_len=0;
    while(c!='\r'){
        file_len*=10;
        file_len+=c-'0';
        recv(master_fd, &c, 1, 0);
    }

    recv(master_fd, read_buffer, file_len+1, 0);
    return master_fd;

}

void send_to_replicas(char **commands, int commandLen){
    if(replica_of)
        return;
    char* writeBuffer;
    int value_len = serialize_strs(&writeBuffer, commands, commandLen);

    pthread_mutex_lock(&ack_mutex);
    replicas_acknowledged=0;
    pthread_mutex_unlock(&ack_mutex);

    for(int i=0;i<num_replicas; i++){
        send_helper(replicas_fd[i], writeBuffer, value_len);
        replicas_offset[i]+=value_len;
    }

    free(writeBuffer);
}

void send_ack_request(){
    for(int i=0;i<num_replicas; i++){
        replicas_offset[i]+=(int)strlen(GET_ACK_COMMAND);
        send_helper(replicas_fd[i], GET_ACK_COMMAND, strlen(GET_ACK_COMMAND));
    }
}

#endif
#if 0
int main()
{
    char addr_buf[64];
    struct addrinfo* feed_server = NULL;

    memset(addr_buf, 0, sizeof(addr_buf));

    getaddrinfo("localhost", NULL, NULL, &feed_server);
    struct addrinfo *res;
    for(res = feed_server; res != NULL; res = res->ai_next)
    {
        if ( res->ai_family == AF_INET )
        {
            inet_ntop(AF_INET, &((struct sockaddr_in *)res->ai_addr)->sin_addr, addr_buf, sizeof(addr_buf));
        }
        else
        {
            inet_ntop(AF_INET6, &((struct sockaddr_in6 *)res->ai_addr)->sin6_addr, addr_buf, sizeof(addr_buf));
        }

        printf("hostname: %s\n", addr_buf);
    }

    return 0;
}
#endif