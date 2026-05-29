#pragma once

#define DISALLOW_DUPLICATE 0
#define ALLOW_DUPLICATE 1

struct HashNode;
typedef struct HashNode {
    short x, y;
    void* data;
    struct HashNode* next;
} HashNode;

typedef struct {
    short x, y;
    void* data;
} HashPair;

typedef struct {
    int size;
    int num_items;
    HashNode** array;

    int allow_duplicates;
} HashTable;

int hashTableCreate(HashTable* hash_table, int size, int allow_duplicates);
int hashTableHas(const HashTable* hash_table, short x, short y);
int hashTablePut(HashTable* hash_table, short x, short y, void* data);
int hashTableGet(const HashTable* hash_table, short x, short y, void** data);
int hashTableGetAll(const HashTable* hash_table, short x, short y, void*** data, int* len_data);
int hashTableGetPairs(const HashTable* hash_table, HashPair** data, int* len_data);
int hashTableFreeAndDelete(HashTable* hash_table);
int hashTableDelete(HashTable* hash_table);