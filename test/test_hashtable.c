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
    int num_items = INITIAL_CAPACITY * 2;
    for (int i = 0; i < num_items; i++)
    {
        char key[4];
        key[0] = ('a' + i) % 26;
        key[1] = ('c' + i) % 26 + i;
        key[2] = ('g' - i) % 200;
        key[3] = '\0';
        if (!htab_set(h, key, i))
            return false;
    }
    return true;
}

int main(void)
{
    ht_t *h = malloc(sizeof(ht_t *));
    int num_tests = 2;
    bool (*funcs[4])(ht_t * h) = {initialise, add_item, add_items, destroy};
    int num_passed = 0;
    for (int i = 0; i < num_tests; i++)
    {
        if ((*funcs[i])(h))
        {
            printf("Test %d passed\n", i);
        }
        else
        {
            printf("Test %d failed\n", i);
        }
        num_passed++;
    }
    printf("Passed %d/%d tests\n", num_passed, num_tests);

    return 0;
}