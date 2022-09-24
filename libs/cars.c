#include "cars.h"
#include "shm_parking.h"
#include <stdio.h>
#include <stdlib.h>

// car given a number plate and an entrance queue
void *car_handler(void *arg) {
  ct_data *data = (ct_data *)arg;
  printf("Car %s has started\n", data->plate);
  return NULL;
}