#include "hash_table.h"
#include <stdlib.h>

int hashTableCreate(HashTable* hash_table, const int size, const int allow_duplicates) {
    hash_table->size = 0;
    hash_table->array = 0;
    hash_table->num_items = 0;
    if (allow_duplicates) hash_table->allow_duplicates = 1;
    else hash_table->allow_duplicates = 0;

    // ensure correct size
    if (size <= 0)
        return -1;

    // allocate initial internal array
    hash_table->array = calloc(size, sizeof(HashNode*));
    if (!hash_table->array)
        return -1;
    hash_table->size = size;

    return 0;
}

static int hashTableHash(const HashTable* hash_table, const short x, const short y) {
    // random primes
    return abs(x * 31 + y * 37) % hash_table->size;
}

static int hashTableTooFull(const HashTable* hash_table) {
    // maximum capacity ~= 75% full
    return hash_table->size * 3 < hash_table->num_items * 4;
}

static int hashTablePutNoRehash(HashTable* hash_table, const short x, const short y, void* data) {
    // mallocate hash node
    HashNode* hash_node = malloc(sizeof(HashNode));
    if (!hash_node)
        return -1;

    // write data into node
    hash_node->x = x;
    hash_node->y = y;
    hash_node->data = data;
    hash_node->next = 0;

    const int index = hashTableHash(hash_table, x, y);

    if (hash_table->array[index]) {
        // there is a collision
        HashNode* loop_node = hash_table->array[index];
        if (hash_table->allow_duplicates) {
            // if allowing duplicates, just jump to the end
            while (loop_node->next) {
                loop_node = loop_node->next;
            }
            loop_node->next = hash_node;
        } else {
            int exists = 0;
            HashNode** next = &hash_table->array[index];

            // if not, make sure this slot has not been taken.
            while (loop_node->next) {
                if (loop_node->x == x && loop_node->y == y) {
                    exists = 1;
                    break;
                }
                next = &loop_node->next;
                loop_node = loop_node->next;
            }
            if (loop_node->x == x && loop_node->y == y)
                exists = 1;

            if (exists) {
                hash_node->next = loop_node->next;
                free(loop_node);
                *next = hash_node;
                return 0;
            }
            loop_node->next = hash_node;
        }
    } else {
        // no collision, chuck it into the array
        hash_table->array[index] = hash_node;
    }

    // increase size
    hash_table->num_items += 1;

    return 0;
}

static int hashTableRehash(HashTable* hash_table) {
    // make copy of old array
    const int old_size = hash_table->size;
    HashNode** old_array = hash_table->array;

    // reallocate data for resized table
    HashNode** array = calloc(hash_table->size * 2, sizeof(HashNode*));
    if (!array)
        return -1;

    hash_table->array = array;
    hash_table->size *= 2;
    hash_table->num_items = 0;

    // copy over each item of the table
    for (int i = 0; i < old_size; i++) {
        HashNode* loop_node = old_array[i];

        if (!loop_node)
            continue;

        do {
            if (hashTablePutNoRehash(hash_table, loop_node->x, loop_node->y, loop_node->data)) {
                free(old_array);
                return -1;
            }
        } while ((loop_node = loop_node->next));
    }

    // free old array
    free(old_array);

    return 0;
}

int hashTableHas(const HashTable* hash_table, const short x, const short y) {
    const int index = hashTableHash(hash_table, x, y);
    const HashNode* loop_node = hash_table->array[index];

    while (loop_node) {
        if (loop_node->x == x && loop_node->y == y)
            return 1;
        loop_node = loop_node->next;
    }

    return 0;
}

int hashTablePut(HashTable* hash_table, const short x, const short y, void* data) {
    // if hashtable too full, attempt to rehash
    if (hashTableTooFull(hash_table) && hashTableRehash(hash_table))
        return -1;

    // put new item in
    if (hashTablePutNoRehash(hash_table, x, y, data))
        return -1;

    return 0;
}

int hashTableGet(const HashTable* hash_table, const short x, const short y, void** data) {
    // get index
    const int index = hashTableHash(hash_table, x, y);
    HashNode* loop_node = hash_table->array[index];
    if (!loop_node)
        return 0;

    // loop through to find data
    do {
        if (loop_node->x == x && loop_node->y == y) {
            *data = loop_node->data;
            return 1;
        }
    } while ((loop_node = loop_node->next));

    // if not found, return 0
    return 0;
}

int hashTableGetAll(const HashTable* hash_table, const short x, const short y, void*** data, int* len_data) {
    const int index = hashTableHash(hash_table, x, y);
    const HashNode* loop_node = hash_table->array[index];

    // count number of elements
    *len_data = 0;
    while (loop_node) {
        if (loop_node->x == x && loop_node->y == y)
            *len_data += 1;
        loop_node = loop_node->next;
    }

    // if no data, dont bother continuing
    if (*len_data == 0) {
        *data = 0;
        return 0;
    }

    // mallocate
    *data = malloc(*len_data * sizeof(void*));
    if (!*data)
        return -1;

    // copy everything over
    loop_node = hash_table->array[index];
    int i = 0;
    while (loop_node) {
        if (loop_node->x == x && loop_node->y == y)
            (*data)[i++] = loop_node->data;
        loop_node = loop_node->next;
    }

    return 0;
}

int hashTableGetPairs(const HashTable* hash_table, HashPair** data, int* len_data) {
    *len_data = 0;
    *data = malloc(hash_table->num_items * sizeof(HashNode));
    if (!*data) {
        *data = 0;
        return -1;
    }

    for (int i = 0; i < hash_table->size; i++) {
        const HashNode* loop_node = hash_table->array[i];
        while (loop_node) {
            (*data)[*len_data].x = loop_node->x;
            (*data)[*len_data].y = loop_node->y;
            (*data)[*len_data].data = loop_node->data;
            *len_data += 1;
            loop_node = loop_node->next;
        }
    }

    return 0;
}

int hashTableFreeAndDelete(HashTable* hash_table) {
    if (!hash_table->array)
        return -1;

    // delete each item of the table
    for (int i = 0; i < hash_table->size; i++) {
        HashNode* loop_node = hash_table->array[i];

        if (!loop_node)
            continue;

        do {
            HashNode* next_node = loop_node->next;
            free(loop_node->data);
            free(loop_node);
            loop_node = next_node;
        } while (loop_node);
    }

    free(hash_table->array);
    hash_table->size = 0;
    hash_table->num_items = 0;
    hash_table->array = 0;

    return 0;
}

int hashTableDelete(HashTable* hash_table) {
    if (!hash_table->array)
        return -1;

    // delete each item of the table
    for (int i = 0; i < hash_table->size; i++) {
        HashNode* loop_node = hash_table->array[i];

        if (!loop_node)
            continue;

        do {
            HashNode* next_node = loop_node->next;
            free(loop_node);
            loop_node = next_node;
        } while (loop_node);
    }

    free(hash_table->array);
    hash_table->size = 0;
    hash_table->num_items = 0;
    hash_table->array = 0;

    return 0;
}