#pragma once
#include <pthread.h>

/* delay for a random amount of time between min and max,
 measured in ms. mutex is a mutex protecting the random number generator */
void rand_delay_ms(int min, int max, pthread_mutex_t *mutex);

/* delay for the given number of miliseconds. Uses the time factor to keep
 * consistent with other timings*/
void delay_ms(int delay);