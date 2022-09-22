#include <stdio.h>
#include <stdlib.h>
#include "hashtable.h"

#define INITIAL_CAPACITY 10

// initialise a new hash table with 10 buckets
bool initialise(ht_t *h)
{
    if (!htab_init(h, INITIAL_CAPACITY))
        return false;
    return true;
}

bool destroy(ht_t *h)
{
    htab_destroy(h);
    return true;
}

bool add_item(ht_t *h)
{
    // add one item (don't grow)
    if (!htab_set(h, "hello", 1))
        return false;
    return true;
}

bool add_items(ht_t *h)
{
    // add heaps of items (grow the table a couple of times)
    htab_set(h, "B", 100);
    htab_set(h, "A1", 1);
    htab_set(h, "B1", 3);
    htab_set(h, "C1", 3);
    htab_set(h, "A2", 1);
    htab_set(h, "B2", 3);
    htab_set(h, "C2", 3);
    htab_set(h, "A3", 1);
    htab_set(h, "B3", 3);
    htab_set(h, "C3", 3);
    htab_set(h, "A4", 1);
    htab_set(h, "B4", 3);
    return true;
}

int main(void)
{
    ht_t *h = calloc(1, sizeof(ht_t *));
    // int num_tests = 4;
    // bool (*funcs[4])(ht_t * h) = {
    //     initialise,
    //     add_item,
    //     add_items,
    //     destroy};
    // int num_passed = 0;
    // for (int i = 0; i < num_tests; i++)
    // {
    //     printf("Test %d: \n", i);
    //     if ((*funcs[i])(h))
    //     {
    //         printf("Test %d passed\n", i);
    //     }
    //     else
    //     {
    //         printf("Test %d failed\n", i);
    //     }
    //     num_passed++;
    // }
    int result = initialise(h);
    if (result)
    {
        printf("Initialise passed\n");
    }
    else
    {
        printf("Initialise failed\n");
    }
    htab_print(h);
    // add_items(h);
    destroy(h);

    // printf("Passed %d/%d tests\n", num_passed, num_tests);

    return 0;
}