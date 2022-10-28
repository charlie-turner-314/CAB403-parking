#include "logging.h"
#include <stdio.h>

// Wrapper for printf for the firealarm in order to comply with MISRA C
// obvoiusly this is not a good solution, and in practice with more time,
// we would implement our own logging system not using stdout and printf

void log_print_string(char *str) { printf("%s", str); }

void log_raise_alarm() { printf("\x1B[31mALARM ACTIVATED\x1B[0m\n"); }

void log_stop_alarm() { printf("\x1B[32mALARM DEACTIVATED\x1B[0m\n"); }