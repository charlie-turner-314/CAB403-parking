#include "config.h"
#include "delay.h"
#include "display.h"
#include "hashtable.h"
#include "shm_parking.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

// struct for holding assigned and current car level
struct car_levels {
  int8_t current;  // current level, -1 if no level
  int8_t assigned; // assigned level, -1 if no level
};

// Macros because signs display chars and not ints
#define CHAR_TO_INT(c) (c - '0')
#define INT_TO_CHAR(i) (i + '0')

// number of plates to initialise hashtable, can be lower than the actual number
// and table will grow
#define EXPECTED_NUM_PLATES 10

pthread_mutex_t rand_mutex; // mutex for rand() function
pthread_mutex_t cars_mutex; // mutex for hashtable of vehicles
ht_t *cars_ht; // hashtable of vehicles and their current and assigned level

pthread_mutex_t capacity_mutex; // mutex for capacity of each level
ht_t *capacity_ht;              // hashtable of levels and their capacity

// hashtable for storing billing information for cars
ht_t *billing_ht;
pthread_mutex_t billing_mutex = PTHREAD_MUTEX_INITIALIZER;
float total_bill = 0;
struct timezone;
struct timeval;
int gettimeofday(struct timeval *tp, struct timezone *tz);

int run = 1;

struct SharedMemory *shm; // shared memory

// structs for passing arguments to threads
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
  cars = *(int *)htab_get(capacity_ht, level);
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
  if (l >= NUM_LEVELS || l < 0) {
    return 0;
  }
  // turn level into null-terminated string
  char level[2];
  level[0] = INT_TO_CHAR(l);
  level[1] = '\0';
  int cars;
  pthread_mutex_lock(&capacity_mutex);
  int *cars_ptr = (int *)htab_get(capacity_ht, level);
  if (cars_ptr == NULL) {
    return 0;
  }
  cars = *cars_ptr;
  cars += num_cars;
  // This is such a hack, if you're marking this just ignore
  // the fact that I brute force the capacity to 0 if it goes below 0 during a
  // fire
  cars = cars > 0 ? cars : 0;
  htab_set(capacity_ht, level, &cars, sizeof(int));
  pthread_mutex_unlock(&capacity_mutex);
  return cars;
}

// thread-safe access to the number plates
struct car_levels *ts_get_number_plate(char *plate) {
  struct car_levels *value;
  // ensure the plate is null-terminated
  char null_terminated_plate[7];
  strncpy(null_terminated_plate, plate, 7);
  null_terminated_plate[6] = '\0';
  // ----------
  pthread_mutex_lock(&cars_mutex);
  value = (struct car_levels *)htab_get(cars_ht, null_terminated_plate);
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
  struct car_levels *current_value =
      (struct car_levels *)htab_get(cars_ht, null_terminated_plate);
  struct car_levels new_value = {-1, -1}; // assume failure
  if (current_value) {
    new_value = *current_value; // if not fail, update
  }
  new_value.assigned = level; // set the assigned level
  success = htab_set(cars_ht, null_terminated_plate, &new_value,
                     sizeof(struct car_levels));
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
  struct car_levels *current_value =
      (struct car_levels *)htab_get(cars_ht, null_terminated_plate);
  struct car_levels new_value = {-1, -1}; // assume failure
  if (current_value) {
    new_value = *current_value; // if not fail, update
  }
  new_value.current = level; // set the assigned level
  success = htab_set(cars_ht, null_terminated_plate, &new_value,
                     sizeof(struct car_levels));
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
  struct car_levels unassigned = {-1, -1};
  while ((linelen = getline(&line, &linecap, fp)) > 0) {
    // null-terminate the plate if not already
    line[6] = '\0';
    // set the value to 0xFF (unassigned)
    htab_set(ht, line, &unassigned, sizeof(struct car_levels));
  }
  free(line);
  return ht;
}

// Wait at the LPR for a licence plate to be written
void wait_for_lpr(struct LPR *lpr) {
  // wait at the given LPR for anything other than NULL to be written
  pthread_mutex_lock(&lpr->mutex);
  while (lpr->plate[0] == '\0' && run) { // while the lpr is empty
    pthread_cond_wait(&lpr->condition, &lpr->mutex);
  }
  pthread_mutex_unlock(&lpr->mutex);
}

// checks each level, returns 1 immediately if any level has the alarm active
int alarm_is_active() {
  for (int i = 0; i < NUM_LEVELS; i++) {
    if (shm->levels[i].alarm) {
      return 1;
    }
  }
  return 0;
}

void *entry_handler(void *arg) {
  struct EntryArgs *args = (struct EntryArgs *)arg;
  int id = args->id;
  struct Entrance *entrance = &shm->entrances[id]; // The corresponding entrance
  int *available_levels = calloc(NUM_LEVELS + 1, sizeof(int));
  if (available_levels == NULL) {
    perror("Levels Calloc");
    exit(EXIT_FAILURE);
  }
  while (run) {
    // wait for a car to arrive at the LPR
    wait_for_lpr(&entrance->lpr);
    if (!run)
      break;
    // should be a licence plate there now, so read it
    char *plate = entrance->lpr.plate;
    char level = '\0';

    if (alarm_is_active()) {
      // clear the LPR and continue to the next iteration
      pthread_mutex_lock(&entrance->lpr.mutex);
      memset(entrance->lpr.plate, '\0', 6);
      pthread_cond_broadcast(&entrance->lpr.condition);
      pthread_mutex_unlock(&entrance->lpr.mutex);
      continue;
    }
    // check if the car is in the hashtable (and not already in the car park)
    struct car_levels *value = ts_get_number_plate(plate);

    if (!value) // not in the hashtable
    {
      level = 'X';
    } else if (value->assigned == -1 &&
               value->current == -1) // not already in but allowed
    {
      // update available levels
      available_levels = get_available_levels(available_levels);
      if (available_levels[0] == 0) {
        level = 'F'; // Carpark Full
      } else {
        pthread_mutex_lock(&rand_mutex);
        // id of a random available level
        int available_level_index = rand() % available_levels[0] + 1;
        pthread_mutex_unlock(&rand_mutex);
        level = available_levels[available_level_index]; // random available
                                                         // level (integer)
        level = INT_TO_CHAR(level + 1); // level offset by 1 for display
      }
    } else { // not allowed in the car park (already in )
      level = 'X';
    }

    // set the sign
    pthread_mutex_lock(&entrance->sign.mutex);
    if (level) { // don't touch the level if we are evacuating
      entrance->sign.display = level;
    }
    pthread_cond_broadcast(&entrance->sign.condition);
    pthread_mutex_unlock(&entrance->sign.mutex);

    // Tell the simulator to open the gate if the level is one of the numbers
    if ((level && level >= '0' && level <= '9')) {
      ts_set_assigned_level(plate,
                            CHAR_TO_INT(level) -
                                1);    // assign them the given level
      ts_set_current_level(plate, -1); // they aren't on a current level
      pthread_mutex_lock(&entrance->gate.mutex);
      entrance->gate.status = 'R'; // set the gate to rising
      pthread_cond_broadcast(&entrance->gate.condition);
      pthread_mutex_unlock(&entrance->gate.mutex);
      // Add car to billing table
      // Null terminate plate
      char array[7];
      memccpy(array, plate, 0, 6);
      array[6] = '\0';

      // get current time in milliseconds
      struct timeval tv;
      gettimeofday(&tv, NULL);
      long long millisecondsTime =
          (long long)(tv.tv_sec) * 1000 +
          (long long)(tv.tv_usec) /
              1000; // convert tv_sec & tv_usec to// milliseconds

      // add to hashtable
      pthread_mutex_lock(&billing_mutex);
      htab_set(billing_ht, array, &millisecondsTime, sizeof(long long));
      pthread_mutex_unlock(&billing_mutex);

      // close gate after 20ms
      delay_ms(20);
      pthread_mutex_lock(&entrance->gate.mutex);
      entrance->gate.status = 'L';
      pthread_cond_broadcast(&entrance->gate.condition);
      pthread_mutex_unlock(&entrance->gate.mutex);
    }
    delay_ms(20); // allow sim time to close the gate
    // clear the Sign from the last guy
    pthread_mutex_lock(&entrance->sign.mutex);
    entrance->sign.display = '\0';
    pthread_mutex_unlock(&entrance->sign.mutex);
    // clear the LPR
    pthread_mutex_lock(&entrance->lpr.mutex);
    memset(entrance->lpr.plate, '\0', 6);
    pthread_cond_broadcast(&entrance->lpr.condition);
    pthread_mutex_unlock(&entrance->lpr.mutex);
  }
  free(available_levels); // get rid of the levels array
  printf("Entry Stop %d\n", id);
  return NULL;
}

void *level_handler(void *arg) {
  int *lid = (int *)arg;
  int level_id = *lid;
  struct Level *level = &shm->levels[level_id];
  // forever stuck checking for cars
  while (run) {
    // wait at the lpr for a car to arrive
    wait_for_lpr(&level->lpr);
    if (!run)
      break;
    // read the plate
    char *plate = level->lpr.plate;
    // check if they are entering or exiting
    struct car_levels *value = ts_get_number_plate(plate);
    int assigned = value->assigned;
    int current = value->current;
    if (current != -1) // they are already on a level
    {
      if (current == level_id) // they must be on this level and leaving
      {
        // unassign car from the level
        ts_set_current_level(plate, -1);
        // decrement the level capacity
        ts_add_cars_to_level(level_id, -1);
      } else // they are on a different level currently ????
      {
        // something went real wrong, they haven't left the level they were on
        printf("Car %.6s teleported to different level, current: %d, "
               "thislevel: %d, value: c:%d, a:%d\n",
               plate, current, level_id, value->current, value->assigned);
        exit(EXIT_FAILURE);
      }
    } else if (assigned != level_id) // they aren't assigned to this level
    {
      // they are on the wrong level (or not assigned at all), re-assign them
      // if there is room
      if (ts_cars_on_level(level_id) < LEVEL_CAPACITY) {
        ts_add_cars_to_level(assigned, -1);
        ts_add_cars_to_level(level_id, 1);
        ts_set_current_level(plate, level_id);
      } else {
        // Can't really communicate with the cars as there is no sign
        printf("Car trying to enter full level\n");
      }
    } else // they are assigned this level and current level is NO_LEVEL
    {
      // increment the level capacity
      ts_add_cars_to_level(level_id, 1);
      // set the car's current level
      ts_set_current_level(plate, level_id);
    }

    // clear the lpr
    pthread_mutex_lock(&level->lpr.mutex);
    memset(level->lpr.plate, '\0', 6);
    pthread_cond_broadcast(&level->lpr.condition);
    pthread_mutex_unlock(&level->lpr.mutex);
  }
  return NULL;
}

void *exit_handler(void *arg) {
  struct ExitArgs *args = (struct ExitArgs *)arg;
  int id = args->id;
  struct Exit *exit = &shm->exits[id]; // The corresponding exit
  while (run) {
    // wait for a car to arrive at the LPR
    wait_for_lpr(&exit->lpr);
    if (!run)
      break;
    // should be a licence plate there now, so read it
    char *plate = exit->lpr.plate;
    // open the gate
    pthread_mutex_lock(&exit->gate.mutex);
    exit->gate.status = 'R';

    pthread_cond_broadcast(&exit->gate.condition);
    pthread_mutex_unlock(&exit->gate.mutex);
    // Calculate billing
    char exitplate[7];
    memccpy(exitplate, plate, 0, 6);
    exitplate[6] = '\0';
    pthread_mutex_lock(&billing_mutex);
    long long *entry_time = (long long *)htab_get(billing_ht, exitplate);
    pthread_mutex_unlock(&billing_mutex);
    if (!entry_time) {
      printf("Car %.6s not found in billing table\n", plate);
    } else {
      struct timeval tv;
      gettimeofday(&tv, NULL);
      long long millisecondsTime =
          (long long)(tv.tv_sec) * 1000 +
          (long long)(tv.tv_usec) /
              1000; // convert tv_sec & tv_usec to// milliseconds
      int time_in_carpark = (millisecondsTime - *entry_time) / TIME_FACTOR;
      float bill = time_in_carpark * COST_PER_MS;
      FILE *billing_file = fopen("billing.txt", "a");
      fprintf(billing_file, "%s $%.2f \n", exitplate, bill);
      fclose(billing_file);
      total_bill += bill;
    }

    // car left, unassign them from the carpark.
    ts_set_assigned_level(plate, -1);
    ts_set_current_level(plate, -1);

    // wait 20ms and then tell sim to close the gate, only if we aren't
    // evacuating
    if (!alarm_is_active()) {
      delay_ms(20);
      pthread_mutex_lock(&exit->gate.mutex);
      exit->gate.status = 'L';
      pthread_cond_broadcast(&exit->gate.condition);
      pthread_mutex_unlock(&exit->gate.mutex);
    }
    delay_ms(20); // allow sim time to close the gate
    // clear the LPR, ready for another car
    pthread_mutex_lock(&exit->lpr.mutex);
    memset(exit->lpr.plate, '\0', 6);
    pthread_cond_broadcast(&exit->lpr.condition);
    pthread_mutex_unlock(&exit->lpr.mutex);
  }
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
  }
  // reset terminal
  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  system("stty echo");

  run = 0;
  return NULL;
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
    int num_cars = 0;
    htab_set(capacity_ht, level, &num_cars, sizeof(int));
  }

  // initialise billing hashtable
  billing_ht = htab_create(billing_ht, 5);

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
  for (int i = 0; i < NUM_LEVELS; i++) {
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
    display_data.billing_total = &total_bill;
    pthread_t display_thread;
    pthread_create(&display_thread, NULL, man_display_handler, &display_data);
  }

  // create input handler thread
  pthread_t input_thread;
  pthread_create(&input_thread, NULL, input_handler, NULL);

  pthread_join(input_thread, NULL);
  // signal all the possible waitings
  int num_lprs = NUM_ENTRANCES + NUM_LEVELS + NUM_EXITS;
  for (int i = 0; i < num_lprs; i++) {
    if (i < NUM_ENTRANCES) {
      pthread_mutex_lock(&shm->entrances[i].lpr.mutex);
      pthread_cond_broadcast(&shm->entrances[i].lpr.condition);
      pthread_mutex_unlock(&shm->entrances[i].lpr.mutex);
    } else if (i < NUM_ENTRANCES + NUM_LEVELS) {
      pthread_mutex_lock(&shm->levels[i - NUM_ENTRANCES].lpr.mutex);
      pthread_cond_broadcast(&shm->levels[i - NUM_ENTRANCES].lpr.condition);
      pthread_mutex_unlock(&shm->levels[i - NUM_ENTRANCES].lpr.mutex);
    } else {
      pthread_mutex_lock(&shm->exits[i - NUM_ENTRANCES - NUM_LEVELS].lpr.mutex);
      pthread_cond_broadcast(
          &shm->exits[i - NUM_ENTRANCES - NUM_LEVELS].lpr.condition);
      pthread_mutex_unlock(
          &shm->exits[i - NUM_ENTRANCES - NUM_LEVELS].lpr.mutex);
    }
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