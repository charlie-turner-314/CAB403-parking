#pragma once

/*
Handle entry for cars
1. wait for a car to trigger the entrance LPR
2. if they aren't in the allowed list of cars, reject them with 'X' and go to
step 1
3. If the carpark is full, reject them with 'F' and go to step 1
4. Choose a random available level to park on, and set the sign to that level
5. Raise the boomgate
6. After 20ms, lower the boomgate
*/
void *entry_handler(void *arg);

/*
Handle the level entry/exit system
1. wait for a car to trigger level LPR
2. if the car is currently on the level, unassign it from the level
as it is exiting
3. if the car is not on the level, assign it to the level if there is space
4. If there is no space, set the cars current level but don't assign it as there
is no space
*/
void *level_handler(void *arg);

/*
Handle the carpark exit system
1. Wait for a car to trigger the exit LPR
2. Unassign the car from the carpark
3. Open the boomgate

*/
void *exit_handler(void *arg);

/*
Handle any user input for the manager display

- `q` to quit the program gracefully. This assumes the simulator
    has stopped running, as the manager will not be able to handle any
    cars currently in the carpark after quitting
*/
void *input_handler();