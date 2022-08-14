#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#define HT_FREE_DATA (0x1)
#define HT_FREE_KEY (0x1 << 1)

typedef struct {
    char* key;
    void* value;
} HashTableItem;

typedef struct {
    size_t size;
    size_t count;
    uint64_t flags;
    HashTableItem* data;
    char* deleted;
} HashTable;

size_t hash(void* key, size_t length);
size_t hash_string_find_length(char* str);
size_t hash_string(char* str, size_t length);
HashTable* hash_table_create(size_t size, uint64_t flags);
int hash_table_insert(HashTable* ht, char* str, void* data);
int hash_table_insert_collision(HashTable* ht, char* str, void* data, size_t index_start);
void* hash_table_get(HashTable* ht, char* key);
size_t hash_table_get_index(HashTable* ht, char* key);
void hash_table_remove(HashTable* ht, char* key);
void hash_table_destroy(HashTable* ht);
void hash_table_print(HashTable* ht);

#endif
