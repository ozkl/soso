#include "hashtable.h"
#include "alloc.h"

typedef struct DataItem
{
   uint32_t data;
   uint32_t key;
   uint8_t used;
} DataItem;

typedef struct HashTable
{
   DataItem* items;
   uint32_t capacity;
} HashTable;

static uint32_t hash_code(HashTable* hashtable, uint32_t key)
{
   return key % hashtable->capacity;
}

HashTable* hashtable_create(uint32_t capacity)
{
    HashTable* hashtable = kmalloc(sizeof(HashTable));
    memset((uint8_t*)hashtable, 0, sizeof(HashTable));
    hashtable->capacity = capacity;
    hashtable->items = kmalloc(sizeof(DataItem) * capacity);

    return hashtable;
}

void hashtable_destroy(HashTable* hashtable)
{
    kfree(hashtable->items);
    kfree(hashtable);
}

DataItem* HashTable_search_internal(HashTable* hashtable, uint32_t key)
{
   //get the hash
   uint32_t hash_index = hash_code(hashtable, key);

   uint32_t counter = 0;
   while(counter < hashtable->capacity)
   {
      if(hashtable->items[hash_index].key == key)
      {
          if(hashtable->items[hash_index].used == TRUE)
          {
              return &(hashtable->items[hash_index]);
          }
      }

      //go to next cell
      ++hash_index;

      //wrap around the table
      hash_index %= hashtable->capacity;

      ++counter;
   }

   return NULL;
}

BOOL hashtable_search(HashTable* hashtable, uint32_t key, uint32_t* value)
{
    DataItem* existing = HashTable_search_internal(hashtable, key);

    if (existing)
    {
        *value = existing->data;

        return TRUE;
    }

    return FALSE;
}

BOOL hashtable_insert(HashTable* hashtable, uint32_t key, uint32_t data)
{
    DataItem* existing = HashTable_search_internal(hashtable, key);

    if (existing)
    {
        existing->data = data;

        return TRUE;
    }

    //get the hash
    uint32_t hash_index = hash_code(hashtable, key);

    uint32_t counter = 0;
    //move in array until an empty or deleted cell
    while(counter < hashtable->capacity)
    {
        if (hashtable->items[hash_index].used == FALSE)
        {
            hashtable->items[hash_index].key = key;
            hashtable->items[hash_index].data = data;
            hashtable->items[hash_index].used = TRUE;

            return TRUE;
        }


        //go to next cell
        ++hash_index;

        //wrap around the table
        hash_index %= hashtable->capacity;

        ++counter;
    }

    return FALSE;
}

BOOL hashtable_remove(HashTable* hashtable, uint32_t key)
{
    DataItem* existing = HashTable_search_internal(hashtable, key);

    if (existing)
    {
        existing->used = FALSE;

        return TRUE;
    }

    return FALSE;
}
