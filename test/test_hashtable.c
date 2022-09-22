#include <stdio.h>
#include <stdlib.h>
#include "hashtable.h"

int main(void)
{
    ht_t *h = malloc(sizeof(ht_t *));
    htab_init(h, 10);
    htab_destroy(h);
    printf("Creating and destroying hashtable didn't seem to break\n");
    return 0;
}