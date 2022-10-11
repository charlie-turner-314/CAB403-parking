// A thread-safe queue implementation.
#include "queue.h"
#include "simulator.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Queue *queue_create(int id) {
  Queue *q = malloc(sizeof(Queue));
  q->id = id;
  q->head = NULL;
  q->tail = NULL;
  q->length = 0;
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
  QItem *item = q->head;
  return item;
}

// Push an item to the back of the queue.
bool queue_push(Queue *q, void *value, size_t size) {
  if (q == NULL) {
    return false;
  }
  // ensure only one thread can access the queue at a time
  pthread_mutex_lock(&q->mutex);
  QItem *new_item = calloc(1, sizeof(QItem));
  if (new_item == NULL) {
    return false;
  }
  // allocate memory for the value
  new_item->value = calloc(1, size);
  // copy the value into the new item
  memcpy(new_item->value, value, size);
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
  q->length++;
  return true;
}

// Pop an item from the front of the queue.
void queue_pop(Queue *q) {
  if (!q || !q->head) {
    return;
  }
  // ensure only one thread can access the queue at a time
  pthread_mutex_lock(&q->mutex);
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
  q->length--;
  return;
}

QItem *unsafe_queue_pop_return(Queue *q) {
  if (!q || !q->head) {
    return NULL;
  }
  QItem *item = q->head;
  q->head = q->head->next;
  // if the queue is now empty, set the tail to NULL
  if (q->head == NULL) {
    q->tail = NULL;
  }
  q->length--;
  return item;
}

bool destroy_queue(Queue *q) {
  if (q == NULL) {
    return false;
  }
  while (q->head != NULL) {
    QItem *temp = q->head;
    q->head = q->head->next;
    q->length--;
    free(temp->value);
    free(temp);
  }
  int mutex_destroy = pthread_mutex_destroy(&q->mutex);
  printf("queue %d mutex destroy result: %d\n", q->id, mutex_destroy);
  if (mutex_destroy) {
    perror("Error destroying mutex");
    exit(EXIT_FAILURE);
  }
  int cond_destroy = pthread_cond_destroy(&q->condition);
  if (cond_destroy) {
    perror("Error destroying condition");
    exit(EXIT_FAILURE);
  }
  free(q);
  return 1;
}

// print entrance item
static void entrance_item_print(char *plate) { printf("'%6s' ", plate); }

// print entrance queue
void entry_queue_print(Queue *q) {

  pthread_mutex_lock(&q->mutex);
  QItem *node = q->head;

  if (!node)
    printf("empty");

  while (node) {
    entrance_item_print(node->value);
    node = node->next;
  }
  pthread_mutex_unlock(&q->mutex);
}

// print car item
static void car_item_print(ct_data *car_data) {
  printf("'%6s' ", car_data->plate);
}

// print car object queue
void car_queue_print(Queue *q) {

  pthread_mutex_lock(&q->mutex);
  QItem *node = q->head;

  if (!node)
    printf("empty");

  while (node) {
    car_item_print(node->value);
    node = node->next;
  }
  pthread_mutex_unlock(&q->mutex);
}
