#include "simulator.h"
#include "config.h"
#include "delay.h"
#include "queue.h"
#include "shm_parking.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// set to zero to run indefinitely
#define MAX_CARS 2

// Node in a linked list of number plates
typedef struct Plate {
  char plate[7]; // null-terminated plates
  struct Plate *next;
} Plate;

pthread_mutex_t rand_mutex;

// Structure for storing a linked list of number plates
// and the number of number plates available
typedef struct NumberPlates {
  size_t count;
  Plate *head;
} NumberPlates;

NumberPlates *plates;

int add_plate(NumberPlates *plates, char *platestr) {
  Plate *plate = malloc(sizeof(Plate));
  if (plate == NULL) {
    perror("Error allocating memory for plate");
    return 0;
  }
  // copy number into plate struct until null terminator or 7 characters
  memccpy(plate->plate, platestr, 0, 7);
  plate->next = plates->head;
  plates->head = plate;
  plates->count++;
  return 1;
}

// read number plates from a file called "plates.txt"
// store them in a linked list
NumberPlates *list_from_file(char *FILENAME) {
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
    printf("index is %zu\n", index);
    pthread_mutex_unlock(&rand_mutex); // unlock mutex for rand
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
    pthread_mutex_unlock(&rand_mutex); // unlock mutex for rand
  }
  return plate;
}

void wait_at_gate(Boomgate *gate) {
  // wait for exit gate to be raising
  pthread_mutex_lock(&gate->mutex);
  while (gate->status != 'R' && gate->status != 'O') {
    pthread_cond_wait(&gate->condition, &gate->mutex);
  }

  // if the gate has been opened by another thread, just drive through at the
  // same time causing a horrible collision
  if (gate->status == 'O') {
    pthread_mutex_unlock(&gate->mutex);
    return;
  }
  printf("Gate is being raised\n");
  // gate is 'R' -> wait 10ms for gate to open
  rand_delay_ms(10, 10, &rand_mutex);
  // open the gate
  gate->status = 'O';
  printf("Opened the gate\n");
  pthread_cond_broadcast(&gate->condition);
  pthread_mutex_unlock(&gate->mutex);
}

void send_licence_plate(char *plate, LPR *lpr) {
  pthread_mutex_lock(&lpr->mutex);
  // wait for level lpr to be free (cleared by manager)
  while (lpr->plate[0] != '\0') {
    pthread_cond_wait(&lpr->condition, &lpr->mutex);
  }
  // write the car's plate to the level lpr
  memccpy(lpr->plate, plate, 0, 6);
  // broadcast to all threads waiting on the level lpr and unlock mutex
  pthread_cond_broadcast(&lpr->condition);
  pthread_mutex_unlock(&lpr->mutex);
}

// car is at front of queue
int attempt_entry(ct_data *car_data) {
  // signal LPR on the shared memory
  printf("%s Attempting to enter at entrance: %d\n", car_data->plate,
         car_data->entry_queue->id);
  // wait 2ms
  rand_delay_ms(2, 2, &rand_mutex);
  Entrance *entrance = &car_data->shm->entrances[car_data->entry_queue->id];
  send_licence_plate(car_data->plate, &entrance->lpr);
  int level;
  // wait on the entrance sign
  printf("Waiting on condition for entrance %d\n", car_data->entry_queue->id);
  pthread_mutex_lock(&entrance->sign.mutex);
  pthread_cond_wait(&entrance->sign.condition, &entrance->sign.mutex);
  printf("Condition Signaled\n");
  char display = entrance->sign.display;
  pthread_mutex_unlock(&entrance->sign.mutex);

  printf("display is %c\n", display);
  if (display > '0' && display < '9') { // level number
    level = display - '0';              // convert to int
    printf("Level is %d\n", level);
    // wait at gate if given a level
    wait_at_gate(&entrance->gate);
  } else { // Full or Not allowed Or Evacuating
    level = -1;
  }
  // remove self from queue
  queue_pop(car_data->entry_queue);
  return level;
}

void park_car(ct_data *car_data, int level) {
  // travel to the level (10ms)
  rand_delay_ms(10, 10, &rand_mutex);
  // signal the level that the car is there
  send_licence_plate(car_data->plate, &car_data->shm->levels[level].lpr);

  // stay parked for 100-1000ms
  rand_delay_ms(100, 1000, &rand_mutex);
}

void exit_car(ct_data *car_data, int level) {
  // signal the level lpr
  send_licence_plate(car_data->plate, &car_data->shm->levels[level].lpr);
  // travel to the exit (10ms)
  rand_delay_ms(10, 10, &rand_mutex);
  // get random exit
  pthread_mutex_lock(&rand_mutex);
  int exit = rand() % NUM_EXITS;
  pthread_mutex_unlock(&rand_mutex);
  printf("Car %.6s attempting to exit at exit %d\n", car_data->plate, exit);
  // trigger exit lpr
  pthread_mutex_lock(&car_data->shm->exits[exit].lpr.mutex);
  // wait for exit lpr to be free (cleared by manager)
  while (car_data->shm->exits[exit].lpr.plate[0] != '\0') {
    pthread_cond_wait(&car_data->shm->exits[exit].lpr.condition,
                      &car_data->shm->exits[exit].lpr.mutex);
  }
  // write the car's plate to the exit lpr
  memccpy(car_data->shm->exits[exit].lpr.plate, car_data->plate, 0, 6);
  // broadcast to any threads waiting on the exit lpr and unlock mutex
  pthread_cond_broadcast(&car_data->shm->exits[exit].lpr.condition);
  pthread_mutex_unlock(&car_data->shm->exits[exit].lpr.mutex);
  // wait for gate to open
  wait_at_gate(&car_data->shm->exits[exit].gate);
  // we are all done
  return;
}

// car given a number plate and an entrance queue
void *car_handler(void *arg) {
  ct_data *data = (ct_data *)arg;
  printf("Car %s has started\n", data->plate);
  // add self to entrance queue
  queue_push(data->entry_queue, data->plate);
  printf("Car %s added to queue %d\n", data->plate, data->entry_queue->id);
  // wait until front of queue
  // while not at front of queue
  pthread_mutex_lock(&data->entry_queue->mutex);
  while (strcmp(queue_peek(data->entry_queue)->value, data->plate) != 0) {
    pthread_cond_wait(&data->entry_queue->condition, &data->entry_queue->mutex);
  }
  pthread_mutex_unlock(&data->entry_queue->mutex);
  printf("%s made it to the front of the queue\n", data->plate);
  int level = attempt_entry(data);
  if (level == -1) {
    printf("%s was not allowed to enter, disintegrating\n", data->plate);
    return NULL;
  }
  printf("%s allowed to go to level %d\n", data->plate, level);

  // park the car on the given level
  park_car(data, level);

  // exit the carpark
  exit_car(data, level);

  printf("Car %s is disintegrating\n", data->plate);
  // add licence plate back in the available pool
  add_plate(plates, data->plate);
  return NULL;
}

int main(int argc, char *argv[]) {
  // allow passing in a seed for rand
  // the seed '3141592' nicely queues the first two cars at entrances[1] for
  // debugging
  if (argc > 1) {
    int c;
    c = atoi(argv[1]);
    srand(c);
  }
  // initialise the shared memory
  SharedMemory *shm = create_shm(SHM_NAME);

  // protect rand with a mutex
  pthread_mutex_init(&rand_mutex, NULL);

  // read allowed plates into a linked list
  // doesn't need to be a hashtable, as we are just grabbing a random plate
  // manager has the hashtable
  plates = list_from_file("plates.txt");
  printf("Loaded %zu plates\n", plates->count);

  // create queues for each entry
  Queue *entry_queues[NUM_ENTRANCES];
  for (int i = 0; i < NUM_ENTRANCES; i++) {
    entry_queues[i] = queue_create(i);
  }

  // forever, generate cars
  int num_cars = 0;
  for (;;) {
    if (MAX_CARS && num_cars > MAX_CARS) {    // for debugging
      rand_delay_ms(1000, 1000, &rand_mutex); // sleep so not using 100% cpu
      continue;
    }
    char *plate = random_available_plate(plates);
    // create a car thread
    ct_data *data = calloc(1, sizeof(ct_data));
    memccpy(data->plate, plate, 0, 7);

    pthread_mutex_lock(&rand_mutex);
    data->entry_queue = entry_queues[rand() % NUM_LEVELS];
    pthread_mutex_unlock(&rand_mutex);

    data->rand_mutex = &rand_mutex;
    data->shm = shm;
    pthread_t car_thread;
    pthread_create(&car_thread, NULL, car_handler, data);
    num_cars++;
    // wait between 1 and 100 ms
    rand_delay_ms(1, 100, &rand_mutex);
  }

  // destroy the hashtable of plates
  // destroy the shared memory after use
  destroy_shm(shm);
  return 0;
}