#include "hashtable.h"

// compute raw hash based on the raw bytes
size_t hash(void* key, size_t length){
    uint8_t* key_bytes = (uint8_t*) key;
    size_t result = 5381;
    int c = *key_bytes;

    for(size_t i=0; i<length; i++){
        result = ((result << 5) + result) + c;
        key_bytes++;
        c = *key_bytes;
    }
    return result;
}


// find the end of the string, looking for the '\0' character
size_t hash_string_find_length(char* str){
    size_t length=0;
    while (str[length] != '\0'){
        length++;
    }
    assert(length > 0);
    assert(length < 128);
    return length;
}


// hash function that takes argument of char* type
size_t hash_string(char* str, size_t length){
    return hash((void*) str, length);
}


// create data structures and allocate memory for them
HashTable* hash_table_create(size_t size, uint64_t flags){
    HashTable* ht;
    ht = (HashTable*) malloc( sizeof(HashTable) );
    ht->size = size;
    ht->count = 0;
    ht->flags = flags;

    HashTableItem* data = (HashTableItem*) malloc( size * sizeof(HashTableItem) );
    ht->data = data;
    for(size_t i=0; i<size; i++){
        ht->data[i].value = NULL;
    }
    
    // look for this key to indicate deleted items
    ht->deleted = (char*) malloc(1);
    ht->deleted[0] = '\0';
    return ht;
}


// look for first available hash slot that has NULL value (including using deleted entries)
// TODO how to handle if the same object is added twice with different keys?
int hash_table_insert(HashTable* ht, char* str, void* data){
    // find index section but allowing (stopping at) deleted values
    size_t string_length = hash_string_find_length(str);
    size_t index = hash_string(str, string_length) % ht->size;

    // move past entries which are being actively used
    while (ht->data[index].value != NULL){
      if (strcmp(str, ht->data[index].key) == 0){
        break;
      }
      index = (index + 1) % ht->size;
    }

    int result;
    // now do the insertion work if a new entry
    if (ht->data[index].value == NULL){
        // allocate memory to store the key string long term
        char* key = (char*) malloc( (string_length) * sizeof(char));
        strcpy(key, str);

        // now store information into the hash table
        ht->data[index].value = data;
        ht->data[index].key = key;
        ht->count++;
        result = EXIT_SUCCESS;
    }
    // or some work if not a new entry
    else{
        printf(" index %ld (key: %s) already points to %p\n", index, ht->data[index].key, ht->data[index].value);
        // printf(" comparing keys: %s to %s\n", str, ht->data[index].key);
        
        if (strcmp(str, ht->data[index].key) == 0){
            printf("[WARNING] the requested key is the same, updating the data, freeing the old one\n");
            free(ht->data[index].value);
            ht->data[index].value = data;
            result = EXIT_SUCCESS;
        }
        else{
            result = hash_table_insert_collision(ht, str, data, index);
        }
    }
    return result;
}


// search in the table forwards until a place to put the data is found
// OK to place over deleted data
int hash_table_insert_collision(HashTable* ht, char* str, void* data, size_t index_start){
    printf("Running collision insert function.\n");
    size_t index = index_start;
    int found = 0;
    while (found==0){
        index = (index + 1) % ht->size;
        if (ht->data[index].value == NULL){
            size_t string_length = hash_string_find_length(str);
            char* key = (char*) malloc ((string_length) * sizeof(char));
            strcpy(key, str);

            ht->data[index].value = data;
            ht->data[index].key = str;
            ht->count++;
            return EXIT_SUCCESS;
        }
        if (index == index_start){
            break;
        }
    }
    return EXIT_FAILURE;
}

// find the index of key in the hashtable
// continue to search past deleted entries
size_t hash_table_get_index(HashTable* ht, char* key){
    size_t length = hash_string_find_length(key);
    size_t index = hash_string(key, length) % ht->size;

    // keep looking through the table until the keys match or the table is empty 
    // look past deleted cells as needed
    while ((ht->data[index].value != NULL) || (ht->data[index].key == ht->deleted)){
      if (strcmp(key, ht->data[index].key) == 0){
        break;
      }
      index = (index + 1) % ht->size;
    }
    return index;
}


// return a pointer to the object stored at key. Return null if no data is present
// continue search past deleted entries; find the key at all costs!
void* hash_table_get(HashTable* ht, char* key){
    size_t index = hash_table_get_index(ht, key);
    void* result = ht->data[index].value;
   return result;
}


// search for the key, continuing past deleted entries and remove it
void hash_table_remove(HashTable* ht, char* key){
    size_t index = hash_table_get_index(ht, key);
    assert(ht->data[index].value != NULL); // verify the entry is not already deleted

    if ((ht->flags & HT_FREE_DATA) > 0){
        free(ht->data[index].value);
    }
    if ((ht->flags & HT_FREE_KEY) > 0){
        free(ht->data[index].key);
    }
 
    ht->data[index].value = NULL;
    ht->data[index].key = ht->deleted;
    ht->count--;
}


// remove any objects remaining in the table
void hash_table_destroy(HashTable* ht){
    for( size_t i=0; i<ht->size; i++){
        if (ht->data[i].value != NULL){
            if ((ht->flags & HT_FREE_DATA) > 0){
              free(ht->data[i].value);
            }
            if ((ht->flags & HT_FREE_KEY) > 0){
              free(ht->data[i].key);
            }
        }
    }

    // remove the table itself
    free(ht->deleted);
    free(ht->data);
    free(ht);
}


void hash_table_print(HashTable* ht){
    printf("contents of hashtable index:\n");
    for(size_t i=0; i<ht->size; i++){
        if (ht->data[i].value != NULL){
            printf("\t%ld\t%s\t=\t%p\n", i, ht->data[i].key, ht->data[i].value);
        }
    }
}
