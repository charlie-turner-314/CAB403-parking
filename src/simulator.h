#pragma once
#include "config.h"
#include "queue.h"
#include "shm_parking.h"

// number of possible car threads - most sleeping so more than enough
#define CAR_THREADS (NUM_LEVELS * LEVEL_CAPACITY * 2)

// Types of fires - DEBUG ONLY, not used in real version
#define FIRE_ROR 1
#define FIRE_FIXED 2
#define FIRE_OFF 0

// Simulating Cars
// ==============

typedef struct car_thread_data {
  Queue *entry_queue;       // pointer to the entry queue
  char plate[7];            // number plate of the car
  struct SharedMemory *shm; // pointer to the shared memory
} ct_data;

/*
Main thread handler function for cars

    PROCESS:
    1. Wait for a car to enter the queue of all cars
    2. Assign the car to a random entry queue
    3. Wait for the car to be at the front of its entry queue
    4. Attempt entry -> `attempt_entry()`
    5. If the car is rejected, go back to step 1
    6. If the car is accepted, decide whether to listen to the given level
    7. Park the car on the given or a random elvel -> `park_car()`
    8. Exit the parking lot -> `exit_car()`
    9. Cleanup and go to step 1
*/
void *car_handler(void *arg);

/*
Simulate the temperature changing every 2ms

    DEBUGGING:

    - if `fire_type = FIRE_ROR`, the fire will be simulated by increasing the
      temperature to simulate a rate-of-rise fire

    - if `fire_type = FIRE_FIXED`, the fire will be simulated by imeediately
      increasing the temperature to simulate a fixed fire

    - if `fire_type = FIRE_OFF`, the temperature will hang around a safe range
*/
void *temp_simulator(void *arg);

/*
Handle the asynchronous opening and closing of the given boomgate

  WHEN the gate is set to R: wait 20ms, then set the gate to O

  WHEN the gate is set to L: wait 20ms, then set the gate to C
*/

void *gate_handler(void *arg);
/*
  Wait for the given gate to be open before returning
*/
void wait_at_gate(struct Boomgate *gate);

/*
Send the given plate to the given plate reader
- Waits for the plate reader to be NULL before sending
- Sets the plate reader to the given plate, broadcasts to all threads and
returns
*/
void send_licence_plate(char *plate, struct LPR *lpr);

/*
  Attempt to gain entry to the carpark
  1. Send the licence plate to the entrance LPR
  2. Wait for the entrance sign to display a character
  3. If the character is not a number, the car is rejected `return -1`
  4. If the character is a number, the car is accepted
  5. Wait at the entrance gate
  6. Return the level to park on
*/
int attempt_entry(ct_data *car_data);

/*
park the car in the level given by the entrance
1. Drive to level
2. Trigger level LPR
3. Park/drive around for a while
*/
void park_car(ct_data *car_data, int level);

/*
Process for exiting car park
1. Trigger the level LPR for the second time
2. drive to a random exit
3. Trigger the exit LPR -> `send_licence_plate()`
4. Wait for the exit boomgate to open -> `wait_at_gate()`
*/
void exit_car(ct_data *car_data, int level);

/*
Handle any user input from the command line

    - `q` to quit the program gracefully. This assumes the manager is still
      running, and will be able to handle any cars currently in the carpark

    - `f` to start a fixed temperature fire

    - `r` to start a rate-of-rise temperature fire

    - `s` to stop any fire
*/
void *input_handler(void *arg);
