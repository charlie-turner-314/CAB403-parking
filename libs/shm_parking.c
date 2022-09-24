#include "shm_parking.h"
#include "config.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

SharedMemory *create_shm(char *name) {
  // remove if exists
  shm_unlink(name);
  // create shared memory segment with shm_open
  // map the memory to the size of a SharedMemory struct
  // initialize the mutex and condition variables
  // return the pointer to the shared memory
  int fd = shm_open(name, O_RDWR | O_CREAT, 0666);
  if (fd == -1) {
    perror("shm_open");
    exit(1);
  }
  ftruncate(fd, sizeof(SharedMemory));
  SharedMemory *shm = mmap(NULL, sizeof(SharedMemory), PROT_READ | PROT_WRITE,
                           MAP_SHARED, fd, 0);
  if (shm == MAP_FAILED) {
    perror("mmap");
    exit(1);
  }

  printf("Size of shared memory: %ld\n", sizeof(SharedMemory));

  // Track any mutex errors (don't want to track each individually, and if one
  // fails we need to exit anyway)
  int mutex_error = 0;

  // Pthread attributes to share between processes
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
  pthread_condattr_t cond_attr;
  pthread_condattr_init(&cond_attr);
  pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);

  // initialise mutexes and condition variables for the entrances
  for (int entrance = 0; entrance < NUM_ENTRANCES; entrance++) {
    mutex_error |=
        pthread_mutex_init(&shm->entrances[entrance].lpr.mutex, &attr);
    mutex_error |=
        pthread_cond_init(&shm->entrances[entrance].lpr.condition, &cond_attr);
    mutex_error |=
        pthread_mutex_init(&shm->entrances[entrance].gate.mutex, &attr);
    mutex_error |=
        pthread_cond_init(&shm->entrances[entrance].gate.condition, &cond_attr);
    mutex_error |=
        pthread_mutex_init(&shm->entrances[entrance].sign.mutex, &attr);
    mutex_error |=
        pthread_cond_init(&shm->entrances[entrance].sign.condition, &cond_attr);
  }

  // initialise mutexes and condition variables for the exits
  for (int exit = 0; exit < NUM_EXITS; exit++) {
    mutex_error |= pthread_mutex_init(&shm->exits[exit].lpr.mutex, &attr);
    mutex_error |=
        pthread_cond_init(&shm->exits[exit].lpr.condition, &cond_attr);
    mutex_error |= pthread_mutex_init(&shm->exits[exit].gate.mutex, &attr);
    mutex_error |=
        pthread_cond_init(&shm->exits[exit].gate.condition, &cond_attr);
  }

  // initialise mutexes and condition variables for the levels
  for (int level = 0; level < NUM_LEVELS; level++) {
    mutex_error |= pthread_mutex_init(&shm->levels[level].lpr.mutex, &attr);
    mutex_error |=
        pthread_cond_init(&shm->levels[level].lpr.condition, &cond_attr);
  }

  if (mutex_error) {
    perror("mutex_initialisation");
    exit(EXIT_FAILURE);
  }

  return shm;
}

SharedMemory *get_shm(char *name) {
  // open the shared memory segment with shm_open
  // map the memory to the size of a SharedMemory struct
  // return the pointer to the shared memory
  int fd = shm_open(name, O_RDWR, 0666);
  if (fd == -1) {
    perror("shm_open");
    exit(1);
  }
  SharedMemory *shm = mmap(NULL, sizeof(SharedMemory), PROT_READ | PROT_WRITE,
                           MAP_SHARED, fd, 0);
  if (shm == MAP_FAILED) {
    perror("mmap");
    exit(1);
  }
  return shm;
}

bool destroy_shm(SharedMemory *shm) {
  // unmap the shared memory (for completeness - will be done automatically on
  // exit)
  // close the shared memory return 0
  if (munmap(shm, sizeof(SharedMemory)) == -1) {
    perror("munmap");
    return (1);
  }
  shm_unlink(SHM_NAME);
  return 1;
}