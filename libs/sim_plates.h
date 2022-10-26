#pragma once
#include <pthread.h>

// Node in a linked list of number plates
typedef struct Plate {
  char plate[7]; // null-terminated plates
  struct Plate *next;
} Plate;

// Structure for storing a linked list of number plates
// and the number of number plates available
typedef struct NumberPlates {
  pthread_mutex_t *rand_mutex;
  pthread_mutex_t mutex;
  size_t count;
  Plate *head;
} NumberPlates;

int add_plate(NumberPlates *plates, char *platestr);

NumberPlates *list_from_file(char *FILENAME, pthread_mutex_t *rand_mutex);

char *random_available_plate(NumberPlates *plates);

int clear_plates(NumberPlates *plates);

int destroy_plates(NumberPlates *plates);
