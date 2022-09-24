#pragma once

#include <pthread.h>
#include <stdbool.h>

// Header file for a thread-safe queue of strings

typedef struct QItem {
  char *value;
  struct QItem *next;
} QItem;

typedef struct Queue {
  QItem *head;
  QItem *tail;
  pthread_mutex_t mutex;
  pthread_cond_t condition;
} Queue;

Queue *queue_create();

QItem *queue_peek(Queue *q);

bool queue_push(Queue *q, char *value);

QItem *queue_pop(Queue *q);

bool destroy_queue(Queue *q);