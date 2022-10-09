#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "shm_parking.h"
#include "display.h"
#include "config.h"

#define ANSI_CTRL_CLEAR "\x1B[2J"
#define ANSI_CTRL_HOME  "\x1B[;H"

//TODO: running in seperate terminal for debugging purposes atm. Need to run in manager as spec sheet defines.
void *display_handler(void *arg) {
    arg = arg; // silence unused arg warning
    puts(ANSI_CTRL_CLEAR);

    while (true) {
        puts(ANSI_CTRL_HOME);
        printf("Status Display\n");
        
        // print each entrance LPR for each level
        for (int i = 0; i < NUM_ENTRANCES; i++) {
            pthread_mutex_lock(&shm->entrances[i].lpr.mutex);
            char plate[6];
            memcpy(plate, shm->entrances[i].lpr.plate, 6);
            pthread_mutex_unlock(&shm->entrances[i].lpr.mutex);
            printf("Entry LPR %d: %6s \n", i, plate);

            // wait 50ms
            usleep(50000);
        }
    }
    return NULL;
}

