#include "simulator.h"
#include "config.h"
#include <stdio.h>
#include <string.h>

int main(void) {
  SharedMemory *shm = create_shm(SHM_NAME);
  // change the lpr in shm->entrances[0]->lpr.plate
  // signal shm->entrances[0]->lpr.lpr_cond
  // exit
  //   strncpy(shm->entrances[0].lpr.plate, "ABC233", 6);
  //   pthread_cond_signal(&shm->entrances[0].lpr.condition);
  printf("Set plate to %.6s\n", shm->entrances[0].lpr.plate);

  // destroy the shared memory after use
  destroy_shm(shm);
  return 0;
}