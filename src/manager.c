#include "config.h"
#include "hashtable.h"
#include "shm_parking.h"
#include <stdio.h>
#include <string.h>

// number of plates to initialise hashtable, can be lower than the actual number
#define EXPECTED_NUM_PLATES 10

// mutex for accessing the hashtable, hashtable should be fast enough to
// not be a performance problem
pthread_mutex_t hashtable_mutex;

// read number plates from a file called "plates.txt"
// will be formatted as follows:
// ABC123\nABC123\nABC123\n
// places all the plates in a hashtable, initialising their value to 0
ht_t *read_plates(char *filename) {
  puts(filename);
  FILE *fp = fopen(filename, "r");
  if (fp == NULL) {
    perror("Error opening file");
    return NULL;
  }
  // create a hashtable
  ht_t *ht = NULL;
  ht = htab_init(ht, EXPECTED_NUM_PLATES);
  // read the file line by line
  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;
  while ((linelen = getline(&line, &linecap, fp)) > 0) {
    line[6] = '\0'; // null-terminate the plate if not already
    htab_set(ht, line, 0);
  }
  return ht;
}

int main(void) {
  SharedMemory *shm = get_shm(SHM_NAME);
  // set the entrance 4 entry sign to '4'
  printf("Plate is: %.6s\n", shm->entrances[1].lpr.plate);
  // set the plate to NULL characters
  pthread_mutex_lock(&shm->entrances[1].lpr.mutex);
  memset(shm->entrances[1].lpr.plate, '\0', 6);
  pthread_cond_signal(&shm->entrances[1].lpr.condition);
  pthread_mutex_unlock(&shm->entrances[1].lpr.mutex);

  InfoSign *sign = &shm->entrances[1].sign;
  pthread_mutex_lock(&sign->mutex);
  sign->display = '4';
  pthread_cond_signal(&sign->condition);
  pthread_mutex_unlock(&sign->mutex);
  // destroy_shm(shm);
}