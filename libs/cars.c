#include "shm_parking.h"
#include <stdlib.h>

void generate_car(char *plate) {
  // generate a random licence plate as key -> ABC123:
  for (int j = 0; j < 6; j++) {
    if (j < 3)
      plate[j] = ("ABCDEFGHIJKLMNOPQRSTUVWXYZ"[rand() % 26]);
    else
      plate[j] = "0123456789"[(rand() % 10)];
  }
  plate[6] = '\0';
}

// void *car_handler(void *arg) {
//     // get the shared memory
//     SharedMemory *shm = get_shm(SHM_NAME);
//     // get the car

//     return NULL; }