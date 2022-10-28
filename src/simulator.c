#include "simulator.h"
#include "delay.h"
#include "display.h"
#include "hashtable.h"
#include "sim_plates.h"
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

// GLOBALS - should try to avoid these but pretty much every function needs them
// ----------------------------------------------------
pthread_mutex_t rand_mutex = PTHREAD_MUTEX_INITIALIZER; // mutex for random
volatile int run = 1; // Whether the program should continue running

volatile int fire = FIRE_OFF; // Whether the fire alarm has been triggered
                              // (triggered through input for testing)
int used_threads = 0; // number of car threads currently running for debug
pthread_mutex_t used_threads_mutex; // mutex for used_threads

NumberPlates *plates; // Linked list of number plates
pthread_mutex_t plate_mutex = PTHREAD_MUTEX_INITIALIZER; // mutex for plate

// CAR UTILITIES
// ----------------------------------------------------
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
  delay_ms(2);
  // signal LPR on the shared memory
  struct Entrance *entrance =
      &car_data->shm->entrances[car_data->entry_queue->id];
  send_licence_plate(car_data->plate, &entrance->lpr);
  int level_id; // index (0-indexed) of level to travel to

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
  } else {
    level_id = -1; // no level given
  }

  // remove self from queue
  // safe to do this because we know the car is at the front of the queue,
  // and has been assigned a level so the entry process is complete
  queue_pop(car_data->entry_queue);
  return level_id;
}

void park_car(ct_data *car_data, int level_id) {
  // travel to the level (10ms)
  delay_ms(10);
  // signal the level that the car is there
  send_licence_plate(car_data->plate, &car_data->shm->levels[level_id].lpr);

  // stay parked for 100-1000ms
  rand_delay_ms(100, 1000, &rand_mutex);
}

void exit_car(ct_data *car_data, int level_id) {
  // signal the level lpr
  send_licence_plate(car_data->plate, &car_data->shm->levels[level_id].lpr);
  // travel to the exit (10ms)
  delay_ms(10);
  // get random exit
  pthread_mutex_lock(&rand_mutex);
  int exit = rand() % NUM_EXITS;
  pthread_mutex_unlock(&rand_mutex);
  // trigger exit lpr
  send_licence_plate(car_data->plate, &car_data->shm->exits[exit].lpr);
  // wait for gate to open
  wait_at_gate(&car_data->shm->exits[exit].gate);
  // we are all done
  return;
}

void *car_handler(void *arg) {
  Queue *car_queue = (Queue *)arg;
  while (run) {
    // get the next car from the queue and pop it
    QItem *car_item = NULL;
    pthread_mutex_lock(&car_queue->mutex);
    while (car_item == NULL && run) {
      pthread_cond_wait(&car_queue->condition, &car_queue->mutex);
      car_item = unsafe_queue_pop_return(
          car_queue); // get the item from the queue, we need to free later
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

    if (level_id >= NUM_LEVELS) {
      perror("Error: level_id is greater than or equal to NUM_LEVELS\n");
      exit(EXIT_FAILURE);
    }
    // if level_id is negative, then the car is not allowed
    // NOTE: this breaks the plate list when evacuating, as there
    // is no way to tell whether the car should put it's plate back
    // in the list during an evacuation
    if (level_id == -1) {
      pthread_mutex_lock(&used_threads_mutex);
      used_threads--;
      pthread_mutex_unlock(&used_threads_mutex);
      free(data);
      free(car_item);
      continue; // ready for next car
    }

    pthread_mutex_lock(&rand_mutex);
    int listen = rand() % 2;
    if (!listen) {
      level_id = rand() % NUM_LEVELS;
    }
    pthread_mutex_unlock(&rand_mutex);

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

void *temp_simulator(void *arg) {
  struct SharedMemory *shm = (struct SharedMemory *)arg;
  int16_t randTempChange;  // a random temperature change to alter temp
  int16_t fixedTempChange; // a specific temperature (e.g from fire to no fire)
  int lastFireType;
  while (run) {
    for (int i = 0; i < NUM_LEVELS; i++) {
      if (fire == FIRE_OFF) // no fire
      {
        // generate a random temperature between 25 and 32
        pthread_mutex_lock(&rand_mutex);
        fixedTempChange = rand() % 8 + 25;
        pthread_mutex_unlock(&rand_mutex);
      } else if (fire == FIRE_FIXED) // fixed temperature fire
      {
        // generate a random temperature between 60 and 67
        pthread_mutex_lock(&rand_mutex);
        fixedTempChange = rand() % 8 + 60;
        pthread_mutex_unlock(&rand_mutex);
      } else if (fire == FIRE_ROR) // ror fire
      {
        if (lastFireType != FIRE_ROR) {
          fixedTempChange = 20;
        } else {
          fixedTempChange = 0;
          // generate a random temperature change between -1 and 2
          pthread_mutex_lock(&rand_mutex);
          randTempChange = rand() % 4 - 1;
          pthread_mutex_unlock(&rand_mutex);
        }
      }
      // update the temperature
      int16_t currTemp = shm->levels[i].temp;
      int16_t newTemp =
          fixedTempChange
              ? fixedTempChange
              : (currTemp + randTempChange) %
                    50; // Make sure rate of rise doesnt go above 50c
      shm->levels[i].temp = newTemp < 99 ? newTemp : 99;
    }
    lastFireType = fire;
    delay_ms(2); // 2ms until next update
  }
  return NULL;
}

void *gate_handler(void *arg) {
  struct Boomgate *gate = (struct Boomgate *)arg;
  while (run || used_threads > 0) {
    pthread_mutex_lock(&gate->mutex);
    while (!(gate->status == 'R' || gate->status == 'L') &&
           (used_threads > 0 || run)) {
      pthread_cond_wait(&gate->condition, &gate->mutex);
    }
    pthread_mutex_unlock(&gate->mutex);
    if (used_threads == 0 && !run) {
      break;
    }
    // gate is now 'R' or 'L
    if (gate->status == 'R') {
      delay_ms(10); // raising takes 10ms
      pthread_mutex_lock(&gate->mutex);
      gate->status = 'O';
      pthread_cond_broadcast(&gate->condition);
    } else if (gate->status == 'L') {
      delay_ms(10); // closing takes 10ms
      pthread_mutex_lock(&gate->mutex);
      gate->status = 'C';
      pthread_cond_broadcast(&gate->condition);
    }
    pthread_mutex_unlock(&gate->mutex);
  }
  // stopped running and all car threads alive
  return NULL;
}

void *input_handler() {
  char input = 'o';
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
    if (input == 'r') {
      fire = FIRE_ROR;
    } else if (input == 'f') {
      fire = FIRE_FIXED;
    } else if (input == 's') {
      fire = FIRE_OFF;
    }
  }
  // reset terminal
  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  system("stty echo");

  run = 0;
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
  plates = list_from_file("plates.txt", &rand_mutex);
  printf("Loaded %zu plates\n", plates->count);

  // create queues for each entry
  Queue *entry_queues[NUM_ENTRANCES];
  for (int i = 0; i < NUM_ENTRANCES; i++) {
    entry_queues[i] = queue_create(i);
  }

  // handle any user input (q) to quit
  pthread_t input_thread;
  pthread_create(&input_thread, NULL, input_handler, NULL);

  // handle the (limited) display for the simulator
  pthread_t display_thread;
  SimDisplayData display_data;
  if (argc < 2 || strcmp(argv[1], "nodisp") != 0) {
    printf("Starting Sim Display\n");
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
    if (i < NUM_ENTRANCES) {
      struct Boomgate *gate = &shm->entrances[i].gate;
      pthread_create(&gate_threads[i], NULL, gate_handler, gate);
    } else {
      struct Boomgate *gate = &shm->exits[i - NUM_ENTRANCES].gate;
      pthread_create(&gate_threads[i], NULL, gate_handler, gate);
    }
  }

  pthread_t car_threads[CAR_THREADS];
  for (int i = 0; i < CAR_THREADS; i++) {
    // create a car thread
    pthread_create(&car_threads[i], NULL, car_handler, car_queue);
  }

  // start temperature simulation
  pthread_t temperature;
  pthread_create(&temperature, NULL, temp_simulator, shm);

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
      data->entry_queue = entry_queues[rand() % NUM_ENTRANCES];
      pthread_mutex_unlock(&rand_mutex);

      data->shm = shm;
      // add the car to the queue
      queue_push(car_queue, data, sizeof(ct_data));
      // free our copy of the car, queue_push makes a copy
      free(data);
    }
    free(plate); // we don't need the plate anymore
    // wait between 1 and 100 ms before creating new car
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
  printf("\033[2J\033[1;1H");
  printf("Car Threads Joined, Used Threads = %d\n", used_threads);
  // signal to gate threads
  // they will check that run is false and there are no more cars alive
  for (int i = 0; i < NUM_ENTRANCES + NUM_EXITS; i++) {
    if (i < NUM_ENTRANCES) {
      pthread_mutex_lock(&shm->entrances[i].gate.mutex);
      pthread_cond_broadcast(&shm->entrances[i].gate.condition);
      pthread_mutex_unlock(&shm->entrances[i].gate.mutex);
    } else {
      pthread_mutex_lock(&shm->exits[i - NUM_ENTRANCES].gate.mutex);
      pthread_cond_broadcast(&shm->exits[i - NUM_ENTRANCES].gate.condition);
      pthread_mutex_unlock(&shm->exits[i - NUM_ENTRANCES].gate.mutex);
    }
  }

  pthread_join(input_thread, NULL);
  printf("Input Thread Joined\n");
  pthread_join(display_thread, NULL);
  printf("Display Thread Joined\n");
  pthread_join(temperature, NULL);
  printf("Temperature Thread Joined\n");

  // destroy the plates
  Plate *plate = plates->head;
  while (plate != NULL) {
    Plate *next = plate->next;
    free(plate);
    plate = next;
  }
  free(plates);
  printf("Plates Destroyed\n");

  // join gate threads
  for (int i = 0; i < (NUM_ENTRANCES + NUM_EXITS); i++) {
    int jres = pthread_join(gate_threads[i], NULL);
    if (jres != 0) {
      perror("Error joining thread");
      exit(EXIT_FAILURE);
    }
  }
  printf("Gate Threads Joined\n");

  // destroy mutexes
  pthread_mutex_destroy(&rand_mutex);
  pthread_mutex_destroy(&plate_mutex);
  pthread_mutex_destroy(&used_threads_mutex);
  // destroy the queues
  for (int i = 0; i < NUM_ENTRANCES; i++) {
    destroy_queue(entry_queues[i]);
  }
  destroy_queue(car_queue);
  printf("Entry Queue Destroyed\n");

  // destroy the shared memory after use
  // can't actually have this as manager may still be using it so it locks up
  // destroy_shm(shm);
  // printf("Shared Memory Destroyed, exiting...\n");

  return 0;
}
