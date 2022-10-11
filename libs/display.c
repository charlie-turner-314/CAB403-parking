#include "display.h"
#include "config.h"
#include "shm_parking.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define ANSI_CTRL_CLEAR "\x1B[2J"
#define ANSI_CTRL_HOME "\x1B[;H"

// TODO: running in seperate terminal for debugging purposes atm. Need to run in
// manager as spec sheet defines.
void *display_handler() {
  puts(ANSI_CTRL_CLEAR);
  struct SharedMemory *shm = get_shm(SHM_NAME);

  while (true) {
    puts(ANSI_CTRL_HOME);
    printf("Status Display\n");

    // print each entrance LPR for each level
    for (int i = 0; i < NUM_ENTRANCES; i++) {
      pthread_mutex_lock(&shm->entrances[i].lpr.mutex);
      char plate[7];
      memcpy(plate, shm->entrances[i].lpr.plate, 7);
      char boom = shm->entrances[i].gate.status;
      char sign = shm->entrances[i].sign.display;
      pthread_mutex_unlock(&shm->entrances[i].lpr.mutex);
      printf("Entry LPR %d: %6s | ", i, plate);
      printf("'%c' | ", boom == 0 ? ' ' : boom);
      printf("'%c' |\n", sign == 0 ? ' ' : sign);
      // wait 50ms
      usleep(50000);
    }

    //   // print each exit LPR for each level
    // for (int i = 0; i < NUM_EXITS; i++) {
    //     pthread_mutex_lock(&shm->exits[i].lpr.mutex);
    //     char plate[7];
    //     memcpy(plate, shm->exits[i].lpr.plate, 7);
    //     pthread_mutex_unlock(&shm->exits[i].lpr.mutex);
    //     printf("Exit LPR %d: %6s \n", i, plate);

    //     // wait 50ms
    //     usleep(50000);
    // }
  }
  return NULL;
}
