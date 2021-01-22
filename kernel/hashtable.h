#ifndef HASHTABLE_H
#define HASHTABLE_H

#include "common.h"

typedef struct HashTable HashTable;

HashTable* hashtable_create(uint32 capacity);
void hashtable_destroy(HashTable* hashtable);
BOOL hashtable_search(HashTable* hashtable, uint32 key, uint32* value);
BOOL hashtable_insert(HashTable* hashtable, uint32 key, uint32 data);
BOOL hashtable_remove(HashTable* hashtable, uint32 key);

#endif // HASHTABLE_H
