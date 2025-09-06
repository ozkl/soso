/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2017, ozkl
 * All rights reserved.
 *
 * This file is licensed under the BSD 2-Clause License.
 * See the LICENSE file in the project root for full license information.
 */

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
