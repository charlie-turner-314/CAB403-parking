#include <stdlib.h>
#include <stdbool.h>
#include "hashtable.h"

#define INITIAL_CAPACITY 10

typedef struct item
{
    char *key;
    item_t *next;
} item_t;

typedef struct ht
{
    item_t **buckets;
    size_t size;
} ht_t;

bool htab_init(ht_t *h, size_t n)
{
    h = malloc(sizeof(h));
    h->buckets = calloc(n, sizeof(item_t *));
    h->size = n;
    return h->buckets != 0;
}
