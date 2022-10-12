#include "simulator.h"
#include "config.h"
#include "delay.h"
#include "display.h"
#include "queue.h"
#include "shm_parking.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

// number of possible cars at any one time,
// should be okay having heaps of these, cause most threads are sleeping most of
// the time. Allow for entire carpark capacity to be filled with cars as well as
// a large queue
#define CAR_THREADS (NUM_LEVELS * LEVEL_CAPACITY * 2)

// get's set to 0 when the simulation is over
int run = 1;
int used_threads = 0;
pthread_mutex_t used_threads_mutex;
pthread_mutex_t rand_mutex;

pthread_mutex_t plate_mutex = PTHREAD_MUTEX_INITIALIZER;
// Node in a linked list of number plates
typedef struct Plate {
  char plate[7]; // null-terminated plates
  struct Plate *next;
} Plate;

// Structure for storing a linked list of number plates
// and the number of number plates available
typedef struct NumberPlates {
  pthread_mutex_t mutex;
  size_t count;
  Plate *head;
} NumberPlates;

NumberPlates *plates;

int add_plate(NumberPlates *plates, char *platestr) {
  Plate *plate = malloc(sizeof(Plate));
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
  if (!plates) {
    perror("Error allocating memory for plates");
    exit(EXIT_FAILURE);
  }
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

// generate a car with a random number plate
// 50% of the time will be within the hashtable
char *random_available_plate(NumberPlates *plates) {
  char *plate = calloc(7, sizeof(char));
  if (!plate) {
    perror("Error allocating memory for plate");
    exit(EXIT_FAILURE);
  }
  pthread_mutex_lock(&rand_mutex); // ensure access to rand
  int allowed = rand() % 2;
  pthread_mutex_unlock(&rand_mutex);
  pthread_mutex_lock(&plates->mutex); // ensure access to the plates
  if (!plates->count)
    allowed = 0;
  if (allowed) {
    pthread_mutex_lock(&rand_mutex); // ensure access to rand
    size_t index = rand() % plates->count;
    pthread_mutex_unlock(&rand_mutex);
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

void wait_at_gate(struct Boomgate *gate) {
  // wait for exit gate to be open
  pthread_mutex_lock(&gate->mutex);
  while (gate->status != 'O') {
    pthread_cond_wait(&gate->condition, &gate->mutex);
  }
  pthread_mutex_unlock(&gate->mutex);
}

void send_licence_plate(char *plate, struct LPR *lpr) {
  pthread_mutex_lock(&lpr->mutex);
  // wait for level lpr to be free (cleared by manager)
  while (lpr->plate[0] != '\0') {
    pthread_cond_wait(&lpr->condition, &lpr->mutex);
  }
  // write the car's plate to the level lpr
  memccpy(lpr->plate, plate, 0, 6);
  // broadcast to threads waiting on the level lpr and unlock mutex
  pthread_cond_broadcast(&lpr->condition);
  pthread_mutex_unlock(&lpr->mutex);
}

// car is at front of queue
int attempt_entry(ct_data *car_data) {
  // wait 2ms
  rand_delay_ms(2, 2, &rand_mutex);
  // signal LPR on the shared memory
  struct Entrance *entrance =
      &car_data->shm->entrances[car_data->entry_queue->id];
  send_licence_plate(car_data->plate, &entrance->lpr);
  int level_id;
  // wait 2ms for sign to update
  rand_delay_ms(2, 2, &rand_mutex);
  // wait on the entrance sign
  pthread_mutex_lock(&entrance->sign.mutex);
  while (entrance->sign.display == '\0') {
    pthread_cond_wait(&entrance->sign.condition, &entrance->sign.mutex);
  }
  char display = entrance->sign.display;
  pthread_mutex_unlock(&entrance->sign.mutex);

  if (display > '0' && display <= '9') { // level number
    level_id = display - '1';            // convert to level index
    // wait at gate if given a level
    wait_at_gate(&entrance->gate);
  } else { // Full or Not allowed Or Evacuating
    level_id = -1;
  }
  // remove self from queue
  // safe to do this because we know the car is at the front of the queue,
  // and has been assigned a level
  queue_pop(car_data->entry_queue);
  return level_id;
}

void park_car(ct_data *car_data, int level_id) {
  // travel to the level (10ms)
  rand_delay_ms(10, 10, &rand_mutex);
  // signal the level that the car is there
  send_licence_plate(car_data->plate, &car_data->shm->levels[level_id].lpr);

  // stay parked for 100-1000ms
  rand_delay_ms(100, 1000, &rand_mutex);
}

void exit_car(ct_data *car_data, int level_id) {
  // signal the level lpr
  send_licence_plate(car_data->plate, &car_data->shm->levels[level_id].lpr);
  // travel to the exit (10ms)
  rand_delay_ms(10, 10, &rand_mutex);
  // get random exit
  pthread_mutex_lock(&rand_mutex);
  int exit = rand() % NUM_EXITS;
  pthread_mutex_unlock(&rand_mutex);
  // trigger exit lpr
  send_licence_plate(car_data->plate, &car_data->shm->exits[exit].lpr);
  // pthread_mutex_lock(&car_data->shm->exits[exit].lpr.mutex);
  // // wait for exit lpr to be free (cleared by manager)
  // while (car_data->shm->exits[exit].lpr.plate[0] != '\0') {
  //   pthread_cond_wait(&car_data->shm->exits[exit].lpr.condition,
  //                     &car_data->shm->exits[exit].lpr.mutex);
  // }
  // // write the car's plate to the exit lpr
  // memccpy(car_data->shm->exits[exit].lpr.plate, car_data->plate, 0, 6);
  // // broadcast to any threads waiting on the exit lpr and unlock mutex
  // pthread_cond_broadcast(&car_data->shm->exits[exit].lpr.condition);
  // pthread_mutex_unlock(&car_data->shm->exits[exit].lpr.mutex);
  // wait for gate to open
  wait_at_gate(&car_data->shm->exits[exit].gate);
  // we are all done
  return;
}

// Car_handler is given the queue of cars, and takes one when available
// - deals with the entire lifespan of the car in the carpark
// - waits for another car in the queue
void *car_handler(void *arg) {
  Queue *car_queue = (Queue *)arg;
  while (run) {
    // get the next car from the queue and pop it
    QItem *car_item = NULL;
    pthread_mutex_lock(&car_queue->mutex);
    while (car_item == NULL && run) {
      pthread_cond_wait(&car_queue->condition, &car_queue->mutex);
      car_item = unsafe_queue_pop_return(car_queue);
    }
    pthread_mutex_unlock(&car_queue->mutex);
    if (!run) {
      break;
    }
    // we got a car
    pthread_mutex_lock(&used_threads_mutex);
    used_threads++;
    pthread_mutex_unlock(&used_threads_mutex);
    ct_data *data = (ct_data *)car_item->value;
    // add self to entrance queue (size 7 as 6 characters on the plate + pad
    // with null)
    queue_push(data->entry_queue, data->plate, 7);
    // wait until front of queue
    // while not at front of queue
    pthread_mutex_lock(&data->entry_queue->mutex);
    while (strcmp(queue_peek(data->entry_queue)->value, data->plate) != 0) {
      pthread_cond_wait(&data->entry_queue->condition,
                        &data->entry_queue->mutex);
    }
    pthread_mutex_unlock(&data->entry_queue->mutex);

    // Assigned level, or -1 if not allowed
    int level_id = attempt_entry(data);

    // +++ Not Touching Shared Memory +++
    if (level_id >= NUM_LEVELS) {
      perror("Error: level_id is greater than or equal to NUM_LEVELS\n");
      exit(EXIT_FAILURE);
    }
    // if level_id is -1 then the car is not allowed in
    if (level_id == -1) {
      pthread_mutex_lock(&used_threads_mutex);
      used_threads--;
      pthread_mutex_unlock(&used_threads_mutex);
      free(data);
      free(car_item);
      continue;
    }

    pthread_mutex_lock(&rand_mutex);
    int listen = rand() % 2;
    if (!listen) {
      level_id = rand() % NUM_LEVELS;
    }
    pthread_mutex_unlock(&rand_mutex);
    // +++ END not Touching Shared Memory +++

    // park the car on the given level

    park_car(data, level_id);

    // exit the carpark
    exit_car(data, level_id);

    // add licence plate back in the available pool
    add_plate(plates, data->plate);
    free(data);
    free(car_item);

    // update used threads
    pthread_mutex_lock(&used_threads_mutex);
    used_threads--;
    pthread_mutex_unlock(&used_threads_mutex);
  }
  return NULL;
}

char input = 'o';

void *input_handler() {
  // setup terminal to read character without pressing enter
  struct termios oldt, newt;
  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~(ICANON);
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  // setup terminal to not echo input to the screen
  system("stty -echo");

  while (input != 'q') {
    input = getchar();
  }
  // clear the screen
  printf("\033[2J\033[1;1H");
  // reset terminal
  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  system("stty echo");

  run = 0;
  return NULL;
}

void *gate_handler(void *arg) {
  struct Boomgate *gate = (struct Boomgate *)arg;
  while (run || used_threads > 0) {
    pthread_mutex_lock(&gate->mutex);
    while (!(gate->status == 'R' || gate->status == 'L') && used_threads > 0) {
      pthread_cond_wait(&gate->condition, &gate->mutex);
    }
    // gate is now 'R' or 'L
    if (gate->status == 'R') {
      rand_delay_ms(10, 10, &rand_mutex); // raising takes 10ms
      gate->status = 'O';
      pthread_cond_broadcast(&gate->condition);
    } else {
      rand_delay_ms(10, 10, &rand_mutex); // closing takes 10ms
      gate->status = 'C';
      pthread_cond_broadcast(&gate->condition);
    }
    pthread_mutex_unlock(&gate->mutex);
  }
  // stopped running and all car threads alive
  return NULL;
}

int main(int argc, char *argv[]) {
  srand(time(NULL));
  // initialise the shared memory
  struct SharedMemory *shm = create_shm(SHM_NAME);

  // protect rand with a mutex
  pthread_mutex_init(&rand_mutex, NULL);

  // num used threads
  pthread_mutex_init(&used_threads_mutex, NULL);

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

  // handle any user input (q) to quit
  pthread_t input_thread;
  pthread_create(&input_thread, NULL, input_handler, &input);

  // handle the (limited) display for the simulator
  pthread_t display_thread;
  if (argc < 2 || strcmp(argv[1], "nodisp") != 0) {
    printf("Starting Sim Display\n");
    SimDisplayData display_data;
    display_data.num_cars = &used_threads;
    display_data.entry_queues = entry_queues;
    display_data.running = &run;
    display_data.available_plates = &plates->count;
    pthread_create(&display_thread, NULL, sim_display_handler, &display_data);
  }

  Queue *car_queue = queue_create(0);

  // threads who's jobs are to just look at boomgates and open/close them
  // depending on the manager
  pthread_t gate_threads[NUM_ENTRANCES + NUM_EXITS];
  for (int i = 0; i < NUM_ENTRANCES + NUM_EXITS; i++) {
    pthread_t thread;
    if (i < NUM_ENTRANCES) {
      struct Boomgate *gate = &shm->entrances[i].gate;
      pthread_create(&thread, NULL, gate_handler, gate);
    } else {
      struct Boomgate *gate = &shm->exits[i - NUM_ENTRANCES].gate;
      pthread_create(&thread, NULL, gate_handler, gate);
    }
    gate_threads[i] = thread;
  }

  pthread_t car_threads[CAR_THREADS];
  for (int i = 0; i < CAR_THREADS; i++) {
    // create a car thread
    pthread_create(&car_threads[i], NULL, car_handler, car_queue);
  }

  while (run) {
    char *plate = random_available_plate(plates);
    if (plate) {
      // create a car thread
      ct_data *data = calloc(1, sizeof(ct_data));
      if (!data) {
        perror("Calloc car data");
        exit(EXIT_FAILURE);
      }
      memccpy(data->plate, plate, 0, 7);

      pthread_mutex_lock(&rand_mutex);
      data->entry_queue = entry_queues[rand() % NUM_LEVELS];
      pthread_mutex_unlock(&rand_mutex);

      data->shm = shm;
      // add the car to the queue
      queue_push(car_queue, data, sizeof(ct_data));
      // free our copy of the car, queue_push makes a copy
      free(data);
    }
    free(plate); // we don't need the plate anymore
    // wait between 1 and 100 ms
    rand_delay_ms(1, 100, &rand_mutex);
  }
  // join the threads
  // broadcast to the car_queue to wake up all the threads
  // that might be waiting for a car to enter the queue
  // and let them know to exit
  printf("Attempting To Join Car Threads\n");
  pthread_mutex_lock(&car_queue->mutex);
  pthread_cond_broadcast(&car_queue->condition);
  pthread_mutex_unlock(&car_queue->mutex);

  for (int i = 0; i < CAR_THREADS; i++) {
    int jres = pthread_join(car_threads[i], NULL);
    if (jres != 0) {
      perror("Error joining thread");
      exit(EXIT_FAILURE);
    }
  }
  printf("Car Threads Joined\n");
  // signal to gate threads
  // they will check that run is false and there are no more cars alive
  for (int i = 0; i < NUM_ENTRANCES + NUM_EXITS; i++) {
    if (i < NUM_ENTRANCES) {
      pthread_mutex_lock(&shm->entrances[i].gate.mutex);
      pthread_cond_broadcast(&shm->entrances[i].gate.condition);
      pthread_mutex_unlock(&shm->entrances[i].gate.mutex);
    } else {
      pthread_mutex_lock(&shm->exits[i].gate.mutex);
      pthread_cond_broadcast(&shm->exits[i].gate.condition);
      pthread_mutex_unlock(&shm->exits[i].gate.mutex);
    }
  }

  pthread_join(input_thread, NULL);
  pthread_join(display_thread, NULL);

  // destroy the plates
  Plate *plate = plates->head;
  while (plate != NULL) {
    Plate *next = plate->next;
    free(plate);
    plate = next;
  }
  free(plates);

  // join gate threads
  for (int i = 0; i < NUM_ENTRANCES + NUM_EXITS; i++) {
    int jres = pthread_join(gate_threads[i], NULL);
    if (jres != 0) {
      perror("Error joining thread");
      exit(EXIT_FAILURE);
    }
  }

  // destroy mutexes
  pthread_mutex_destroy(&rand_mutex);
  // destroy the queues
  for (int i = 0; i < NUM_ENTRANCES; i++) {
    destroy_queue(entry_queues[i]);
  }
  destroy_queue(car_queue);
  // destroy the shared memory after use
  destroy_shm(shm);

  return 0;
}