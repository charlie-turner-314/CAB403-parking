// A thread-safe queue implementation.
#include "queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Create a new queue.
Queue *queue_create(int id) {
  Queue *q = malloc(sizeof(Queue));
  q->id = id;
  q->head = NULL;
  q->tail = NULL;
  int mutex = pthread_mutex_init(&q->mutex, NULL);
  if (mutex) {
    perror("Error creating mutex");
    exit(EXIT_FAILURE);
  }
  int condition = pthread_cond_init(&q->condition, NULL);
  if (condition) {
    perror("Error creating condition");
    exit(EXIT_FAILURE);
  }
  return q;
}

QItem *queue_peek(Queue *q) {
  // NOTE: removed the mutex lock but keep in mind for later if something breaks
  // pthread_mutex_lock(&q->mutex);
  QItem *item = q->head;
  // pthread_mutex_unlock(&q->mutex);
  return item;
}

// Push an item to the back of the queue.
bool queue_push(Queue *q, char *value) {
  if (q == NULL) {
    return false;
  }
  // ensure only one thread can access the queue at a time
  pthread_mutex_lock(&q->mutex);
  QItem *new_item = calloc(1, sizeof(QItem));
  if (new_item == NULL) {
    return false;
  }
  // allocate memory for the value and a null terminator
  new_item->value = calloc(sizeof(char), (strlen(value) + 1));
  // copy the value into the new item, ensuring null termination
  strncpy(new_item->value, value, strlen(value) + 1);
  // end of queue so no next
  new_item->next = NULL;

  if (q->head == NULL) {
    q->head = new_item;
    q->tail = new_item;
  } else {
    q->tail->next = new_item;
    q->tail = new_item;
  }
  // signal the condition variable
  pthread_cond_signal(&q->condition);
  pthread_mutex_unlock(&q->mutex);
  return true;
}

// Pop an item from the front of the queue.
QItem *queue_pop(Queue *q) {
  if (q == NULL) {
    return NULL;
  }
  // ensure only one thread can access the queue at a time
  pthread_mutex_lock(&q->mutex);
  // wait until the queue has an item
  while (q->head == NULL) {
    pthread_cond_wait(&q->condition, &q->mutex);
  }
  QItem *item = q->head;
  q->head = q->head->next;
  // if the queue is now empty, set the tail to NULL
  if (q->head == NULL) {
    q->tail = NULL;
  }
  // broadcast the condition variable
  pthread_cond_broadcast(&q->condition);
  pthread_mutex_unlock(&q->mutex);
  free(item->value);
  free(item);
  return item;
}

// Destroy a queue.
bool destroy_queue(Queue *q) {
  if (q == NULL) {
    return false;
  }
  while (q->head != NULL) {
    QItem *temp = q->head;
    q->head = q->head->next;
    free(temp->value);
    free(temp);
  }
  int mutex_destroy = pthread_mutex_destroy(&q->mutex);
  if (mutex_destroy) {
    perror("Error destroying mutex");
    exit(EXIT_FAILURE);
  }
  int cond_destroy = pthread_cond_destroy(&q->condition);
  if (cond_destroy) {
    perror("Error destroying condition");
    exit(EXIT_FAILURE);
  }
  return 1;
}