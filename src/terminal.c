#include "config.h"
#include "display.h"
#include "shm_parking.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct SharedMemory *shm;

// used ot run display in seperate terminal (only for now as easier to debug)
int main(void) {
    // get the shared memory object
    shm = get_shm(SHM_NAME);

    display_handler(NULL);
}