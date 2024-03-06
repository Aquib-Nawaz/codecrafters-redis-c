//
// Created by Aquib Nawaz on 05/03/24.
//

#ifndef CODECRAFTERS_REDIS_C_STREAMS_H
#define CODECRAFTERS_REDIS_C_STREAMS_H

#include "hashset.h"

#define ID_GREATER_THAN_ERR "-ERR The ID specified in XADD is equal or smaller than the target stream top item\r\n"
#define ID_00_ERROR "-ERR The ID specified in XADD must be greater than 0-0\r\n"

void type_command(int,char*,struct HMap*);
void xadd_command(int, char**, int, struct HMap*);
void xrange_command(int, char**, int, struct HMap*);

#endif //CODECRAFTERS_REDIS_C_STREAMS_H
