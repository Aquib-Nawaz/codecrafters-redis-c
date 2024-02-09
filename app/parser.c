//
// Created by Aquib Nawaz on 08/02/24.
//

#include "parser.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

int parseString(FILE *fptr, char **ret){
    int isInt=0;
    unsigned char first;
    fread(&first, sizeof (unsigned char ), 1, fptr);
    unsigned int len=0;
    int val=0;
    switch(first>>6){
        unsigned char t;
        case 0:
            len |= first;
            break;
        case 1:
            len |= first&((1<<6)-1);
            len <<= 8;
            fread(&t, sizeof (unsigned char ), 1, fptr);
            len |= t;
            break;
        case 2:
            for(int i=0; i<3; i++){
                fread(&t, sizeof (unsigned char ), 1, fptr);
                len |= t;
                len <<= 8;
            }
            fread(&t, sizeof (unsigned char ), 1, fptr);
            len |= t;
            break;
        case 3:
            isInt = 1;
            switch(first&((1<<6)-1)){
                case 0:
//                    fread(&t, sizeof (unsigned char ), 1, fptr);
//                    val |= t;
                    len=SPECIAL_0;
                    break;
                case 1:
//                    fread(&t, sizeof (unsigned char ), 1, fptr);
//                    val |= t;
//                    val <<= 8;
//                    fread(&t, sizeof (unsigned char ), 1, fptr);
//                    val != t;
                    len=SPECIAL_1;
                    break;
                case 2:
//                    for(int i=0; i<3; i++){
//                        fread(&t, sizeof (unsigned char ), 1, fptr);
//                        val |= t;
//                        val <<= 8;
//                    }
//                    fread(&t, sizeof (unsigned char ), 1, fptr);
//                    val |= t;
                    len=SPECIAL_2;
                    break;
                default:
                    printf("Error\n");
            }
            break;
        default:
            printf("Error\n");
    }
    if(ret!=NULL) {
        *ret = malloc(len + 1);
        (*ret)[len] = '\0';
        fread(*ret, 1, len, fptr);
    }
    else{
        fseek(fptr,len,SEEK_CUR);
    }
    return isInt;
}

void seekData(FILE *fptr){

    char *ret;
    char redis[6];
    fread(redis, 1, MAGIC_SIZE, fptr);
    assert(strcmp(redis, REDIS)==0);
    fseek(fptr, 4, SEEK_CUR); //version
    unsigned char c;
    fread(&c, 1, 1, fptr);
    while(c==(unsigned char )AUX_FIELD){
        parseString(fptr, NULL);
        parseString(fptr, NULL);
//        printf("%s->", ret);
//        free(ret);
//        if(parseString(fptr, &ret)){
//            long i;
//            memcpy(&i, ret, strlen(ret));
//            printf("%li", i);
//        }
//        else{
//            printf("%s ", ret);
//        }
//        printf("\n");
//        free(ret);
        fread(&c, 1, 1, fptr);
    }
    assert(c==(unsigned char )DB_SELECTOR);
}

void getData(char *** ret, FILE *fptr, int *len){
    unsigned char c;
    unsigned char lengthData[4];
    *len=0;
    fread(lengthData, 1, 4, fptr);
    memcpy(len, lengthData+2, 2);
    *len *= 2;
    printf("Length:- %d\n", *len);
    (*ret) = (char **)calloc(*len, sizeof (char*));
    fread(&c, 1, 1, fptr);

    int arrayIdx = 0;

    while(c!=(unsigned char)FILE_END){
        printf("New Keyword\n");
        assert(c==0);
        if(c==0){
            parseString(fptr, &ret[0][arrayIdx++]);
            parseString(fptr, &ret[0][arrayIdx++]);
        }
        fread(&c, 1, 1, fptr);
    }
    fclose(fptr);
}

#if 0
int main(){
    setbuf(stdout, NULL);
    int len;
    char ** data;
    FILE *fptr;
    if(!(fptr = fopen("dump.rdb", "rb"))){

        perror("fopen\n");
        exit(1);
    }
    seekData(fptr);
    getData(&data, fptr, &len);
    assert(len == 4);
    for(int i=0;i<len-1; i+=2){
        printf("%s->%s\n", data[i], data[i+1]);
    }
}
#endif