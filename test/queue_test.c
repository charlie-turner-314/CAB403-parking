#include "queue.h"
#include "testing.h"

bool push_item(Queue *q) {
  // push one item
  if (!queue_push(q, "test", 5))
    return false;
  return true;
}

bool push_item2(Queue *q) {
  // push one item
  if (!queue_push(q, "test2", 6))
    return false;
  return true;
}

bool peek_item(Queue *q) {
  // peek at the item at front of queue
  QItem *item = queue_peek(q);
  if (item == NULL)
    return false;
  if (strcmp(item->value, "test") != 0)
    return false;
  return true;
}

bool pop_item(Queue *q) {
  // pop the item at front of queue
  QItem *item = queue_peek(q);
  if (item == NULL)
    return false;
  if (strcmp(item->value, "test") != 0)
    return false;
  queue_pop(q);
  return true;
}

bool pop_item2(Queue *q) {
  // pop the item at front of queue
  QItem *item = queue_peek(q);
  if (item == NULL)
    return false;
  if (strcmp(item->value, "test2") != 0)
    return false;
  queue_pop(q);
  return true;
}

bool is_empty(Queue *q) {
  // check if queue is empty
  if (q->head || q->tail)
    return false;
  return true;
}

// PRE: push_item has been called
bool has_item(Queue *q) {
  // check that queue has an item
  if (q->head == NULL || q->tail == NULL)
    return false;
  return true;
}

int main(void) {
  // Initialise
  // set color to yellow
  printf("\033[0;33m");
  printf("Testing Queue\n");
  // reset color
  printf("\033[0m");
  Queue *q = queue_create();

  // Run tests
  setlocale(LC_CTYPE, "");
  wchar_t cross = 0x00D7;
  wchar_t check = 0x2713;

  int num_tests = 7;
  bool (*funcs[7])(Queue * q) = {
      push_item,  /* 0 */
      has_item,   /* 1 */
      peek_item,  /* 2 */
      push_item2, /* 3 */
      pop_item,   /* 4 */
      pop_item2,  /* 5 */
      is_empty    /* 6 */
  };
  int num_passed = 0;
  for (int i = 0; i < num_tests; i++) {
    if ((*funcs[i])(q)) {
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

  destroy_queue(q);

  if (num_passed == num_tests) {
    // set color to green
    printf("\033[0;32m");
    printf("---------------------\n");
    printf("All Queue Tests Passed\n");
    // reset color
    printf("\033[0m");
  } else {
    // set color to red
    printf("\033[0;31m");
    printf("Passed %d/%d Queue tests\n", num_passed, num_tests);
    // reset color
    printf("\033[0m");
  }

  return 0;
}