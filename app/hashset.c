//
// Created by Aquib Nawaz on 06/02/24.
//
#include "hashset.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>

static void h_init(struct HTab *htab, size_t n) {
    assert(n>0 && (n&(n-1))==0);
    htab->tab = (struct HNode **)calloc(n,sizeof (struct HNode *));
    htab->size = 0;
    htab->mask = n-1;
}

static void h_insert(struct HTab *htab, struct HNode *node) {
    int idx = node->hcode & htab->mask;
    struct HNode * first = htab->tab[idx];
    node->next = first;
    htab->tab[idx] = node;
    htab->size ++;
}

static struct HNode **h_lookup(struct HTab *htab, struct HNode *key, int (*eq)(struct HNode *, struct HNode *)) {
    if(!htab->tab){
        return NULL;
    }
    int idx = key->hcode & htab->mask;
    struct HNode ** from = &htab->tab[idx];
    for(struct HNode*cur; (cur=*from)!=NULL; from = &cur->next){
        if(cur->hcode == key->hcode && eq(cur, key)) {
            return from;
        }
    }
    return NULL;
}

static struct HNode *h_detach(struct HTab *htab, struct HNode **from) {
    htab->size--;
    struct HNode * ret = *from;
    //This operation will directly change the node's parent's next value
    *from = (*from)->next;
    return ret;
}

const size_t k_resizing_work = 128; // constant work

static void hm_help_resizing(struct HMap *hmap) {
    size_t nwork = 0;
    while (nwork<k_resizing_work && hmap->t2.size>0){

        struct HNode **from = &hmap->t2.tab[hmap->resizing_pos];

        if (*from == NULL){
            hmap->resizing_pos++;
            continue;
        }

        h_insert(&hmap->t1,h_detach(&hmap->t2, from));
        nwork++;
    }

    if (hmap->t2.size == 0 && hmap->t2.tab) {
        // free calloc-ated memory
        free(hmap->t2.tab);
        hmap->t2 = (const struct HTab){ };
    }

}

static void hm_start_resizing(struct HMap *hmap) {
    //No Data in tab2 else it will get lost
    assert(hmap->t2.tab==NULL);
    hmap->t2.tab = hmap->t1.tab;
    h_init(&hmap->t1, hmap->t1.size*2);
    hmap->resizing_pos = 0;
}

struct HNode *hm_lookup(struct HMap *hmap, struct HNode *key, int (*eq)(struct HNode *, struct HNode *)){
    struct HNode **from = h_lookup(&hmap->t1, key, eq);
    if(from != NULL){
        return *from;
    }
    from = h_lookup(&hmap->t2, key, eq);
    return from ? *from:NULL;
}

const size_t k_max_load_factor = 8;

void hm_insert(struct HMap *hmap, struct HNode *node){
    if (!hmap->t1.tab) {
        h_init(&hmap->t1, 4);
    }
    h_insert(&hmap->t1, node);
    if (!hmap->t2.tab) {
        //average number of nodes for each position
        size_t load_factor = hmap->t1.size / (hmap->t1.mask + 1);
        if(load_factor >= k_max_load_factor){
            hm_start_resizing(hmap);
        }
    }
    hm_help_resizing(hmap);
}

struct HNode *hm_pop(struct HMap *hmap, struct HNode *key, int  (*eq)(struct HNode *, struct HNode *)){
    struct HNode **from = h_lookup(&hmap->t1,key, eq);
    if(from){
        return h_detach(&hmap->t1, from);
    }
    from = h_lookup(&hmap->t2,key, eq);
    if(from){
        return h_detach(&hmap->t2, from);
    }
    return NULL;
}

size_t hm_size(struct HMap *hmap){
    return hmap->t1.size + hmap->t2.size;
}

void hm_destroy(struct HMap *hmap){
    free(hmap->t1.tab);
    free(hmap->t2.tab);
    *hmap = (struct HMap){};
}
unsigned long hash( char *str)
{
    unsigned long hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

int serialize_str(char **writeBuffer, char* str){
    size_t str_len = strlen(str);
    *writeBuffer = calloc(str_len+3, sizeof (char ));
    (*writeBuffer)[0] = '+';
    strcpy(*writeBuffer+1, str);
    strcpy(*writeBuffer+1+str_len, "\r\n");
    return str_len+3;
}
//struct Entry* entry_init(char* key, char* value){
//    struct Entry *entry = malloc(sizeof (struct Entry));
//    if(!entry)
//        return NULL;
//    entry->key = key;
//    entry->value = value;
//    entry->node->hcode = hash(key);
//    return entry;
//}



