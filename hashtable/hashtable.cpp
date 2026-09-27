#include "hashtable.h"

#include <cstdlib>

#define init_size 64
#define Max_load_factor 8
#define once_rehashing 16

void init_table(table* &tab, int64_t new_sz){
    if(tab == nullptr || new_sz <= 0 || (new_sz & (new_sz - 1)) != 0)return;
    tab->arr = (node**)(calloc(new_sz, sizeof(node*)));
    if(tab->arr == nullptr)return;
    tab->mask = new_sz-1;
    tab->size = 0;
}

void clear_table(table* tab){
    if(tab == nullptr)return;
    if(tab->arr != nullptr){
        for(int64_t i = 0; i <= tab->mask; i++){
            std::free(tab->arr[i]); // Bucket heads are sentinels; linked items are caller-owned.
        }
        std::free(tab->arr);
    }
    tab->arr = nullptr;
    tab->mask = 0;
    tab->size = 0;
}

void clear_map(Map* map){
    if(map == nullptr)return;
    clear_table(map->older);
    if(map->newer != map->older)clear_table(map->newer);
    map->migrate_index = 0;
    map->rehashing = false;
}

void insert_in_table(table* &tab, node* item){
    if(tab == nullptr || item == nullptr)return;
    if(!tab->arr)init_table(tab, init_size);
    if(tab->arr == nullptr)return;
    int64_t pos = (tab->mask)&(item->hash_value);
    if(tab->arr[pos] == nullptr){
        tab->arr[pos] = (node*)(calloc(1, sizeof(node)));
        if(tab->arr[pos] == nullptr)return;
    }
    item->next = tab->arr[pos]->next;
    tab->arr[pos]->next = item;
    tab->size ++;
}

static node* find_previous(table* tab, node* query, node_eq_fn equal){
    if(tab == nullptr || tab->arr == nullptr || query == nullptr || equal == nullptr)return nullptr;
    int64_t pos = (tab->mask)&(query->hash_value);
    if(tab->arr[pos] == nullptr)return nullptr;
    node* ptr = tab->arr[pos]->next;
    node* prev = tab->arr[pos];
    while(ptr != nullptr){
        if(equal(ptr,query)){
            return prev;
        }
        prev = ptr;
        ptr = ptr->next;
    }
    return nullptr;
}

static node* delete_from_table(table* tab, node* query, node_eq_fn equal){
    node* prev = find_previous(tab, query, equal);
    if(prev == nullptr || prev->next == nullptr)return nullptr;
    node* removed = prev->next;
    prev->next = removed->next;
    removed->next = nullptr;
    --tab->size;
    return removed;
}

void rehash_once(Map* map){
    // move <= 16 buckets
    if(map == nullptr || map->older == nullptr || map->newer == nullptr ||
       map->older->arr == nullptr || map->newer->arr == nullptr)return;
    for(int16_t _ = 0; _ < once_rehashing; _++){
        if(map->migrate_index >= (map->older->mask + 1))break;
        node* bucket = map->older->arr[map->migrate_index];
        node* ptr = bucket == nullptr ? nullptr : bucket->next;
        while(ptr != nullptr){
            node* next = ptr->next;
            insert_in_table(map->newer, ptr); 
            ptr = next;
            map->older->size--;
        }
        if(bucket != nullptr)bucket->next = nullptr;
        map->migrate_index ++;
    }
    if(map->migrate_index == (map->older->mask + 1)){
        table* old_table = map->older;
        map->older = map->newer;
        map->newer = old_table;
        map->migrate_index = 0;
        map->rehashing = false;
    }
}

void trigger_rehash(Map* map){
    if(map == nullptr || map->older == nullptr || map->newer == nullptr ||
       map->newer == map->older || map->older->arr == nullptr)return;
    clear_table(map->newer);
    init_table(map->newer,(map->older->mask+1)*2);
    if(map->newer->arr == nullptr)return;
    map->rehashing = true;
    rehash_once(map);
}

void insert(Map* map, node* item){
    if(map == nullptr || map->older == nullptr || item == nullptr)return;
    if(map->rehashing){
        insert_in_table(map->newer, item);
        rehash_once(map);
    }
    else{
        insert_in_table(map->older, item);
    }
    if(!map->rehashing && map->older != nullptr){
        if(Max_load_factor*(map->older->mask+1) < map->older->size){
            trigger_rehash(map);
        }
    }
}

node *delete_node(Map *map, node *query, node_eq_fn equal){
    if(map == nullptr || query == nullptr || equal == nullptr)return nullptr;
    node* removed = delete_from_table(map->older, query, equal);
    return removed == nullptr ? delete_from_table(map->newer, query, equal) : removed;
}

node *lookup(Map *map, node *query, node_eq_fn equal){
    if(map == nullptr || query == nullptr || equal == nullptr)return nullptr;
    node* prev = find_previous(map->older, query, equal);
    if(prev == nullptr)prev = find_previous(map->newer, query, equal);
    return prev == nullptr ? nullptr : prev->next;
}

size_t size(Map *map){
    return (size_t)(map->older->size + map->newer->size);
}