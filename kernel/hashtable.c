#include "hashtable.h"
#include "alloc.h"

typedef struct DataItem
{
   uint32 data;
   uint32 key;
   uint8 used;
} DataItem;

typedef struct HashTable
{
   DataItem* items;
   uint32 capacity;
} HashTable;

static uint32 hash_code(HashTable* hashtable, uint32 key)
{
   return key % hashtable->capacity;
}

HashTable* hashtable_create(uint32 capacity)
{
    HashTable* hashtable = kmalloc(sizeof(HashTable));
    memset((uint8*)hashtable, 0, sizeof(HashTable));
    hashtable->capacity = capacity;
    hashtable->items = kmalloc(sizeof(DataItem) * capacity);

    return hashtable;
}

void hashtable_destroy(HashTable* hashtable)
{
    kfree(hashtable->items);
    kfree(hashtable);
}

DataItem* HashTable_search_internal(HashTable* hashtable, uint32 key)
{
   //get the hash
   uint32 hash_index = hash_code(hashtable, key);

   uint32 counter = 0;
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

BOOL hashtable_search(HashTable* hashtable, uint32 key, uint32* value)
{
    DataItem* existing = HashTable_search_internal(hashtable, key);

    if (existing)
    {
        *value = existing->data;

        return TRUE;
    }

    return FALSE;
}

BOOL hashtable_insert(HashTable* hashtable, uint32 key, uint32 data)
{
    DataItem* existing = HashTable_search_internal(hashtable, key);

    if (existing)
    {
        existing->data = data;

        return TRUE;
    }

    //get the hash
    uint32 hash_index = hash_code(hashtable, key);

    uint32 counter = 0;
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

BOOL hashtable_remove(HashTable* hashtable, uint32 key)
{
    DataItem* existing = HashTable_search_internal(hashtable, key);

    if (existing)
    {
        existing->used = FALSE;

        return TRUE;
    }

    return FALSE;
}
