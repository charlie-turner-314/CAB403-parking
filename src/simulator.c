#include "simulator.h"
#include "cars.h"
#include "config.h"
#include "queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Node in a linked list of number plates
typedef struct Plate {
  char plate[7]; // null-terminated plates
  struct Plate *next;
} Plate;

// Structure for storing a linked list of number plates
// and the number of number plates available
typedef struct NumberPlates {
  size_t count;
  Plate *head;
} NumberPlates;

pthread_mutex_t rand_mutex;

// read number plates from a file called "plates.txt"
// store them in a linked list
NumberPlates *read_plates(char *FILENAME) {
  FILE *fp = fopen(FILENAME, "r");
  if (fp == NULL) {
    perror("Error opening file");
    return NULL;
  }
  // create a linked list
  Plate *head = NULL;
  Plate *tail = NULL;
  // read the file line by line
  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;
  size_t count = 0;
  while ((linelen = getline(&line, &linecap, fp)) > 0) {
    line[6] = '\0'; // null-terminate the plate ()
    Plate *new_plate = malloc(sizeof(Plate));
    // copy into plate struct until null terminator or 6 characters
    memccpy(new_plate->plate, line, 0, 7);
    new_plate->next = NULL;
    if (head == NULL) {
      head = new_plate;
      tail = new_plate;
    } else {
      tail->next = new_plate;
      tail = new_plate;
    }
    count++;
  }
  NumberPlates *plates = malloc(sizeof(NumberPlates));
  plates->count = count;
  plates->head = head;
  return plates;
}

// generate a car with a random number plate
// 50% of the time will be within the hashtable
char *random_available_plate(NumberPlates *plates) {
  pthread_mutex_lock(&rand_mutex);
  int allowed = rand() % 2;
  char *plate = calloc(7, sizeof(char));
  if (!plates->count)
    allowed = 0;
  if (allowed) {
    size_t index = rand() % plates->count;
    pthread_mutex_unlock(&rand_mutex); // unlock mutex for rand
    Plate *plate_node = plates->head;
    Plate *prev = NULL;
    // pretty slow, but it works for now
    // TODO: make this faster
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
    // TODO: valgrind says conditional jump or move depends on uninitialised
    // value(s)
    memccpy(plate, plate_node->plate, 0, 7);
    // free the deleted plate
    free(plate_node);
  } else {
    pthread_mutex_unlock(&rand_mutex); // unlock mutex for rand
    // generate random licence plate
    for (int j = 0; j < 6; j++) {
      if (j < 3)
        plate[j] = ("ABCDEFGHIJKLMNOPQRSTUVWXYZ"[rand() % 26]);
      else
        plate[j] = "0123456789"[(rand() % 10)];
    }
    plate[6] = '\0';
  }
  return plate;
}

int main(void) {
  // initialise the shared memory
  SharedMemory *shm = create_shm(SHM_NAME);

  // read allowed plates into a linked list
  // doesn't need to be a hashtable, as we are just grabbing a random plate
  // manager has the hashtable
  NumberPlates *plates = read_plates("plates.txt");
  Queue *entry_queue = queue_create();

  // protect rand with a mutex
  pthread_mutex_init(&rand_mutex, NULL);

  // forever, generate cars
  for (;;) {
    char *plate = random_available_plate(plates);
    // create a car thread
    ct_data *data = calloc(1, sizeof(ct_data));
    memccpy(data->plate, plate, 0, 7);
    data->entry_queue = entry_queue;
    pthread_t car_thread;
    pthread_create(&car_thread, NULL, car_handler, data);
    // wait 1 second
    usleep(1000 * 1000);
  }

  // destroy the hashtable of plates
  // destroy the shared memory after use
  destroy_shm(shm);
  return 0;
}