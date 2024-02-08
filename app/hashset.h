//
// Created by Aquib Nawaz on 06/02/24.
//

#ifndef REDIS_HASHSET_H
#define REDIS_HASHSET_H

#include <stdint.h>
#include <stddef.h>
#include <sys/time.h>

struct HNode{
    struct HNode* next;
    uint64_t hcode;
};

struct HTab{
    struct HNode ** tab;
    size_t mask;
    size_t size;
};

struct HMap{
    struct HTab t1;
    struct HTab t2;
    size_t resizing_pos;
};
#define container_of(ptr, type, member) ({                  \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
    (type *)( (char *)__mptr - offsetof(type, member) );})

struct Entry{
    struct HNode node;
    char* key;
    char* value;
    struct timeval expiry;
};

int entry_eq(struct HNode *lhs, struct HNode *rhs);
void delete_entry(struct Entry* entry);

unsigned long hash(char *str);
struct HNode *hm_lookup(struct HMap *hmap, struct HNode *key, int (*eq)(struct HNode *, struct HNode *));
void hm_insert(struct HMap *hmap, struct HNode *node);
struct HNode *hm_pop(struct HMap *hmap, struct HNode *key, int  (*eq)(struct HNode *, struct HNode *));
size_t hm_size(struct HMap *hmap);
void hm_destroy(struct HMap *hmap);
int check_expired(struct timeval *time);
#endif //REDIS_HASHSET_H
