#pragma once
#include "hashtable.h"
#include "queue.h"

typedef struct ManDisplayData {
  struct SharedMemory *shm;  // pointer to the shared memory
  ht_t *ht;                  // hashtable of car positions
  pthread_mutex_t *ht_mutex; // mutex for the hashtable
} ManDisplayData;

typedef struct SimDisplayData {
  Queue **entry_queues;
  int *num_cars;
  int *running;
  size_t *available_plates;
} SimDisplayData;

// Display Handler For the Manager
//
// ------------------------------------------------------
// - Updates every 50ms
// - Displays all info available to manager:
// *arg is a pointer to a ManDisplayData struct
void *man_display_handler(void *arg);

// Display Handler For the Simulator
//
// not necessary for the spec, but useful for debugging
// ------------------------------------------------------
// - Updates only when necessary by waiting for condition variable:
// - Waits for the condition variable to be signalled (by simulator)
// - Prints the current state of the simulator
// - Repeats
// *arg is a pointer to a `SimDisplayData` struct
void *sim_display_handler(void *arg);
