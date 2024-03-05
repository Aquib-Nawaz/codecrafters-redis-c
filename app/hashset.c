//
// Created by Aquib Nawaz on 06/02/24.
//
#include "hashset.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>

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

static struct HNode **h_lookup(struct HTab *htab, SearchKey *key, int (*eq)(struct HNode *, SearchKey *)) {
    if(!htab->tab){
        return NULL;
    }
    int idx = key->hcode & htab->mask;
    struct HNode ** from = &htab->tab[idx];
    for(struct HNode*cur; (cur=*from)!=NULL; from = &cur->next){
        if(cur->hcode == key->hcode && eq(cur, key)) {
            if(entry_expired(from, NULL, htab, NULL, 5))
                return NULL;
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
        if(!entry_expired(&detached_node, NULL, NULL, NULL, 1))
            h_insert(&hmap->t1, detached_node);
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

struct HNode *hm_lookup(struct HMap *hmap, SearchKey *key, int (*eq)(struct HNode *, SearchKey *)){
    hm_help_resizing(hmap);
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

struct HNode *hm_pop(struct HMap *hmap, SearchKey *key, int  (*eq)(struct HNode *, SearchKey *)){
    hm_help_resizing(hmap);
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

    while ((c = (unsigned char )*str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

int entry_eq(struct HNode *lhs, SearchKey *rhs) {
    int node_type = lhs->type;
    char* lkey, *rkey=rhs->key;
    if(node_type==ENTRY_STR) {
        lkey = get_string_container(lhs)->key;
    }
    else if(node_type==ENTRY_STREAM){
        lkey = get_stream_container(lhs)->key;
    }
    else
        return 1;
    return strcmp(lkey, rkey)==0;
}

void delete_str(struct Entry_Str* entry){
    free(entry->value);
    free(entry->key);
    free(entry);
}

void delete_stream_data(struct StreamData* data){
    int i;
    for(i=0; i<data->len; i++){
        free(data->keys[i]);
        free(data->values[i]);
    }
    free(data->keys);
    free(data->values);
    free(data->id);
    free(data);
}

void delete_stream(struct Entry_Stream* entry){
    free(entry->key);
    struct StreamData* current = entry->data;
    while(current){
        struct StreamData* prev = current->prev;
        delete_stream_data(current);
        current = prev;
    }
    free(entry);
}

void delete_entry(void * entry, int type){
    if(type==ENTRY_STR)
        delete_str((struct Entry_Str*)entry);
    else if(type==ENTRY_STREAM)
        delete_stream((struct Entry_Stream*)entry);
}

int check_expired(struct timeval *expiry){
    struct timeval curr_time;
    gettimeofday(&curr_time, NULL);
    if(curr_time.tv_sec > expiry->tv_sec)
        return 1;
    if(curr_time.tv_sec == expiry->tv_sec)
        return curr_time.tv_usec > expiry->tv_usec;
    return 0;
}

int h_scan(char **ret, struct HTab* htab){
    if(htab->size==0 || htab->tab==NULL)
        return 0;
    int idx=0;

    Entry entry;
    for(int i=0; i<=htab->mask; i++) {
        struct HNode **from = &htab->tab[i];
        for (struct HNode *cur; (cur = *from) != NULL; from = &cur->next) {

        if(!entry_expired(from, NULL, htab, &entry, 5))
            {
                ret[idx++] = entry.key;
            }
        }
    }
    assert(idx==htab->size);
    return idx;
}

int hm_scan(char ***ret, struct HMap *hmap){
    int size = (int)hm_size(hmap);
    (*ret) = (char **)calloc(size, sizeof (char *));
    int s1 = h_scan(*ret, &hmap->t1);
    s1 += h_scan(*ret+s1, &hmap->t2);
    assert(s1<=size);
    return s1;

}


void set_expiry(struct timeval* tm, long ms){
	gettimeofday(tm,NULL);
//	printf("Setting Expiry of %li\n", ms);
	tm->tv_sec += ms/1000;
	tm->tv_usec += (ms%1000)*1000;
	if (tm->tv_usec >= 1000000) {
		tm->tv_usec -= 1000000;
		tm->tv_sec++;
	}
}
//I have migrated it here
//Should entry have its own module! I think so
//I will decouple entry hnode then
void do_set (char **commands, int commandLen, struct HMap* hmap){
	if(commandLen<3)
		return;
	char* key = commands[1], *value = commands[2];
    SearchKey keyEntry;
    keyEntry.hcode = hash(key);
	keyEntry.key=key;


	struct HNode* node = hm_lookup(hmap,  &keyEntry, entry_eq);
	if(node){
		struct Entry_Str * existing = get_string_container(node);
		free(existing->value);
		existing->value = malloc(strlen(value)+1);
		strcpy(existing->value, value);
		if(commandLen==5){
			long ms = strtol(commands[4],NULL, 10);
			set_expiry(&existing->expiry, ms);
		}
		return;
	}
	struct Entry_Str *entry = calloc(1, sizeof (struct Entry_Str));
	entry->key = malloc(strlen(key)+1);
	entry->value = malloc(strlen(value)+1);
	strcpy(entry->key, key);
	strcpy(entry->value, value);
	entry->node.hcode = hash(key);
	entry->node.next = NULL;
	entry->expiry.tv_sec=0;
	if(commandLen==5){
		long ms = strtol(commands[4],NULL, 10);
		set_expiry(&entry->expiry, ms);
	}
	hm_insert(hmap, &entry->node);
}

char* do_get(char** commands, int commandLen, struct HMap* hmap){
	if(commandLen<2)
		return NULL;

    SearchKey keyEntry;
    keyEntry.hcode = hash(commands[1]);
    keyEntry.key=commands[1];
	struct HNode* node = hm_lookup(hmap, &keyEntry, entry_eq);
	if(!node){
		return nil;
	}
	return get_string_container(node)->value;
}

bool entry_expired(struct HNode **node, struct HMap* map, struct HTab* tab, Entry* ret_entry ,int flags){
    struct timeval expiry;
    bool ret;
    void* entry;
    int node_type = (*node)->type;
    SearchKey key;
    key.hcode = (*node)->hcode;
    if(node_type==ENTRY_STR) {
        entry = get_string_container(*node);
        expiry = ((struct Entry_Str*)entry)->expiry;
        key.key =((struct Entry_Str*)entry)->key;
    }
    else if(node_type==ENTRY_STREAM){
        entry = get_stream_container(*node);
        expiry = ((struct Entry_Stream*)entry)->expiry;
        key.key =((struct Entry_Stream*)entry)->key;
    }
    ret = expiry.tv_sec != 0 && check_expired(&expiry);

    if(ret){
        if(flags&4)
            h_detach(tab, node);

        if(flags&2) {
            hm_pop(map, &key, entry_eq);
        }
        if(flags&1)
            delete_entry(entry, node_type);
    }
    else if(ret_entry!=NULL){
        ret_entry->key = key.key;
        if(node_type==ENTRY_STR) {
            ret_entry->str_value = (struct Entry_Str*)entry;
        }
        else if(node_type==ENTRY_STREAM){
            ret_entry->stream_value = (struct Entry_Stream*)entry;
        }
    }
    return ret;
}

struct Entry_Str* get_string_container(struct HNode* _node){
    return (struct Entry_Str*)container_of(_node, struct Entry_Str, node);
}

struct Entry_Stream* get_stream_container(struct HNode* _node){
    return (struct Entry_Stream*)container_of(_node, struct Entry_Stream, node);
}



#if 0
struct {
    struct HMap db;
} g_data;
int main(void){


    char * commands[] = {"set", "randomkey", "randomvalue", "px", "1000"};
    do_set(commands, 5 , &g_data.db);
    assert(hm_size(&g_data.db)==1);
    char** commands2 = (char* []) {"get", "randomkey"};
    char* ret = do_get(commands2, 2, &g_data.db);
    assert(strcmp(ret, commands[2])==0);
    sleep(1);
    ret = do_get(commands2, 2, &g_data.db);
    assert(strcmp(nil, ret)==0);
    char** commands3 = (char* []) {"set", "randomkey", "random"};
    do_set(commands3, 3, &g_data.db);
    assert(hm_size(&g_data.db)==1);

}
#endif


