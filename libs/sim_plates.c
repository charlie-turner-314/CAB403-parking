#include "sim_plates.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// read number plates from a file called "plates.txt"
// store them in a linked list
NumberPlates *list_from_file(char *FILENAME, pthread_mutex_t *rand_mutex) {
  FILE *fp = fopen(FILENAME, "r");
  if (fp == NULL) {
    perror("Error opening file");
    exit(EXIT_FAILURE);
  }
  // read the file line by line
  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;
  // create the linked list
  NumberPlates *plates = malloc(sizeof(NumberPlates));
  if (!plates) {
    perror("Error allocating memory for plates");
    exit(EXIT_FAILURE);
  }
  plates->rand_mutex = rand_mutex;
  pthread_mutex_init(&plates->mutex, NULL);
  plates->count = 0;
  plates->head = NULL;
  // add the plates to the list
  while ((linelen = getline(&line, &linecap, fp)) > 0) {
    line[6] = '\0'; // null-terminate the plate if not already
    // add the plate to the list
    if (!add_plate(plates, line)) {
      printf("Couldn't add plate %s", line);
    }
  }
  free(line);
  fclose(fp);
  return plates;
}

int add_plate(NumberPlates *plates, char *platestr) {
  Plate *plate = calloc(1, sizeof(Plate));
  if (plate == NULL) {
    perror("Error allocating memory for plate");
    exit(EXIT_FAILURE);
  }
  // copy number into plate struct until null terminator or 7 characters
  memccpy(plate->plate, platestr, 0, 7);
  pthread_mutex_lock(&plates->mutex);
  plate->next = plates->head;
  plates->head = plate;
  plates->count += 1;
  pthread_mutex_unlock(&plates->mutex);
  return 1;
}

// generate a car with a random number plate
// 50% of the time will be within the hashtable
char *random_available_plate(NumberPlates *plates) {
  char *plate = calloc(7, sizeof(char));
  if (!plate) {
    perror("Error allocating memory for plate");
    exit(EXIT_FAILURE);
  }
  pthread_mutex_lock(plates->rand_mutex); // ensure access to rand
  int allowed = rand() % 2;
  pthread_mutex_unlock(plates->rand_mutex);
  pthread_mutex_lock(&plates->mutex); // ensure access to the plates
  if (!plates->count)
    allowed = 0;
  if (allowed) {
    pthread_mutex_lock(plates->rand_mutex); // ensure access to rand
    size_t index = rand() % plates->count;
    pthread_mutex_unlock(plates->rand_mutex);
    Plate *plate_node = plates->head;
    Plate *prev = NULL;
    // pretty slow, but it works for now
    // TODO: make this faster, can probably just index in with the size of a
    // plate

    for (size_t i = 0; i < index; i++) {
      prev = plate_node;
      plate_node = plate_node->next;
    }
    // remove the plate from the list
    if (index == 0) {
      plates->head = plate_node->next;
    } else {
      prev->next = plate_node->next;
    }

    plates->count -= 1;

    // set the plate
    memccpy(plate, plate_node->plate, 0, 7);
    // free the deleted plate
    free(plate_node);
  } else {
    // generate random licence plate
    for (int j = 0; j < 6; j++) {
      if (j < 3)
        plate[j] = ("ABCDEFGHIJKLMNOPQRSTUVWXYZ"[rand() % 26]);
      else
        plate[j] = "0123456789"[(rand() % 10)];
    }
    plate[6] = '\0';
  }
  pthread_mutex_unlock(&plates->mutex); // unlock plates mutex
  return plate;
}

int clear_plates(NumberPlates *plates) {
  while (plates->head != NULL) {
    Plate *temp = plates->head;
    plates->head = plates->head->next;
    plates->count -= 1;
    free(temp);
  }
  return 1;
}

int destroy_plates(NumberPlates *plates) {
  clear_plates(plates);
  pthread_mutex_destroy(&plates->mutex);
  free(plates);
  return 1;
}