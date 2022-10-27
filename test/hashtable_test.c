#include "hashtable.h"
#include "testing.h"

#define INITIAL_CAPACITY 20
#define TEST_KEY "test"
#define TEST_VALUE 15

bool add_item(ht_t *h) {
  // add one item (don't grow)
  int value = TEST_VALUE;
  if (!htab_set(h, TEST_KEY, &value, sizeof(int)))
    return false;
  return true;
}

bool add_items(ht_t *h) {
  // add heaps of items (grow the table a couple of times)
  size_t num_items = INITIAL_CAPACITY * 2;
  char plate[7];
  plate[6] = '\0';
  for (size_t i = 0; i < num_items; i++) {
    // generate a random licence plate as key -> ABC123:
    for (int j = 0; j < 6; j++) {
      if (j < 3)
        plate[j] = ("ABCDEFGHIJKLMNOPQRSTUVWXYZ"[rand() % 26]);
      else
        plate[j] = "0123456789"[(rand() % 10)];
    }
    int value = (rand() % 100);
    if (!htab_set(h, plate, &value, sizeof(int)))
      return false;
  }
  return true;
}

bool find(ht_t *h) {
  // find the item we added
  item_t *item = htab_find(h, TEST_KEY);
  if (item == NULL)
    return false;
  if (*(int *)item_get(item) != TEST_VALUE)
    return false;
  return true;
}

struct test_struct {
  int a;
  int b;
};
bool overwrite_item(ht_t *h) {
  // add a struct
  struct test_struct value = {1, 2};
  if (!htab_set(h, TEST_KEY, &value, sizeof(struct test_struct)))
    return false;
  return true;
}

bool get_overwritten_item(ht_t *h) {
  // get the struct we added
  item_t *item = htab_find(h, TEST_KEY);
  if (item == NULL)
    return false;
  struct test_struct *value = item_get(item);
  if (value->a != 1 || value->b != 2)
    return false;
  return true;
}

bool find_non_existent(ht_t *h) {
  // find a non-existent item
  item_t *item = htab_find(h, "non-existent");
  if (item != NULL)
    return false;
  return true;
}

bool find_removed(ht_t *h) {
  // find a non-existent item
  item_t *item = htab_find(h, TEST_KEY);
  if (item != NULL)
    return false;
  return true;
}

bool remove_item(ht_t *h) {
  // remove the item we added
  if (!htab_remove(h, TEST_KEY))
    return false;
  return true;
}

int main(void) {
  // Initialise
  // set color to yellow
  printf("\033[0;33m");
  printf("Testing Hashtable\n");
  // reset color
  printf("\033[0m");
  ht_t *h = NULL;
  h = htab_create(h, INITIAL_CAPACITY);

  // Run tests
  setlocale(LC_CTYPE, "");
  wchar_t cross = 0x00D7;
  wchar_t check = 0x2713;

  int num_tests = 6;
  bool (*funcs[8])(ht_t * h) = {
      add_item,             /*0*/
      add_items,            /*1*/
      find,                 /*2*/
      overwrite_item,       /*3*/
      get_overwritten_item, /*4*/
      find_non_existent,    /*5*/
      remove_item,          /*6*/
      find_removed          /*7*/
  };
  int num_passed = 0;
  for (int i = 0; i < num_tests; i++) {
    if ((*funcs[i])(h)) {
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

  htab_destroy(h);

  if (num_passed == num_tests) {
    // set color to green
    printf("\033[0;32m");
    printf("---------------------\n");
    printf("All Hashtable Tests passed\n");
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