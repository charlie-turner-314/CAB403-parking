#include "config.h"
#include "display.h"
#include "hashtable.h"
#include "shm_parking.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Assumption
// - Maximum 9 levels -> as each display is only 1 char
//    Implication:
// we can store two digits in one int by by using the first (most significant) 4
// bits for the assigned level and the second four bits for the current level,
// (any platform built after that thing in imitation game should have at least
// 8-bit ints).
//    Reasoning:
// Hashmap stores ints, i don't want to change it to store multiple values to
// deal with unexpected vehicle behaviour
//    Explanation: (abstracted into macros so not that important)
/* e.g
| Assigned level | Current Level |
|     0111       |     0100      |
|      7         |      4        |
|------------ full --------------|
|           01110100             |
|              116               |
to get assigned -> stored_int >> 4
to get current  -> stored_int & 0x0F (0x0F is 00001111 in binary)
*/
// Macros for the above
#define GET_ASSIGNED_LEVEL(x) (x >> 4)
#define GET_CURRENT_LEVEL(x) (x & 0x0F)
#define SET_ASSIGNED_LEVEL(x, y) (x = (x & 0x0F) | (y << 4))
#define SET_CURRENT_LEVEL(x, y) (x = (x & 0xF0) | y)

// Macros because signs display chars and not ints
#define CHAR_TO_INT(c) (c - '0')
#define INT_TO_CHAR(i) (i + '0')

// number of plates to initialise hashtable, can be lower than the actual number
#define EXPECTED_NUM_PLATES 10

pthread_mutex_t rand_mutex;
// mutex for accessing the hashtable of vehicles, hashtable should be fast
// enough to not be a performance problem
pthread_mutex_t plate_hashtable_mutex;
ht_t *number_plates_ht;

// hashtable for storing capacity of each level
pthread_mutex_t capacity_hashtable_mutex;
ht_t *level_capacity_ht;

struct SharedMemory *shm;

struct EntryArgs {
  int id;
};
struct ExitArgs {
  int id;
};

struct LevelArgs {
  int id;
};

// thread-safe access to capacity of a level
int ts_cars_on_level(int l) {
  char *level = malloc(2);
  level[0] = INT_TO_CHAR(l);
  level[1] = '\0';
  int cars;
  pthread_mutex_lock(&capacity_hashtable_mutex);
  cars = htab_get(level_capacity_ht, level);
  pthread_mutex_unlock(&capacity_hashtable_mutex);
  return cars;
}

// thread-safe setting of level capacity
int ts_add_cars_to_level(int l, int num_cars) {
  // turn level into null-terminated string
  char level[2];
  level[0] = INT_TO_CHAR(l);
  level[1] = '\0';
  int cars;
  pthread_mutex_lock(&capacity_hashtable_mutex);
  cars = htab_get(level_capacity_ht, level);
  cars += num_cars;
  htab_set(level_capacity_ht, level, cars);
  pthread_mutex_unlock(&capacity_hashtable_mutex);
  return cars;
}

// thread-safe access to the number plates
int ts_get_number_plate(char *plate) {
  int value;
  // ensure the plate is null-terminated
  char null_terminated_plate[7];
  strncpy(null_terminated_plate, plate, 7);
  null_terminated_plate[6] = '\0';
  // ----------
  pthread_mutex_lock(&plate_hashtable_mutex);
  value = htab_get(number_plates_ht, null_terminated_plate);
  pthread_mutex_unlock(&plate_hashtable_mutex);
  return value;
}

// thread-safe allocation to a level
bool ts_set_assigned_level(char *plate, int level) {
  // ensure the plate is null-terminated
  char null_terminated_plate[7];
  strncpy(null_terminated_plate, plate, 7);
  null_terminated_plate[6] = '\0';
  // ----------
  bool success;
  pthread_mutex_lock(&plate_hashtable_mutex);
  int current_value = htab_get(number_plates_ht, null_terminated_plate);
  int new_value = SET_ASSIGNED_LEVEL(current_value, level);
  success = htab_set(number_plates_ht, null_terminated_plate, new_value);
  pthread_mutex_unlock(&plate_hashtable_mutex);
  return success;
}

// thread-safe allocation to current level
bool ts_set_current_level(char *plate, int level) {
  // ensure the plate is null-terminated
  char null_terminated_plate[7];
  strncpy(null_terminated_plate, plate, 7);
  null_terminated_plate[6] = '\0';
  // ----------
  bool success;
  pthread_mutex_lock(&plate_hashtable_mutex);
  int current_value = htab_get(number_plates_ht, null_terminated_plate);
  int new_value = SET_CURRENT_LEVEL(current_value, level);
  success = htab_set(number_plates_ht, null_terminated_plate, new_value);
  pthread_mutex_unlock(&plate_hashtable_mutex);
  return success;
}

// read each line of a file into a hashtable, initialising their value to 0
ht_t *ht_from_file(char *filename) {
  puts(filename);
  FILE *fp = fopen(filename, "r");
  if (fp == NULL) {
    perror("Error opening file");
    return NULL;
  }
  // create a hashtable
  ht_t *ht = NULL;
  ht = htab_create(ht, EXPECTED_NUM_PLATES);
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

// Wait at the LPR for a licence plate to be written
void wait_for_lpr(struct LPR *lpr) {
  // wait at the given LPR for anything other than NULL to be written
  pthread_mutex_lock(&lpr->mutex);
  while (lpr->plate[0] == 0) { // while the lpr is empty
    pthread_cond_wait(&lpr->condition, &lpr->mutex);
  }
  pthread_mutex_unlock(&lpr->mutex);
}

void *entry_handler(void *arg) {
  struct EntryArgs *args = (struct EntryArgs *)arg;
  int id = args->id;
  while (true) {
    struct Entrance *entrance =
        &shm->entrances[id]; // The corresponding entrance
    // wait for a car to arrive at the LPR
    wait_for_lpr(&entrance->lpr);
    // should be a licence plate there now, so read it
    char *plate = entrance->lpr.plate;
    // check if the car is in the hashtable (and not already in the car park)
    int value = ts_get_number_plate(plate);
    char level;
    // TODO: other cases e.g full, think about number of cars on each level
    if (value == 0) { // not already in but allowed
      // set info sign to a random level
      pthread_mutex_lock(&rand_mutex);
      level = rand() % NUM_LEVELS; // level index
      pthread_mutex_unlock(&rand_mutex);
      // increment the level capacity
      ts_add_cars_to_level(level, 1);
      // assign the car to the level
      ts_set_assigned_level(plate, level);
      // set the level to the character representation
      level = INT_TO_CHAR(level + 1); // level offset by 1 for display

    } else { // not allowed
      level = 'X';
    }
    // set the sign
    pthread_mutex_lock(&entrance->sign.mutex);
    entrance->sign.display = level;
    pthread_cond_signal(&entrance->sign.condition);
    pthread_mutex_unlock(&entrance->sign.mutex);

    // open the gate if the car is allowed
    // TODO: other cases
    if (level != 'X') {
      pthread_mutex_lock(&entrance->gate.mutex);
      // TODO: close the gate after a delay of 20ms
      entrance->gate.status = 'R';
      pthread_cond_signal(&entrance->gate.condition);
      pthread_mutex_unlock(&entrance->gate.mutex);
    }

    // clear the LPR
    pthread_mutex_lock(&entrance->lpr.mutex);
    memset(entrance->lpr.plate, '\0', 6);
    pthread_mutex_unlock(&entrance->lpr.mutex);
  }
  return NULL;
}

void *level_handler(void *arg) {
  int *lid = (int *)arg;
  int level_id = *lid;
  // forever stuck checking for cars
  while (true) {
    // wait at the lpr for a car to arrive
    struct Level *level = &shm->levels[level_id];
    wait_for_lpr(&level->lpr);
    // read the plate
    char *plate = level->lpr.plate;
    // check if they are entering or exiting
    int value = ts_get_number_plate(plate);
    int assigned = GET_ASSIGNED_LEVEL(value);
    int current = GET_CURRENT_LEVEL(value);
    if (current != 0) {
      // they are already on a level
      if (current == level_id) {
        // they must be on this level and leaving
        // decrement the level capacity
        ts_add_cars_to_level(level_id, -1);
        // car from the level
        ts_set_current_level(plate, 0);
      } else {
        // something went real wrong
        perror("Car teleported to different level\n");
        exit(EXIT_FAILURE);
      }
    } else if (assigned != level_id) {
      // they are on the wrong level, re-assign them and let them in
      // TODO: checking if the level is full
      ts_add_cars_to_level(assigned, -1);
      ts_add_cars_to_level(level_id, 1);
    }
    // set the current level for the car
    ts_set_current_level(plate, level_id);

    // clear the lpr
    pthread_mutex_lock(&level->lpr.mutex);
    memset(level->lpr.plate, '\0', 6);
    pthread_cond_signal(&level->lpr.condition);
    pthread_mutex_unlock(&level->lpr.mutex);
  }
}

void *exit_handler(void *arg) {
  struct ExitArgs *args = (struct ExitArgs *)arg;
  int id = args->id;
  while (true) {
    struct Exit *exit = &shm->exits[id]; // The corresponding exit
    // wait for a car to arrive at the LPR
    wait_for_lpr(&exit->lpr);
    // should be a licence plate there now, so read it
    char *plate = exit->lpr.plate;

    // open the gate
    pthread_mutex_lock(&exit->gate.mutex);
    exit->gate.status = 'R';
    pthread_cond_signal(&exit->gate.condition);
    pthread_mutex_unlock(&exit->gate.mutex);
    // clear the LPR
    pthread_mutex_lock(&exit->lpr.mutex);
    memset(exit->lpr.plate, '\0', 6);
    pthread_cond_signal(&exit->lpr.condition);
    pthread_mutex_unlock(&exit->lpr.mutex);

    // car left, unassign them from the carpark
    ts_set_assigned_level(plate, 0);
    ts_set_current_level(plate, 0);
  }
}

int main(void) {
  // initialise local mutexes
  pthread_mutex_init(&rand_mutex, NULL);
  pthread_mutex_init(&plate_hashtable_mutex, NULL);
  pthread_mutex_init(&capacity_hashtable_mutex, NULL);

  // get the shared memory object
  shm = get_shm(SHM_NAME);

  // read the allowed number plates from a file into hashtable
  number_plates_ht = ht_from_file("plates.txt");

  // initialise level capacity hashtable
  level_capacity_ht = htab_create(level_capacity_ht, NUM_LEVELS);
  for (int i = 0; i < NUM_LEVELS; i++) {
    char *level = malloc(2);
    level[0] = INT_TO_CHAR(i);
    level[1] = '\0';
    htab_set(level_capacity_ht, level, 0);
  }

  // create entrance threads
  // -------------------------------
  pthread_t *entry_threads[NUM_ENTRANCES];
  struct EntryArgs *entry_args[NUM_ENTRANCES];
  for (int i = 0; i < NUM_ENTRANCES; i++) {
    struct EntryArgs *args = calloc(1, sizeof(struct EntryArgs));
    entry_args[i] = args;
    args->id = i;
    pthread_t thread;
    // pass in i
    pthread_create(&thread, NULL, entry_handler, args);
    entry_threads[i] = &thread;
  }
  // create level threads
  // -------------------------------
  pthread_t *level_threads[NUM_LEVELS];
  struct LevelArgs *level_args[NUM_LEVELS];
  for (int i = 0; i < NUM_ENTRANCES; i++) {
    struct LevelArgs *args = calloc(1, sizeof(struct LevelArgs));
    level_args[i] = args;
    args->id = i;
    pthread_t thread;
    // pass in i
    pthread_create(&thread, NULL, level_handler, args);
    entry_threads[i] = &thread;
  }
  // create exit threads
  // -------------------------------
  pthread_t *exit_threads[NUM_EXITS];
  struct ExitArgs *exit_args[NUM_EXITS];
  for (int i = 0; i < NUM_EXITS; i++) {
    struct ExitArgs *args = calloc(1, sizeof(struct ExitArgs));
    exit_args[i] = args;
    args->id = i;
    pthread_t thread;
    // pass in i
    pthread_create(&thread, NULL, exit_handler, args);
    exit_threads[i] = &thread;
  }

  pthread_t display_thread;
  pthread_create(&display_thread, NULL, display_handler, NULL);

  // wait for threads to finish and clean up their resources
  for (int i = 0; i < NUM_ENTRANCES; i++) {
    pthread_join(*entry_threads[i], NULL);
    free(entry_args[i]);
  }

  for (int i = 0; i < NUM_LEVELS; i++) {
    pthread_join(*level_threads[i], NULL);
    free(level_args[i]);
  }
  for (int i = 0; i < NUM_EXITS; i++) {
    pthread_join(*exit_threads[i], NULL);
    free(exit_args[i]);
  }

  // set the entrance 4 entry sign to '4'
  // printf("Plate is: %.6s\n", shm->entrances[1].lpr.plate);
  // // set the plate to NULL characters
  // pthread_mutex_lock(&shm->entrances[1].lpr.mutex);
  // memset(shm->entrances[1].lpr.plate, '\0', 6);
  // pthread_cond_signal(&shm->entrances[1].lpr.condition);
  // pthread_mutex_unlock(&shm->entrances[1].lpr.mutex);

  // InfoSign *sign = &shm->entrances[1].sign;
  // pthread_mutex_lock(&sign->mutex);
  // sign->display = '4';
  // pthread_cond_signal(&sign->condition);
  // pthread_mutex_unlock(&sign->mutex);
  // destroy_shm(shm);

  // - Threads for each entry/exit/level
  // - Thread for display
}