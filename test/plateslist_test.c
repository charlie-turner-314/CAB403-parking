#include "sim_plates.h"
#include "testing.h"
#include <stdbool.h>

bool add_one_plate(NumberPlates *p) {
  // add one plate
  if (!add_plate(p, "ABC123"))
    return false;
  return true;
}

bool add_some_plates(NumberPlates *p) {
  // add heaps of plates
  size_t num_plates = 100;
  char plate[7];
  plate[6] = '\0';
  for (size_t i = 0; i < num_plates; i++) {
    // generate a random licence plate as key -> ABC123:
    for (int j = 0; j < 6; j++) {
      if (j < 3)
        plate[j] = ("ABCDEFGHIJKLMNOPQRSTUVWXYZ"[rand() % 26]);
      else
        plate[j] = "0123456789"[(rand() % 10)];
    }
    if (!add_plate(p, plate))
      return false;
  }
  return true;
}

bool get_random_plate(NumberPlates *p) {
  // get a random plate
  char *plate = random_available_plate(p);
  if (plate == NULL)
    return false;
  if (plate[6] != '\0')
    return false;
  return true;
}

bool remove_all_plates(NumberPlates *p) {
  // remove all plates
  if (!clear_plates(p))
    return false;
  return true;
}

bool get_random_plate_none_available(NumberPlates *p) {
  // get a random plate when none are in the list, should still return
  // a plate because it will generate random
  char *plate = random_available_plate(p);
  if (plate == NULL)
    return false;
  return true;
}

bool destroy(NumberPlates *p) {
  // destroy the plates
  if (!destroy_plates(p))
    return false;
  return true;
}

bool is_destroyed(NumberPlates *p) {
  // check if the plates are destroyed
  if (!(p == NULL))
    return false;
  return true;
}

int main(void) {
  // Initialise
  // set color to yellow
  printf("\033[0;33m");
  printf("Testing Plates List\n");
  // reset color
  printf("\033[0m");
  pthread_mutex_t rand_mutex = PTHREAD_MUTEX_INITIALIZER;
  NumberPlates *plates = list_from_file("plates.txt", &rand_mutex);

  // Run tests
  setlocale(LC_CTYPE, "");
  wchar_t cross = 0x00D7;
  wchar_t check = 0x2713;

  int num_tests = 6;
  bool (*funcs[7])(NumberPlates * plates) = {
      add_one_plate,                   /*0*/
      add_some_plates,                 /*1*/
      get_random_plate,                /*2*/
      remove_all_plates,               /*3*/
      get_random_plate_none_available, /*4*/
      destroy,                         /*5*/
      is_destroyed                     /*6*/
  };
  int num_passed = 0;
  for (int i = 0; i < num_tests; i++) {
    if ((*funcs[i])(plates)) {
      // set color to green
      printf("\033[0;32m");
      wprintf(L"%lc Test %d passed\n", check, i);
      num_passed++;
    } else {
      // set color to red
      printf("\033[0;31m");
      wprintf(L"%lc Test %d failed\n", cross, i);
    }
  }

  if (num_passed == num_tests) {
    // set color to green
    printf("\033[0;32m");
    printf("---------------------\n");
    printf("All Plates List Tests passed\n");
    // reset color
    printf("\033[0m");
  } else {
    // set color to red
    printf("\033[0;31m");
    printf("Passed %d/%d tests\n", num_passed, num_tests);
    // reset color
    printf("\033[0m");
  }

  return 0;
}
