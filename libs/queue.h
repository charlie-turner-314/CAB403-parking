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

Queue *queue_create();

QItem *queue_peek(Queue *q);

bool queue_push(Queue *q, void *value, size_t size);

void queue_pop(Queue *q);

QItem *queue_pop_unsafe(Queue *q);

bool destroy_queue(Queue *q);