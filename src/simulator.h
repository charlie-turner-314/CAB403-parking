#pragma once
#include "hashtable.h"
#include "queue.h"
#include "shm_parking.h"

// Simulating Cars
// ==============

typedef struct car_thread_data {
  Queue *entry_queue;       // pointer to the entry queue
  char plate[7];            // number plate of the car
  struct SharedMemory *shm; // pointer to the shared memory
} ct_data;

// attempt entry -> trigger the LPR and wait for entrance thread to open the
// gate, return the level to park on, or -1 if the car is rejected
int attempt_entry(ct_data *car_data);

// park the car in the level given by the entrance -> triggering the level LPR
void park_car(ct_data *car_data, int level);

// leave the current level (triggering the level LPR), and then exit the parking
// lot (trigger the Exit LPR), waiting until the boomgate is open then
// destroying the car thread
void exit_car(ct_data *car_data, int level);

// main handler function for a car thread
// given a car thread data struct
void *car_handler(void *arg);