#pragma once
#include "config.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct LPR {
  pthread_mutex_t mutex;
  pthread_cond_t condition;
  char plate[6];
  char padding[2];
} LPR;

typedef struct Boomgate {
  pthread_mutex_t mutex;
  pthread_cond_t condition;
  char status[1];
  char padding[7];
} Boomgate;

typedef struct InfoSign {
  pthread_mutex_t mutex;
  pthread_cond_t condition;
  char display[1];
  char padding[7];
} InfoSign;

// An entrance
typedef struct Entrance {
  LPR lpr;
  Boomgate gate;
  InfoSign sign;
} Entrance;

typedef struct Exit {
  LPR lpr;
  Boomgate gate;
} Exit;

typedef struct Level {
  LPR lpr;
  volatile int16_t temp;
  volatile int8_t alarm;
  char padding[5];
} Level;

typedef struct SharedMemory {
  Entrance entrances[NUM_ENTRANCES];
  Exit exits[NUM_EXITS];
  Level levels[NUM_LEVELS];
} SharedMemory;

SharedMemory *create_shm(char *name);

SharedMemory *get_shm(char *name);

bool destroy_shm(SharedMemory *shm);
