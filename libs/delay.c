#include "delay.h"
#include "config.h"
#include <stdlib.h>
#include <unistd.h>

void rand_delay_ms(int min, int max, pthread_mutex_t *mutex) {
  pthread_mutex_lock(mutex);
  int delay = rand() % (max - min + 1) + min;
  pthread_mutex_unlock(mutex);
  usleep((delay * 1000) * TIME_FACTOR);
}