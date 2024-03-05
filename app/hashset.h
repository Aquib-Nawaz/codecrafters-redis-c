//
// Created by Aquib Nawaz on 06/02/24.
//

#ifndef REDIS_HASHSET_H
#define REDIS_HASHSET_H

#include <stdint.h>
#include <stddef.h>
#include <sys/time.h>
#include <stdbool.h>

enum EntryType{
    ENTRY_STR,
    ENTRY_STREAM
};

struct HNode{
    struct HNode* next;
    uint64_t hcode;
    enum EntryType type;
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

struct StreamData{
    char* id;
    char **keys;
    char ** values;
    int len;
    struct StreamData* prev;
};

struct Entry_Str{
    struct HNode node;
    char* key;
    char* value;
    struct timeval expiry;
};

struct Entry_Stream{
    struct HNode node;
    char* key;
    struct timeval expiry;
    int len;
    struct StreamData* data;
};

typedef struct {
  char* key;
  union {
    struct Entry_Str* str_value;
    struct Entry_Stream* stream_value;
  };
} Entry;

typedef struct {
    char* key;
    uint64_t hcode;
} SearchKey;

#define container_of(ptr, type, member) ({                  \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
    (type *)( (char *)__mptr - offsetof(type, member) );})

#define nil "$-1\r\n"



void set_expiry(struct timeval* tm, long ms);
void do_set (char **commands, int commandLen, struct HMap* hmap);
char* do_get(char** commands, int commandLen, struct HMap* hmap);

int entry_eq(struct HNode *lhs, SearchKey *rhs);
void delete_entry(void* entry, int);

unsigned long hash(char *str);
struct HNode *hm_lookup(struct HMap *hmap, SearchKey *key, int (*eq)(struct HNode *, SearchKey*));
void hm_insert(struct HMap *hmap, struct HNode *node);
struct HNode *hm_pop(struct HMap *hmap, SearchKey *key, int  (*eq)(struct HNode *, SearchKey*));
size_t hm_size(struct HMap *hmap);
void hm_destroy(struct HMap *hmap);
int check_expired(struct timeval *time);
int hm_scan(char ***, struct HMap *);
bool entry_expired(struct HNode **node, struct HMap*, struct HTab*, Entry*, int flags);

struct Entry_Str* get_string_container(struct HNode* _node);
struct Entry_Stream* get_stream_container(struct HNode* _node);

#endif //REDIS_HASHSET_H
