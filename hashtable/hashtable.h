#pragma once
#include <cstdint>
#include <cstdlib>

struct node{
    node* next;
    int64_t hash_value;
};

using node_eq_fn = bool (*)(node*, node*);

struct table{
    node** arr = nullptr;
    int64_t mask = 0;
    int32_t size = 0;
};

struct Map{
    table* older = nullptr;
    table* newer = nullptr;
    int migrate_index = 0;
    bool rehashing = false;
};

void insert(Map* map, node* node);
size_t size(Map *map);
void clear_table(table* tab);
void clear_map(Map* map);
node *delete_node(Map *map, node *node, node_eq_fn equal);
node *lookup(Map *map, node *node, node_eq_fn equal);