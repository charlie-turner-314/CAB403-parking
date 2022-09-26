#pragma once
#include "config.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

struct LPR {
  pthread_mutex_t mutex;
  pthread_cond_t condition;
  char plate[6];
  char padding[2];
};

struct Boomgate {
  pthread_mutex_t mutex;
  pthread_cond_t condition;
  char status;
  char padding[7];
};

struct InfoSign {
  pthread_mutex_t mutex;
  pthread_cond_t condition;
  char display;
  char padding[7];
};

// An entrance
struct Entrance {
  struct LPR lpr;
  struct Boomgate gate;
  struct InfoSign sign;
};

struct Exit {
  struct LPR lpr;
  struct Boomgate gate;
};

struct Level {
  struct LPR lpr;
  volatile int16_t temp;
  volatile int8_t alarm;
  char padding[5];
};

struct SharedMemory {
  struct Entrance entrances[NUM_ENTRANCES];
  struct Exit exits[NUM_EXITS];
  struct Level levels[NUM_LEVELS];
};

struct SharedMemory *create_shm(char *name);

struct SharedMemory *get_shm(char *name);

bool destroy_shm(struct SharedMemory *shm);
