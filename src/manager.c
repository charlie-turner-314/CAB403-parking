#include "config.h"
#include "shm_parking.h"
#include <stdio.h>

int main(void) {
  SharedMemory *shm = get_shm(SHM_NAME);
  printf("The value of the plate is %.6s\n", shm->entrances[0].lpr.plate);
  destroy_shm(shm);
}