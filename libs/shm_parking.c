#include "shm_parking.h"
#include "config.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

struct SharedMemory *create_shm(char *name) {
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
  ftruncate(fd, sizeof(struct SharedMemory));
  struct SharedMemory *shm = mmap(NULL, sizeof(struct SharedMemory),
                                  PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (shm == MAP_FAILED) {
    perror("mmap");
    exit(1);
  }

  printf("Size of shared memory: %ld\n", sizeof(struct SharedMemory));

  // Track any mutex errors (don't want to track each individually, and if one
  // fails we need to exit anyway)
  int mutex_error = 0;

  // Pthread attributes to share between processes
  pthread_mutexattr_t mutex_attr;
  pthread_mutexattr_init(&mutex_attr);
  int mut_attr_ret =
      pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
  pthread_condattr_t cond_attr;
  pthread_condattr_init(&cond_attr);
  int cond_attr_ret =
      pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);

  if (mut_attr_ret != 0 || cond_attr_ret != 0) {
    perror("pthread_mutexattr_setpshared");
    exit(1);
  }

  // initialise mutexes and condition variables for the entrances
  for (int entrance = 0; entrance < NUM_ENTRANCES; entrance++) {
    mutex_error |=
        pthread_mutex_init(&shm->entrances[entrance].lpr.mutex, &mutex_attr);
    mutex_error |=
        pthread_cond_init(&shm->entrances[entrance].lpr.condition, &cond_attr);
    mutex_error |=
        pthread_mutex_init(&shm->entrances[entrance].gate.mutex, &mutex_attr);
    mutex_error |=
        pthread_cond_init(&shm->entrances[entrance].gate.condition, &cond_attr);
    mutex_error |=
        pthread_mutex_init(&shm->entrances[entrance].sign.mutex, &mutex_attr);
    mutex_error |=
        pthread_cond_init(&shm->entrances[entrance].sign.condition, &cond_attr);
    // set boomgates to 'C' for closed
    shm->entrances[entrance].gate.status = 'C';
  }

  // initialise mutexes and condition variables for the exits
  for (int exit = 0; exit < NUM_EXITS; exit++) {
    mutex_error |= pthread_mutex_init(&shm->exits[exit].lpr.mutex, &mutex_attr);
    mutex_error |=
        pthread_cond_init(&shm->exits[exit].lpr.condition, &cond_attr);
    mutex_error |=
        pthread_mutex_init(&shm->exits[exit].gate.mutex, &mutex_attr);
    mutex_error |=
        pthread_cond_init(&shm->exits[exit].gate.condition, &cond_attr);
    shm->exits[exit].gate.status = 'C';
  }

  // initialise mutexes and condition variables for the levels
  for (int level = 0; level < NUM_LEVELS; level++) {
    mutex_error |=
        pthread_mutex_init(&shm->levels[level].lpr.mutex, &mutex_attr);
    mutex_error |=
        pthread_cond_init(&shm->levels[level].lpr.condition, &cond_attr);
    shm->levels[level].temp = 25; // room temp to start with
  }

  if (mutex_error) {
    perror("mutex or condition initialisation");
    exit(EXIT_FAILURE);
  }

  return shm;
}

struct SharedMemory *get_shm(char *name) {
  // open the shared memory segment with shm_open
  // map the memory to the size of a SharedMemory struct
  // return the pointer to the shared memory
  int fd = shm_open(name, O_RDWR, 0666);
  if (fd == -1) {
    perror("shm_open");
    exit(1);
  }
  struct SharedMemory *shm = mmap(NULL, sizeof(struct SharedMemory),
                                  PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (shm == MAP_FAILED) {
    perror("mmap");
    exit(1);
  }
  return shm;
}

bool destroy_shm(struct SharedMemory *shm) {
  // destroy all the mutexes and condition variables
  for (int entrance = 0; entrance < NUM_ENTRANCES; entrance++) {
    pthread_mutex_destroy(&shm->entrances[entrance].lpr.mutex);
    pthread_mutex_destroy(&shm->entrances[entrance].gate.mutex);
    pthread_mutex_destroy(&shm->entrances[entrance].sign.mutex);
    pthread_cond_destroy(&shm->entrances[entrance].lpr.condition);
    pthread_cond_destroy(&shm->entrances[entrance].gate.condition);
    pthread_cond_destroy(&shm->entrances[entrance].sign.condition);
  }
  for (int exit = 0; exit < NUM_EXITS; exit++) {
    pthread_mutex_destroy(&shm->exits[exit].lpr.mutex);
    pthread_mutex_destroy(&shm->exits[exit].gate.mutex);
    pthread_cond_destroy(&shm->exits[exit].lpr.condition);
    pthread_cond_destroy(&shm->exits[exit].gate.condition);
  }
  for (int level = 0; level < NUM_LEVELS; level++) {
    pthread_mutex_destroy(&shm->levels[level].lpr.mutex);
    pthread_cond_destroy(&shm->levels[level].lpr.condition);
  }

  // unmap the shared memory (for completeness - will be done automatically on
  // exit)
  // close the shared memory return 0
  if (munmap(shm, sizeof(struct SharedMemory)) == -1) {
    perror("munmap");
    return (1);
  }
  int unlink = shm_unlink(SHM_NAME);
  if (unlink == -1) {
    perror("shm_unlink");
    return (0);
  }
  return 1;
}