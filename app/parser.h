//
// Created by Aquib Nawaz on 08/02/24.
//

#ifndef REDIS_PARSER_H
#define REDIS_PARSER_H

#include <stdio.h>

#define MAGIC_SIZE 5
#define VERSION_SIZE 4
#define AUX_FIELD '\xfa'
#define DB_SELECTOR '\xfe'
#define RESIZE_DB '\xfb'
#define EXPIRY_S '\xfd'
#define UNSIGNED_INT_SIZE 4
#define EXPIRY_MS '\xfc'
#define VALUE_TYPE_SIZE 1
#define FILE_END '\xff'

#define REDIS "REDIS"

#define LENGTH_0 0
#define LENGTH_1 1
#define LENGTH_2 4

#define SPECIAL_0 1
#define SPECIAL_1 2
#define SPECIAL_2 4
// 5+4+(fa+str+str)*+(fe+str+fb+str+str)

void seekData(FILE *);
void getData(char *** ret, FILE *fptr, int *len);

#endif //REDIS_PARSER_H
