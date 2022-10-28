#include "display.h"
#include "config.h"
#include "queue.h"
#include "shm_parking.h"
#include "simulator.h"
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ANSI_CTRL_CLEAR "\x1B[2J"
#define ANSI_CTRL_HOME "\x1B[;H"
// macros for moving around the terminal
// ANSI_CTRL_UP moves the cursor up by n lines
#define ANSI_CTRL_UP(n) printf("\x1B[%dA", n)
// ANSI_CTRL_DOWN moves the cursor down by n lines
#define ANSI_CTRL_DOWN(n) "\x1B[" #n "B"
// ANSI_CTRL_RIGHT moves the cursor right by n columns
#define ANSI_CTRL_RIGHT(n) printf("\x1B[%dC", n)
// ANSI_CTRL_LEFT moves the cursor left by n columns
#define ANSI_CTRL_LEFT(n) "\x1B[" #n "D"
// ANSI_CTRL_POS moves the cursor to the specified position
#define ANSI_CTRL_POS(row, col) printf("\x1B[%d;%dH", row, col)

// function prototypes
void car_item_print(ct_data *car_data);
void entry_queue_print(Queue *q);

// used by manager
void *man_display_handler(void *arg) {
  puts(ANSI_CTRL_CLEAR);
  puts(ANSI_CTRL_HOME);
  ManDisplayData *data = (ManDisplayData *)arg;
  struct SharedMemory *shm = data->shm;
  while (*data->run) {
    // clear the screen
    printf(ANSI_CTRL_CLEAR);
    printf(ANSI_CTRL_HOME);
    printf(
        "Parking Simulator - Manager | Press 'q' to exit after exiting sim\n");
    printf("Total bill: $%.2f \n", *data->billing_total);
    // row to display header of each table
    int hrow = 4;

    // print each entrance LPR for each level
    printf("================= Entrances ======================\n\n");
    printf("  LPR|\n");
    printf(" BOOM|\n");
    printf(" SIGN|\n");
    for (int i = 0; i < NUM_ENTRANCES; i++) {
      int col = i * 6 + i + 7;
      ANSI_CTRL_POS(hrow, col + 2);
      printf("\x1B[34m");
      printf("%d\n", i + 1);
      printf("\x1B[0m");
      ANSI_CTRL_POS(hrow + 1, col);

      pthread_mutex_lock(&shm->entrances[i].lpr.mutex);
      char *plate = shm->entrances[i].lpr.plate;
      printf("%.6s|\n", plate[0] ? plate : "      ");
      pthread_mutex_unlock(&shm->entrances[i].lpr.mutex);

      ANSI_CTRL_POS(hrow + 2, col);
      pthread_mutex_lock(&shm->entrances[i].gate.mutex);
      char status = shm->entrances[i].gate.status;
      pthread_mutex_unlock(&shm->entrances[i].gate.mutex);
      printf("  %c   |\n", status ? status : ' ');

      ANSI_CTRL_POS(hrow + 3, col);
      pthread_mutex_lock(&shm->entrances[i].sign.mutex);
      char display = shm->entrances[i].sign.display;
      pthread_mutex_unlock(&shm->entrances[i].sign.mutex);
      printf("  %c   |\n", display ? display : ' ');
    }
    ANSI_CTRL_POS(hrow + 4, 0);
    printf("=================== levels =======================\n\n");
    printf("  LPR|\n");
    printf(" TEMP|\n");
    printf("ALARM|\n");
    printf("  CAP|\n");
    printf(" CURR|\n");
    hrow = 9;
    for (int i = 0; i < NUM_LEVELS; i++) {
      int col = i * 6 + i + 7;
      ANSI_CTRL_POS(hrow, col + 2);
      printf("\x1B[34m");
      printf("%d\n", i + 1);
      printf("\x1B[0m");

      ANSI_CTRL_POS(hrow + 1, col);
      pthread_mutex_lock(&shm->levels[i].lpr.mutex);
      char *plate = shm->levels[i].lpr.plate;
      printf("%.6s|\n", plate[0] ? plate : "      ");
      pthread_mutex_unlock(&shm->levels[i].lpr.mutex);

      ANSI_CTRL_POS(hrow + 2, col);
      int16_t temp = shm->levels[i].temp;
      printf("  %02d  |\n", temp);
      ANSI_CTRL_POS(hrow + 3, col);
      int8_t alarm = shm->levels[i].alarm;
      printf(" %s  |\n", alarm ? " ON" : "OFF");
      ANSI_CTRL_POS(hrow + 4, col);
      printf("  %02d  |\n", LEVEL_CAPACITY);
      ANSI_CTRL_POS(hrow + 5, col);
      char levelstr[2] = {'0' + i, '\0'};
      pthread_mutex_lock(data->ht_mutex);
      printf("  %02d  |\n", *(int *)htab_get(data->ht, levelstr));
      pthread_mutex_unlock(data->ht_mutex);
    }

    ANSI_CTRL_POS(hrow + 6, 0);
    printf("=================== Exits =======================\n\n");
    printf("  LPR|\n");
    printf(" GATE|\n");
    hrow = 16;
    for (int i = 0; i < NUM_EXITS; i++) {
      int col = i * 6 + i + 7;
      ANSI_CTRL_POS(hrow, col + 2);
      printf("\x1B[34m");
      printf("%d\n", i + 1);
      printf("\x1B[0m");

      ANSI_CTRL_POS(hrow + 1, col);
      pthread_mutex_lock(&shm->exits[i].lpr.mutex);
      char *plate = shm->exits[i].lpr.plate;
      printf("%.6s|\n", plate[0] ? plate : "      ");
      pthread_mutex_unlock(&shm->exits[i].lpr.mutex);

      ANSI_CTRL_POS(hrow + 2, col);
      pthread_mutex_lock(&shm->exits[i].gate.mutex);
      char status = shm->exits[i].gate.status;
      pthread_mutex_unlock(&shm->exits[i].gate.mutex);
      printf("  %c   |\n", status ? status : ' ');
    }
    usleep(50000);
  }
  printf("Display Ended\n");
  return NULL;
}

void *sim_display_handler(void *arg) {
  printf("Display handler starting in 1 second\n");
  // sleep for one second to allow user to read any previous output
  sleep(1);
  // clear the terminal
  printf(ANSI_CTRL_CLEAR);
  // Queue **entry_queues = (Queue **)arg;
  SimDisplayData *data = (SimDisplayData *)arg;

  while (*data->running || *data->num_cars) {
    // // clear the screen
    printf("\033[2J\033[1;1H");

    // print the number of used threads
    printf("Number of Car threads in use: %d\n", *data->num_cars);
    // print the number of available number plates
    printf("Number of unused allowed plates: %zu\n", *data->available_plates);
    // print each entry queue
    printf("\033[5;1H");
    for (int i = 0; i < NUM_ENTRANCES; i++) {
      printf("EntryQ %d : ", i);
      entry_queue_print(data->entry_queues[i]);
      printf("\n");
    }
    // print in the top right corner but leave space for 16 characters
    if (*data->running) {
      printf("\033[1;40H");
      printf("| Press 'f' to start a fixed-temp fire\n");
      printf("\033[2;40H");
      printf("| Press 'r' to start a rate-of-rise fire\n");
      printf("\033[3;40H");
      printf("| Press 's' to stop the fire\n");
      printf("\033[4;40H");
      printf("| Press 'q' to quit gracefully\n");
    } else {
      printf("\033[1;40H");
      printf("| Exiting, waiting for manager to release cars...\n");
      printf("\033[2;40H");
      printf("| Press CTRL+C to force quit\n");
    }
    // sleep for 50ms
    usleep(50000);
  }
  return NULL;
}

// print entrance item
static void entrance_item_print(char *plate) { printf("'%6s' ", plate); }

// print entrance queue
void entry_queue_print(Queue *q) {

  pthread_mutex_lock(&q->mutex);
  QItem *node = q->head;

  if (!node)
    printf("empty");

  while (node) {
    entrance_item_print(node->value);
    node = node->next;
  }
  pthread_mutex_unlock(&q->mutex);
}

// print car item
void car_item_print(ct_data *car_data) { printf("'%6s' ", car_data->plate); }

// print car object queue
void car_queue_print(Queue *q) {

  pthread_mutex_lock(&q->mutex);
  QItem *node = q->head;

  if (!node)
    printf("empty");

  while (node) {
    car_item_print(node->value);
    node = node->next;
  }
  pthread_mutex_unlock(&q->mutex);
}
