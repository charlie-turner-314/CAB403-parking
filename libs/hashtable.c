#include "hashtable.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Maximum load before resizing
#define LOAD_FACTOR 0.75

typedef struct item {
  char *key;
  void *value; // allow any type of value
  item_t *next;
} item_t;

typedef struct ht {
  // pointers to items
  item_t **buckets;
  // Current number of items
  size_t size;
  // allocated capacity
  size_t capacity;
} ht_t;

ht_t *htab_create(ht_t *h, size_t n) {
  // Allocate memory for the table
  h = (ht_t *)calloc(1, sizeof(ht_t));
  // Allocate memory for n item pointers
  h->buckets = calloc(n, sizeof(item_t *));
  if (!h->buckets) {
    return NULL;
  }
  h->capacity = n; // allocated for this many buckets
  h->size = 0;     // no items yet
  return h;
}

void htab_destroy(ht_t *h) {
  // Free the memory allocated for each item
  for (size_t i = 0; i < h->capacity; i++) {
    item_t *current = h->buckets[i];
    while (current) {
      item_t *next = current->next;
      // key is a pointer so must free the memory allocated for it
      free(current->key);
      // value is a pointer so must free the memory allocated for it
      free(current->value);
      free(current);
      current = next;
    }
  }
  // Free the memory allocated for the buckets
  free(h->buckets);
  free(h);
}

size_t djb_hash(char *s) {
  size_t hash = 5381;
  int c;
  while ((c = *s++) != '\0') {
    // hash = hash * 33 + c
    hash = ((hash << 5) + hash) + c;
  }
  return hash;
}

size_t htab_index(ht_t *h, char *key) { return djb_hash(key) % h->capacity; }

item_t *htab_bucket(ht_t *h, char *key) {
  size_t index = htab_index(h, key);
  return h->buckets[index];
}

item_t *htab_find(ht_t *h, char *key) {
  // get the bucket for the key
  item_t *item = htab_bucket(h, key);
  // go through each item in the bucket
  while (item != NULL) {
    if (strcmp(item->key, key) == 0) {
      return item;
    }
    item = item->next;
  }
  return NULL;
}

bool htab_set(ht_t *h, char *key, void *value, size_t size) {
  // check if already there
  item_t *existing_item;
  if ((existing_item = htab_find(h, key)) != NULL) {
    // free existing value
    free(existing_item->value);
    // allocate memory for new value
    existing_item->value = malloc(size);
    // copy new value
    memcpy(existing_item->value, value, size);
    return existing_item->value != NULL;
  }

  // ADDING NEW ITEM
  // check that adding the item (assuming into a new bucket) doesn't overfill
  // the table
  if ((h->size + 1.0) / h->capacity >= LOAD_FACTOR) {
    // resize the table, if it fails, return false
    if (!htab_resize(h)) {
      return false;
    }
  }

  // allocate memory for the new item
  item_t *new_item = (item_t *)calloc(1, sizeof(item_t));

  // if the memory allocation failed, return false
  if (new_item == NULL) {
    free(new_item);
    return false;
  }
  // allocate memory for the key (+ 1 for the null terminator)
  new_item->key = (char *)calloc(strlen(key) + 1, sizeof(char));
  // set the key
  strncpy(new_item->key, key, strlen(key) + 1);
  // allocate memory for the value
  new_item->value = malloc(size);
  // copy the value
  memcpy(new_item->value, value, size);

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
bool htab_remove(ht_t *h, char *key) {
  // get the bucket for the key
  item_t *item = htab_bucket(h, key);
  item_t *curr = item;
  item_t *prev = NULL;
  // loop through each item in the bucket
  while (curr != NULL) {
    // if item is not the current item
    if (strcmp(curr->key, key) == 0) {
      if (prev == NULL) { // if the item is the first item in the bucket
        // set the bucket pointer to the next item
        h->buckets[htab_index(h, key)] = curr->next;
      } else { // not the first item, need to point the previous to the next
               // (jump over the deleted item)
        // set the previous item's next pointer to the current item's next
        // pointer
        prev->next = curr->next;
      }
      free(curr->key);
      free(curr->value);
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
bool htab_resize(ht_t *h) {
  size_t new_capacity = h->capacity * 2 + 1;

  // NOTE: check for possible overflow error
  if (new_capacity < h->capacity) {
    return false;
  }

  // allocate memory for new size
  item_t **new_buckets = calloc(new_capacity, sizeof(item_t *));
  if (!new_buckets) {
    // not enough memory
    return false;
  }

  // number of items
  size_t item_count = 0;

  // go through each existing bucket
  for (size_t i = 0; i < h->capacity; i++) {
    // the current item
    item_t *item = h->buckets[i];
    // go through each item in the bucket
    while (item != NULL) {
      item_count++;
      item_t *next = item->next; // save the next item

      // get the new bucket for the item
      size_t new_bucket = djb_hash(item->key) % new_capacity;

      // Insert at head of linked list
      // point the next item to the current head
      item->next = new_buckets[new_bucket];
      // set the bucket to point to the new item
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
  return true;
}

// getters
void *htab_get(ht_t *h, char *key) {
  item_t *item = htab_find(h, key);
  if (item == NULL) {
    return NULL;
  }
  return item->value;
}

void *item_get(item_t *item) { return item->value; }

size_t htab_capacity(ht_t *h) { return h->capacity; }

size_t htab_size(ht_t *h) { return h->size; }
