#pragma once
// Name of the shared memory object
#define SHM_NAME "PARKING"
// Number of entrances in the parking lot
#define NUM_ENTRANCES 5
// Number of exits in the parking lot
#define NUM_EXITS 5
// Number of levels in the parking lot
#define NUM_LEVELS 5
// How many cars are allowed on each level
#define LEVEL_CAPACITY 20
// perhaps unintuitively, how much to slow time
#define TIME_FACTOR 50
// how much to charge customers per milisecond
#define COST_PER_MS 0.05
