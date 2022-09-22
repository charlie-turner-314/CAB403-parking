#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hashtable.h"

// Maximum load before resizing
#define LOAD_FACTOR 0.75

typedef struct item
{
    char *key;
    int value;
    item_t *next;
} item_t;

typedef struct ht
{
    // pointers to items
    item_t **buckets;
    // Current number of items
    size_t size;
    // allocated capacity
    size_t capacity;
} ht_t;

bool htab_init(ht_t *h, size_t n)
{
    // Allocate memory for the buckets
    h->buckets = calloc(n, sizeof(item_t *));
    h->size = 0;     // no items yet
    h->capacity = n; // allocated for this many
    // if the memory allocation failed, return 0 (false), else return true
    if (h->buckets == NULL)
    {
        free(h);
        return false;
    }
    return true;
}

void htab_destroy(ht_t *h)
{
    // Free the memory allocated for each item
    for (size_t i = 0; i < h->size; i++)
    {
        item_t *current = h->buckets[i];
        while (current != NULL)
        {
            item_t *next = current->next;
            free(current->key);
            free(current);
            current = next;
        }
    }
    // Free the memory allocated for the buckets
    free(h->buckets);
    // Free the memory allocated for the hash table
    free(h);
}

size_t djb_hash(char *s)
{
    size_t hash = 5381;
    int c;
    while ((c = *s++) != '\0')
    {
        // hash = hash * 33 + c
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

size_t htab_index(ht_t *h, char *key)
{
    if (h->size == 0)
    {
        return 0;
    }
    return djb_hash(key) % h->size;
}

item_t *htab_bucket(ht_t *h, char *key)
{
    return h->buckets[htab_index(h, key)];
}

item_t *htab_find(ht_t *h, char *key)
{
    // get the bucket for the key
    item_t *item = htab_bucket(h, key);
    // go through each item in the bucket
    while (item != NULL)
    {
        if (strcmp(item->key, key) == 0)
        {
            return item;
        }
        item = item->next;
    }
    return NULL;
}

bool htab_set(ht_t *h, char *key, int value)
{
    // check if already there
    item_t *existing_item;
    if ((existing_item = htab_find(h, key)) != NULL)
    {
        // update the value
        existing_item->value = value;
        return true;
    }

    // ADDING NEW ITEM
    // check that adding the item (assuming into a new bucket) doesn't overfill the table
    if ((h->size + 1.0) / h->capacity >= LOAD_FACTOR)
    {
        // resize the table, if it fails, return false
        if (htab_resize(h) == false)
        {
            return false;
        }
    }

    // allocate memory for the new item
    item_t *new_item = malloc(sizeof(*new_item));
    // if the memory allocation failed, return false
    if (new_item == NULL)
    {
        free(new_item);
        return false;
    }
    // allocate memory for the key (+ 1 for the null terminator)
    new_item->key = calloc(strlen(key) + 1, sizeof(char));
    // set the key
    memccpy(new_item->key, key, '\0', strlen(key));
    // set the value
    new_item->value = value;

    size_t bucket = htab_index(h, key);
    // set the next item in the bucket to the new item
    new_item->next = h->buckets[bucket];
    // set the bucket to the new item
    h->buckets[bucket] = new_item;
    // increment size of table
    h->size++;

    return true;
}

// remove an item from the hash table
// free the memory allocated for the item
bool htab_remove(ht_t *h, char *key)
{
    // get the bucket for the key
    item_t *item = htab_bucket(h, key);
    item_t *curr = item;
    item_t *prev = NULL;
    // loop through each item in the bucket
    while (curr != NULL)
    {
        // if item is not the current item
        if (strcmp(curr->key, key) == 0)
        {
            if (prev == NULL)
            { // if the item is the first item in the bucket
                // set the bucket pointer to the next item
                h->buckets[htab_index(h, key)] = curr->next;
            }
            else
            { // not the first item, need to point the previous to the next (jump over the deleted item)
                // set the previous item's next pointer to the current item's next pointer
                prev->next = curr->next;
            }
            free(curr->key);
            free(curr);
            break;
        }
        // update loop vars
        prev = curr;
        curr = curr->next;
    }
    return true;
}

// resize the table
bool htab_resize(ht_t *h)
{
    size_t new_capacity = h->capacity * 2 + 1;
    // NOTE: check for possible overflow error
    if (new_capacity < h->capacity)
    {
        return false;
    }
    // allocate memory for new size
    item_t **new_buckets = calloc(new_capacity, sizeof(item_t *));
    if (!new_buckets)
    {
        // not enough memory
        return false;
    }

    size_t item_count = 0;

    // go through each existing bucket
    for (size_t i = 0; i < h->capacity; i++)
    {
        printf("Bucket %zu\n", i);
        // the current item
        item_t *item = h->buckets[i];
        // go through each item in the bucket
        while (item != NULL)
        {
            printf("Item: key: %s, value: %d \n", item->key, item->value);
            item_count++;
            item_t *next = item->next; // save the next item

            // get the new bucket for the item
            size_t new_bucket = djb_hash(item->key) % new_capacity;
            // set the next item in the bucket to the new item
            item->next = new_buckets[new_bucket];
            // set the bucket to the new item
            new_buckets[new_bucket] = item;

            // go to next item in existing bucket
            item = next;
        }
    }
    // free the old buckets
    free(h->buckets);
    // set the new members
    h->capacity = new_capacity;
    h->size = item_count;
    h->buckets = new_buckets;
    printf("Resized to hold %zu buckets\n", h->capacity);
    return true;
}

// for debugging, print the hash table
void item_print(item_t *item)
{
    printf("key: %s, value: %d\n", item->key, item->value);
}

void htab_print(ht_t *h)
{
    for (size_t i = 0; i < h->size; ++i)
    {
        printf("bucket %zu: ", i);
        if (h->buckets[i] == NULL)
        {
            printf("empty\n");
        }
        else
        {
            for (item_t *j = h->buckets[i]; j != NULL; j = j->next)
            {
                item_print(j);
                if (j->next != NULL)
                {
                    printf(" -> ");
                }
            }
            printf("\n");
        }
    }
}

// // testing
// int main()
// {
//     ht_t *table = malloc(sizeof(*table));
//     htab_init(table, 10);
//     printf("Table created with %zu buckets and capacity of %zu\n", table->size, table->capacity);
//     // char key[4];
//     // for (int i = 0; i < 100; i++)
//     // {
//     //     key[0] = 'A' + (i % 26);
//     //     key[1] = 'A' + ((i % 26) % 26);
//     //     key[2] = 'A' + ((i % 676) % 26);
//     //     key[3] = '\0';
//     //     htab_set(table, &key, i);
//     // }
//     htab_set(table, "A", 1);
//     htab_set(table, "B", 3);
//     htab_set(table, "C", 3);
//     htab_set(table, "A1", 1);
//     htab_set(table, "B1", 3);
//     htab_set(table, "C1", 3);
//     htab_set(table, "A2", 1);
//     htab_set(table, "B2", 3);
//     htab_set(table, "C2", 3);
//     // htab_set(table, "A3", 1);
//     // htab_set(table, "B3", 3);
//     // htab_set(table, "C3", 3);
//     // htab_set(table, "A4", 1);
//     // htab_set(table, "B4", 3);
//     // htab_set(table, "C4", 3);
//     // htab_set(table, "A5", 1);
//     //    htab_set(table, "B5", 3);
//     //    htab_set(table, "C5", 3);

//     printf("Table size: %zu\n", table->size);

//     item_t *item = htab_find(table, "A");

//     if (item != NULL)
//     {
//         printf("Item found: key: %s, value: %d\n", item->key, item->value);
//     }

//     htab_print(table);

//     htab_destroy(table);
//     return 0;
// }