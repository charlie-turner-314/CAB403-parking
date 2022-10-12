#include "config.h"
#include "delay.h"
#include "display.h"
#include "hashtable.h"
#include "shm_parking.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
// Limitation: would need to one-index to avoid confusion with 0,
// get around this by storing unassigned as FF (11111111), as outside
// the predefined range of 0-9
*/
// Macros for the above
#define GET_ASSIGNED_LEVEL(x) (x >> 4)
#define GET_CURRENT_LEVEL(x) (x & 0x0F)
#define SET_ASSIGNED_LEVEL(x, y) (x = (x & 0x0F) | (y << 4))
#define SET_CURRENT_LEVEL(x, y) (x = (x & 0xF0) | y)
#define NO_LEVEL 0xF

// Macros because signs display chars and not ints
#define CHAR_TO_INT(c) (c - '0')
#define INT_TO_CHAR(i) (i + '0')

// number of plates to initialise hashtable, can be lower than the actual number
#define EXPECTED_NUM_PLATES 10

pthread_mutex_t rand_mutex;
// mutex for accessing the hashtable of vehicles, hashtable should be fast
// enough to not be a performance problem
pthread_mutex_t cars_mutex;
// Hashtable of number plates mapped to their assigned level and current level
// - values indexed starting at zero with 0xF meaing unassigned
ht_t *cars_ht;

pthread_mutex_t capacity_mutex;
// hashtable for storing capacity of each level
//
// could try to reduce the cars hashtable but this seems easier
// - indexed starting at zero
ht_t *capacity_ht;

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
  char level[2];
  level[0] = INT_TO_CHAR(l);
  level[1] = '\0';
  int cars;
  pthread_mutex_lock(&capacity_mutex);
  cars = htab_get(capacity_ht, level);
  pthread_mutex_unlock(&capacity_mutex);
  return cars;
}

// Get Levels that have not reached capacity
// Returns int array with first element being the number of available levels
// and the rest being the actual levels
int *get_available_levels(int *levels) {
  if (levels == NULL) {
    perror("Level Malloc");
    exit(EXIT_FAILURE);
  }
  int i = 0;
  for (int l = 0; l < NUM_LEVELS; l++) {
    if (ts_cars_on_level(l) < LEVEL_CAPACITY) {
      i++;
      levels[i] = l;
    }
  }
  levels[0] = i;
  return levels;
}

// thread-safe setting of level capacity
int ts_add_cars_to_level(int l, int num_cars) {
  // turn level into null-terminated string
  char level[2];
  level[0] = INT_TO_CHAR(l);
  level[1] = '\0';
  int cars;
  pthread_mutex_lock(&capacity_mutex);
  cars = htab_get(capacity_ht, level);
  cars += num_cars;
  htab_set(capacity_ht, level, cars);
  pthread_mutex_unlock(&capacity_mutex);
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
  pthread_mutex_lock(&cars_mutex);
  value = htab_get(cars_ht, null_terminated_plate);
  pthread_mutex_unlock(&cars_mutex);
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
  pthread_mutex_lock(&cars_mutex);
  int current_value = htab_get(cars_ht, null_terminated_plate);
  int new_value = SET_ASSIGNED_LEVEL(current_value, level);
  success = htab_set(cars_ht, null_terminated_plate, new_value);
  pthread_mutex_unlock(&cars_mutex);
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
  pthread_mutex_lock(&cars_mutex);
  int current_value = htab_get(cars_ht, null_terminated_plate);
  int new_value = SET_CURRENT_LEVEL(current_value, level);
  success = htab_set(cars_ht, null_terminated_plate, new_value);
  pthread_mutex_unlock(&cars_mutex);
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
    line[6] = '\0';           // null-terminate the plate if not already
    htab_set(ht, line, 0xFF); // set the value to 0xFF (unassigned)
  }
  free(line);
  return ht;
}

// Wait at the LPR for a licence plate to be written
void wait_for_lpr(struct LPR *lpr) {
  // wait at the given LPR for anything other than NULL to be written
  pthread_mutex_lock(&lpr->mutex);
  while (lpr->plate[0] == '\0') { // while the lpr is empty
    pthread_cond_wait(&lpr->condition, &lpr->mutex);
  }
  pthread_mutex_unlock(&lpr->mutex);
}

void *entry_handler(void *arg) {
  struct EntryArgs *args = (struct EntryArgs *)arg;
  int id = args->id;
  struct Entrance *entrance = &shm->entrances[id]; // The corresponding entrance
  while (true) {
    // wait for a car to arrive at the LPR
    wait_for_lpr(&entrance->lpr);
    // should be a licence plate there now, so read it
    char *plate = entrance->lpr.plate;
    // check if the car is in the hashtable (and not already in the car park)
    int value = ts_get_number_plate(plate);
    char level;
    // TODO: fire alarm stuff
    if (GET_ASSIGNED_LEVEL(value) == NO_LEVEL &&
        GET_CURRENT_LEVEL(value) == NO_LEVEL) { // not already in but allowed
      // set info sign to a random available level
      int *levels = calloc(NUM_LEVELS + 1, sizeof(int));
      if (levels == NULL) {
        perror("Levels Calloc");
        exit(EXIT_FAILURE);
      }
      levels = get_available_levels(levels);
      if (levels[0] == 0) {
        level = 'F'; // Carpark Full
      } else {
        pthread_mutex_lock(&rand_mutex);
        int levelID = rand() % levels[0] + 1; // random available level index
        pthread_mutex_unlock(&rand_mutex);
        level = levels[levelID]; // random available level (integer)
        // increment the level capacity
        ts_add_cars_to_level(level, 1);
        // assign the car to the level
        ts_set_assigned_level(plate, level);
        // set the level to the character representation
        level = INT_TO_CHAR(level + 1); // level offset by 1 for display
      }
      free(levels); // get rid of the levels array
    } else { // not allowed in the car park (already in or not in the hashtable)
      level = 'X';
    }

    // set the sign
    pthread_mutex_lock(&entrance->sign.mutex);
    entrance->sign.display = level;
    pthread_cond_broadcast(&entrance->sign.condition);
    pthread_mutex_unlock(&entrance->sign.mutex);

    // Tell the simulator to open the gate
    // TODO: other cases
    if (level != 'X') {
      pthread_mutex_lock(&entrance->gate.mutex);
      // raise the gate if it is not already open
      if (entrance->gate.status != 'O') {
        entrance->gate.status = 'R';
      }
      pthread_cond_broadcast(&entrance->gate.condition);
      pthread_mutex_unlock(&entrance->gate.mutex);

      // wait 20ms and then tell sim to close the gate
      rand_delay_ms(20, 20, &rand_mutex);
      pthread_mutex_lock(&entrance->gate.mutex);
      entrance->gate.status = 'L';
      pthread_cond_broadcast(&entrance->gate.condition);
      pthread_mutex_unlock(&entrance->gate.mutex);
    }

    // clear the LPR
    pthread_mutex_lock(&entrance->lpr.mutex);
    memset(entrance->lpr.plate, '\0', 6);
    pthread_cond_broadcast(&entrance->lpr.condition);
    pthread_mutex_unlock(&entrance->lpr.mutex);
  }
  printf("Entry Stop %d\n", id);
  return NULL;
}

void *level_handler(void *arg) {
  int *lid = (int *)arg;
  int level_id = *lid;
  struct Level *level = &shm->levels[level_id];
  // forever stuck checking for cars
  while (true) {
    // wait at the lpr for a car to arrive
    wait_for_lpr(&level->lpr);
    // read the plate
    char *plate = level->lpr.plate;
    // check if they are entering or exiting
    int value = ts_get_number_plate(plate);
    int assigned = GET_ASSIGNED_LEVEL(value);
    int current = GET_CURRENT_LEVEL(value);
    if (current != NO_LEVEL) {
      // they are already on a level
      if (current == level_id) {
        // they must be on this level and leaving
        // decrement the level capacity
        ts_add_cars_to_level(level_id, -1);
        // unassign car from the level
        ts_set_current_level(plate, NO_LEVEL);
      } else {
        // something went real wrong, they haven't left the level they were on
        printf("Car %.6s teleported to different level, current: %d, "
               "thislevel: %d, value: %d\n",
               plate, current, level_id, value);
        exit(EXIT_FAILURE);
      }
    } else if (assigned != level_id) {
      // they are on the wrong level (or not assigned at all), re-assign them
      // and let them in
      if (ts_cars_on_level(level_id) < LEVEL_CAPACITY) {
        ts_add_cars_to_level(assigned, -1);
        ts_add_cars_to_level(level_id, 1);
      } else {
        // Can't really communicate with the cars as there is no sign
        printf("Car trying to enter full level\n");
      }
      // set the current level for the car
      ts_set_current_level(plate, level_id);
    }

    // clear the lpr
    pthread_mutex_lock(&level->lpr.mutex);
    memset(level->lpr.plate, '\0', 6);
    pthread_cond_broadcast(&level->lpr.condition);
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
    // car left, unassign them from the carpark. Has to be in critical section
    // to prevent sim reusing plate instantly and then manager thinking they are
    // still in the carpark
    ts_set_assigned_level(plate, NO_LEVEL);
    ts_set_current_level(plate, NO_LEVEL);
    pthread_cond_broadcast(&exit->gate.condition);
    pthread_mutex_unlock(&exit->gate.mutex);
    // clear the LPR, ready for another car
    pthread_mutex_lock(&exit->lpr.mutex);
    memset(exit->lpr.plate, '\0', 6);
    pthread_cond_broadcast(&exit->lpr.condition);
    pthread_mutex_unlock(&exit->lpr.mutex);
  }
}

int main(int argc, char *argv[]) {
  // initialise local mutexes
  srand(time(NULL));
  pthread_mutex_init(&rand_mutex, NULL);
  pthread_mutex_init(&cars_mutex, NULL);
  pthread_mutex_init(&capacity_mutex, NULL);

  // get the shared memory object
  shm = get_shm(SHM_NAME);

  // read the allowed number plates from a file into hashtable
  cars_ht = ht_from_file("plates.txt");

  // initialise level capacity hashtable
  capacity_ht = htab_create(capacity_ht, NUM_LEVELS);
  for (int i = 0; i < NUM_LEVELS; i++) {
    char level[2] = {INT_TO_CHAR(i), '\0'};
    htab_set(capacity_ht, level, 0);
  }

  // create entrance threads
  // -------------------------------
  pthread_t entry_threads[NUM_ENTRANCES];
  struct EntryArgs *entry_args[NUM_ENTRANCES];
  for (int i = 0; i < NUM_ENTRANCES; i++) {
    struct EntryArgs *args = calloc(1, sizeof(struct EntryArgs));
    if (args == NULL) {
      perror("calloc entry");
      exit(EXIT_FAILURE);
    }
    entry_args[i] = args;
    args->id = i;
    pthread_t thread;
    // pass in i
    pthread_create(&thread, NULL, entry_handler, args);
    entry_threads[i] = thread;
  }
  // create level threads
  // -------------------------------
  pthread_t level_threads[NUM_LEVELS];
  struct LevelArgs *level_args[NUM_LEVELS];
  for (int i = 0; i < NUM_ENTRANCES; i++) {
    struct LevelArgs *args = calloc(1, sizeof(struct LevelArgs));
    if (args == NULL) {
      perror("calloc level");
      exit(EXIT_FAILURE);
    }
    level_args[i] = args;
    args->id = i;
    pthread_t thread;
    // pass in i
    pthread_create(&thread, NULL, level_handler, args);
    level_threads[i] = thread;
  }
  // create exit threads
  // -------------------------------
  pthread_t exit_threads[NUM_EXITS];
  struct ExitArgs *exit_args[NUM_EXITS];
  for (int i = 0; i < NUM_EXITS; i++) {
    struct ExitArgs *args = calloc(1, sizeof(struct ExitArgs));
    if (args == NULL) {
      perror("calloc exit");
      exit(EXIT_FAILURE);
    }
    exit_args[i] = args;
    args->id = i;
    pthread_t thread;
    // pass in i
    pthread_create(&thread, NULL, exit_handler, args);
    exit_threads[i] = thread;
  }

  // don't run the display if we don't want it
  if (argc < 2 || strcmp(argv[1], "nodisp") != 0) {
    ManDisplayData display_data;
    display_data.ht = capacity_ht;
    display_data.ht_mutex = &capacity_mutex;
    display_data.shm = shm;
    pthread_t display_thread;
    pthread_create(&display_thread, NULL, man_display_handler, &display_data);
  }

  // wait for threads to finish and clean up their resources
  for (int i = 0; i < NUM_ENTRANCES; i++) {
    pthread_join(entry_threads[i], NULL);
    free(entry_args[i]);
  }

  for (int i = 0; i < NUM_LEVELS; i++) {
    pthread_join(level_threads[i], NULL);
    free(level_args[i]);
  }
  for (int i = 0; i < NUM_EXITS; i++) {
    pthread_join(exit_threads[i], NULL);
    free(exit_args[i]);
  }
}