#ifndef HASHTABLE_H
#define HASHTABLE_H

#include "common.h"

typedef struct HashTable HashTable;

HashTable* hashtable_create(uint32_t capacity);
void hashtable_destroy(HashTable* hashtable);
BOOL hashtable_search(HashTable* hashtable, uint32_t key, uint32_t* value);
BOOL hashtable_insert(HashTable* hashtable, uint32_t key, uint32_t data);
BOOL hashtable_remove(HashTable* hashtable, uint32_t key);

#endif // HASHTABLE_H
