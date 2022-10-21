#include <shm_parking.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>

struct SharedMemory *shm;

pthread_mutex_t alarm_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t alarm_condvar = PTHREAD_COND_INITIALIZER;

int alarm_active = 0;
int temps[NUM_LEVELS][5]; // 2D array to calc median value
int smoothed_temps[NUM_LEVELS][30];  // 2D array to store smoothed median values


// switches index of 2 array elements
void elementSwap(int *element1,int *element2) {
   int temp;
   
   temp = *element1; 
   *element1 = *element2; 
   *element2 = temp;
}


// ASC array sort
void arrSort(int arr[], int arrLen) { 
   for(int i = 0; i < arrLen-1; i++) {
      for(int j = 0; j < arrLen-i-1; j++) {
         if(arr[j] > arr[j+1]) 
            elementSwap(&arr[j], &arr[j+1]);
      }
   }
}


// find median value in raw temperature array
int median_calc(int level) {
    int rawTemp = shm->levels[level].temp; // get new temp from shm
	
 	memmove(&temps[level][0], &temps[level][1], 4*sizeof(temps[level][0])); // shift temps up in array
	temps[level][4] = rawTemp; // push newTemp to array
	
	// check that no empty values in array
	for (int i = 0; i < 5; i++) {
		if (temps[level][i] == INT_MIN) {
			// can't calc median value yet
			return -1;
		}
			
	}

	// median value can be calculated
	int tempsCopy[5]; 
	memcpy(tempsCopy, temps[level], sizeof(temps[level])); // make copy of array for sorting
	arrSort(tempsCopy, 5); //sort in asc first
	int median = tempsCopy[2]; //median is mid index
	return median;
}


// handles smoothed temperatures array
void smoothedTemp_handler(int level) {
	// add median to smoothed array
	int median = median_calc(level);
	if (median != -1) {
		memmove(&smoothed_temps[level][0], &smoothed_temps[level][1], 29*sizeof(int)); // shift temps up in array
		smoothed_temps[level][29] = median; // push median to array
	}
}


// monitorr the temperatures for conditions
void *temp_monitor(void *arg) {
	// init smoothed temps array with non-existing temperature value using INT_MIN  
	for (int i = 0; i < NUM_LEVELS; i++) {
			for (int j = 0; j < 30; j++) {
			smoothed_temps[i][j] = INT_MIN;
		}
	}

	size_t level_id = (size_t) arg;
	int level = (int)level_id;
	printf("Monitoring temperature on level %d\n", level);

	while(true) {
		int hightemps = 0;
		int emptyReadings = 0;
		smoothedTemp_handler(level);
		// fixed temperature fire detection
		for (int i = 0; i < 30; i++) {
			// Temperatures of 58 degrees and higher are a concern
			if (smoothed_temps[level][i] >= 58) {
				hightemps++;
			} 
			// check if array is valid i.e full with temp readings
			else if (smoothed_temps[level][i] == INT_MIN) {
				emptyReadings++;
			}
		}
		// If 90% of the last 30 temperatures are >= 58 degrees,
		// this is considered a high temperature. Raise the alarm
		if ((hightemps >= 30 * 0.9) && emptyReadings == 0) {
			alarm_active = 1;
		}
			
		// rate of rise fire detection
		// If the newest temp is >= 8 degrees higher than the oldest
		// temp (out of the last 30), this is a high rate-of-rise.
		// Raise the alarm
		if ((smoothed_temps[level][29] - smoothed_temps[level][0] >= 8) &&  (emptyReadings == 0)) {
			alarm_active = 1;
		}
		// sleep for 2 ms
		usleep(2000); 
	}
	return NULL;
}

// void *openboomgate(void *arg)
// {
// 	struct boomgate *bg = arg;
// 	pthread_mutex_lock(&bg->m);
// 	for (;;) {
// 		if (bg->s == 'C') {
// 			bg->s = 'R';
// 			pthread_cond_broadcast(&bg->c);
// 		}
// 		if (bg->s == 'O') {
// 		}
// 		pthread_cond_wait(&bg->c, &bg->m);
// 	}
// 	pthread_mutex_unlock(&bg->m);
	
// }


int main()
{
	printf("Firealarm System Running\n");
	fflush(stdout);

	// get the shared memory object
  	shm = get_shm(SHM_NAME);

	pthread_t *threads = malloc(sizeof(pthread_t) * NUM_LEVELS);

	for (size_t i = 0; i < NUM_LEVELS; i++) {
		pthread_create(threads + i, NULL, temp_monitor, (void *)i);
	}
	for (;;) {
		//temp_sim();
		if (alarm_active) {
			goto emergency_mode;
		}
		usleep(2000); // sleep for 2ms
	}
	
	emergency_mode:
	fprintf(stderr, "*** ALARM ACTIVE ***\n");
	
	// Handle the alarm system and open boom gates
	// Activate alarms on all levels
	for (int i = 0; i < NUM_LEVELS; i++) {
		shm->levels[i].alarm = 1; // set shm alarm to true
	}
	
	// Open up all boom gates
	// pthread_t *boomgatethreads = malloc(sizeof(pthread_t) * (NUM_ENTRANCES + NUM_EXITS));
	// for (int i = 0; i < NUM_ENTRANCES; i++) {
	// 	int addr = 288 * i + 96;
	// 	volatile struct boomgate *bg = shm + addr;
	// 	pthread_create(boomgatethreads + i, NULL, openboomgate, bg);
	// }
	// for (int i = 0; i < NUM_EXITS; i++) {
	// 	int addr = 192 * i + 1536;
	// 	volatile struct boomgate *bg = shm + addr;
	// 	pthread_create(boomgatethreads + NUM_ENTRANCES + i, NULL, openboomgate, bg);
	// }
	
	// // Show evacuation message on an endless loop
	// for (;;) {
	// 	char *evacmessage = "EVACUATE ";
	// 	for (char *p = evacmessage; *p != '\0'; p++) {
	// 		for (int i = 0; i < NUM_ENTRANCES; i++) {
	// 			int addr = 288 * i + 192;
	// 			volatile struct parkingsign *sign = shm + addr;
	// 			pthread_mutex_lock(&sign->m);
	// 			sign->display = *p;
	// 			pthread_cond_broadcast(&sign->c);
	// 			pthread_mutex_unlock(&sign->m);
	// 		}
	// 		usleep(20000);
	// 	}
	// }
	
	for (int i = 0; i < NUM_LEVELS; i++) {
		pthread_join(threads[i], NULL);
	}
}