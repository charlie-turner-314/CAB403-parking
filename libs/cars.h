#pragma once
#include "hashtable.h"
#include "queue.h"
#include "shm_parking.h"

// Simulating Cars
// ==============

typedef struct car_thread_data {
  Queue *entry_queue; // pointer to the entry queue
  char plate[7];      // number plate of the car
} ct_data;

// A car is a thread that enters the parking lot, parks, and leaves.

// place a car in the entry queue of a random entrance
void queue_car_entry(char *plate);

// attempt entry -> trigger the LPR and wait for entrance thread to open the
// gate, return the level to park on, or -1 if the car is rejected
int attempt_entry(char *plate, Entrance *entrance);

// park the car in the level given by the entrance -> triggering the level LPR
void park_car(char *plate, int level);

// leave the current level (triggering the level LPR), and then exit the parking
// lot (trigger the Exit LPR), waiting until the boomgate is open then
// destroying the car thread
void exit_car(char *plate, int level);

// main handler function for a car thread
void *car_handler(void *arg);