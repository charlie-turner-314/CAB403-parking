#include "delay.h"
#include "logging.h"
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <shm_parking.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

static struct SharedMemory *shm;
static int alarm_active = 0;
static int smoothed_temps[NUM_LEVELS]
                         [30]; // 2D array to store smoothed median values

// switches index of 2 array elements
static void elementSwap(int *element1, int *element2) {
  int temp;

  temp = *element1;
  *element1 = *element2;
  *element2 = temp;
}

// ASC array sort
static void arrSort(int arr[], int arrLen) {
  for (int i = 0; i < (arrLen - 1); i++) {
    for (int j = 0; j < (arrLen - i - 1); j++) {
      if (arr[j] > arr[j + 1]) {
        elementSwap(&arr[j], &arr[j + 1]);
      }
    }
  }
}

// find median value in raw temperature array
static int median_calc(int level, int temps[5]) {
  int rawTemp = shm->levels[level].temp; // get new temp from shm
  int median = -1;

  // shift temps left in array
  void *dest =
      memmove(&temps[0], &temps[1], (unsigned long)4 * sizeof(temps[0]));
  if (!dest) {
    // something went wrong
    log_print_string("memmove failed");
  } else {
    temps[4] = rawTemp; // push newTemp to array

    int hasFiveTemps = 1; // assume we can calc median
    // check that no empty values in array
    for (int i = 0; i < 5; i++) {
      if (temps[i] == INT_MIN) {
        // can't calc median value yet
        hasFiveTemps = 0;
      }
    }

    if (hasFiveTemps == 1) {
      // median value can be calculated
      int tempsCopy[5];
      void *dest = memcpy(tempsCopy, temps,
                          (unsigned long)5 *
                              sizeof(int)); // make copy of array for sorting
      if (!dest) {
        // something went wrong
        log_print_string("memcpy failed\n");
        median = -1;
      } else {
        arrSort(tempsCopy, 5); // sort in asc first
        median = tempsCopy[2]; // median is mid index
      }
    }
  }
  return median;
}

// handles smoothed temperatures array
static void smoothedTemp_handler(int level, int median) {
  // add median to smoothed array
  if (median != -1) {
    void *dest =
        memmove(&smoothed_temps[level][0], &smoothed_temps[level][1],
                (unsigned long)29 * sizeof(int)); // shift temps up in array
    if (!dest) {
      // something went wrong
      log_print_string("memmove failed\n");
    } else {
      smoothed_temps[level][29] = median; // push median to array
    }
  }
}

// monitorr the temperatures for conditions
static void *temp_monitor(void *arg) {
  // MISRA 11.5: Convert pointer to size
  // Cannot be avoided in the case of pthreads
  size_t level_id = *(size_t *)arg;
  int temps[5] = {INT_MIN, INT_MIN, INT_MIN, INT_MIN,
                  INT_MIN}; // array to calc median value for the level

  size_t level = level_id;
  log_print_string("Starting temperature monitor for all levels\n");
  while (true) {
    int hightemps = 0;
    int emptyReadings = 0;
    int median = median_calc(level, temps);
    smoothedTemp_handler(level, median);
    // fixed temperature fire detection
    for (int i = 0; i < 30; i++) {
      // Temperatures of 58 degrees and higher are a concern
      if (smoothed_temps[level][i] >= 58) {
        hightemps++;
      } else if (smoothed_temps[level][i] ==
                 INT_MIN) // check if array is valid i.e full with temp readings
      {
        emptyReadings++;
      } else {
        // no fire
      }
    }

    // If 90% of the last 30 temperatures are >= 58 degrees,
    // this is considered a high temperature. Raise the alarm
    if (((hightemps >= (30 * 0.9))) && (emptyReadings == 0)) {
      alarm_active = 1;
    } else if (((smoothed_temps[level][29] - smoothed_temps[level][0]) >= 8) &&
               (emptyReadings == 0)) { // ROR raise the alarm
      alarm_active = 1;
    } else {
      alarm_active = 0;
    }
    delay_ms(2);
  }
  return NULL;
}

// opens all entrance and exit boomgates
static void openboomgate(int level) {
  pthread_mutex_lock(&shm->entrances[level].gate.mutex);
  shm->entrances[level].gate.status = 'O'; // set entrance gates to open
  pthread_mutex_unlock(&shm->entrances[level].gate.mutex);
  pthread_mutex_lock(&shm->exits[level].gate.mutex);
  shm->exits[level].gate.status = 'O'; // set exit gates to open
  pthread_mutex_unlock(&shm->exits[level].gate.mutex);
}

int main(void) {
  shm = get_shm(SHM_NAME); // get the shared memory object

  pthread_t level_threads[NUM_LEVELS];
  // create temperature monitoring threads
  for (size_t i = 0; i < (size_t)NUM_LEVELS; i++) {
    // MISRA 11.6: Cast from void pointer to int
    // Cannot be avoided in the case of pthreads
    size_t level = i;
    pthread_create(&level_threads[i], NULL, temp_monitor, &level);
  }
  int8_t printed_deactivated = 1; // don't print deactivated on first read
  int8_t printed_activated = 0;

  // init smoothed temps array with non-existing temperature value using INT_MIN
  for (int i = 0; i < (int)NUM_LEVELS; i++) {
    for (int j = 0; j < 30; j++) {
      smoothed_temps[i][j] = INT_MIN;
    }
  }

  log_print_string("Firealarm System Running\n");
  while (1) {
    if (alarm_active == 1) {
      if (!printed_activated) {
        log_raise_alarm();
        printed_activated = 1;
        printed_deactivated = 0;
      }

      // Handle the alarm system and open boom gates
      // Activate alarms on all levels
      for (int i = 0; i < NUM_LEVELS; i++) {
        shm->levels[i].alarm = 1; // set shm alarm to true
        openboomgate(i);          // open up all boom gates
      }

      // Show evacuation message on an endless loop
      const char evacmessage[9] = "EVACUATE ";
      for (int i = 0; i < 9; i++) {
        for (int j = 0; j < NUM_ENTRANCES; j++) {
          pthread_mutex_lock(&shm->entrances[j].sign.mutex);
          shm->entrances[j].sign.display = evacmessage[i];
          pthread_mutex_unlock(&shm->entrances[j].sign.mutex);
        }
        delay_ms(20); // update sign with new letter every 20ms
      }
    } else {
      if (!printed_deactivated) {
        log_stop_alarm();
        printed_deactivated = 1;
        printed_activated = 0;
      }
      // Deactivate alarms on all levels
      for (int i = 0; i < NUM_LEVELS; i++) {
        shm->levels[i].alarm = 0; // set shm alarm to false
      }
      delay_ms(2); // sleep for 2 ms
    }
  }

  for (int i = 0; i < NUM_LEVELS; i++) {
    int jres = pthread_join(level_threads[i], NULL);
    if (jres != 0) {
      log_print_string("Error joining thread");
      break;
    }
  }
}