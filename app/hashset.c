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
    if (hmap->t2.tab == NULL) {
        return;
    }
    size_t nwork = 0;
    while (nwork<k_resizing_work && hmap->t2.size>0){

        struct HNode **from = &hmap->t2.tab[hmap->resizing_pos];

        if (*from == NULL){
            hmap->resizing_pos++;
            continue;
        }
        struct HNode *detached_node = h_detach(&hmap->t2, from);
        struct Entry * entry = container_of( detached_node, struct Entry,node);
        //check if expiry is set and is expired
        if(entry->expiry!=0 && entry->expiry<time(NULL)){
            delete_entry(entry);
        }
        else {
            h_insert(&hmap->t1, detached_node);
        }
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
    *hmap = (struct HMap){0};
}
unsigned long hash( char *str)
{
    unsigned long hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

int entry_eq(struct HNode *lhs, struct HNode *rhs) {
    struct Entry *le = container_of(lhs, struct Entry, node);
    struct Entry *re = container_of(rhs, struct Entry, node);
    return strcmp(le->key, re->key)==0;
}

void delete_entry(struct Entry* entry){
    free(entry->value);
    free(entry->key);
    free(entry);
}
#if 0

void do_set (char **commands, int commandLen){
	if(commandLen<3)
		return;
	struct Entry keyEntry;
	char* key = commands[1], *value = commands[2];
	keyEntry.node.hcode = hash(key);
	keyEntry.key=key;
	struct HNode* node = hm_lookup(&g_data.db, &keyEntry.node, entry_eq);
	if(node){
		struct Entry * existing = container_of(node, struct Entry, node);
		free(existing->value);
		existing->value = malloc(strlen(value)+1);
		strcpy(existing->value, value);
		return;
	}
	struct Entry *entry = calloc(1, sizeof (struct Entry));
	entry->key = malloc(strlen(key)+1);
	entry->value = malloc(strlen(value)+1);
	strcpy(entry->key, key);
	strcpy(entry->value, value);
	entry->node.hcode = hash(key);
	entry->node.next = NULL;
	hm_insert(&g_data.db, &entry->node);
}

char* do_get(char** commands, int commandLen){
	if(commandLen<2)
		return NULL;
	struct Entry key;
	key.node.hcode = hash(commands[1]);
	key.key = commands[1];
	struct HNode* node = hm_lookup(&g_data.db, &key.node, entry_eq);
	if(!node)
		return "nil";
	return container_of(node, struct Entry, node)->value;
}


int main(void){
    char * commands[] = {"set", "randomkey", "randomvalue"};
    do_set(commands, 3);
    assert(hm_size(&g_data.db)==1);
    char** commands2 = (char* []) {"get", "randomkey"};
    char* ret = do_get(commands2, 2);
    assert(strcmp(commands[2], ret)==0);
    char** commands3 = (char* []) {"set", "randomkey", "random"};
    do_set(commands3, 3);
    assert(hm_size(&g_data.db)==1);

}
#endif


