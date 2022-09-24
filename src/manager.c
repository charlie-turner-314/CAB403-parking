#include "config.h"
#include "hashtable.h"
#include "shm_parking.h"
#include <stdio.h>

// number of plates to initialise hashtable, can be lower than the actual number
#define EXPECTED_NUM_PLATES 10

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
    if (line[linelen - 1] == '\n') {
      line[linelen - 1] = '\0';
    }
    htab_set(ht, line, 0);
  }
  return ht;
}

int main(void) {
  SharedMemory *shm = get_shm(SHM_NAME);
  printf("The value of the plate is %.6s\n", shm->entrances[0].lpr.plate);
  destroy_shm(shm);
}