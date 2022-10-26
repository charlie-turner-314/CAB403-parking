#pragma once

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>

// Header file for a thread-safe queue of strings

typedef struct QItem {
  void *value; // value can be any type
  struct QItem *next;
} QItem;

typedef struct Queue {
  int id;
  QItem *head;
  QItem *tail;
  pthread_mutex_t mutex;
  pthread_cond_t condition;
  size_t length;
} Queue;

// Create a new queue. Allocate memory for the queue and initialise the
// mutex and condition variable.
Queue *queue_create();

// Get the item at the head of the queue, but don't do anything about it.
QItem *queue_peek(Queue *q);

// Add an item to the tail of the queue.
bool queue_push(Queue *q, void *value, size_t size);

// Remove the item at the head of the queue
void queue_pop(Queue *q);

// Pop an item from the front of the queue and return it.
// unsafe, assumes:
// - the caller has locked the mutex
// - the caller will free the memory
QItem *unsafe_queue_pop_return(Queue *q);

// Destroy a queue, freeing memory for remaining items.
bool destroy_queue(Queue *q);
