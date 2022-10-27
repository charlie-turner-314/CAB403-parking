#pragma once
// Name of the shared memory object
#define SHM_NAME "PARKING"
// Number of entrances in the parking lot
#define NUM_ENTRANCES 1
// Number of exits in the parking lot
#define NUM_EXITS 1
// Number of levels in the parking lot
#define NUM_LEVELS 1
// How many cars are allowed on each level
#define LEVEL_CAPACITY 1
// perhaps unintuitively, how much to slow time
#define TIME_FACTOR 5
// how much to charge customers per milisecond
#define COST_PER_MS 0.05
