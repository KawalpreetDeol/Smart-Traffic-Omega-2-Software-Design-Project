#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <ugpio/ugpio.h>

typedef int bool;
#define true 1
#define false 0

const float MAX_SPEED = 0.35;
const double distance = 0.14;

FILE *output;

typedef struct {
	int numModes;
	float* modes;
} Mode;

typedef struct {
	Mode* mode;
	float min;
	float avg;
	float max;
	float popStdDev;
	float smplStdDev;

	int minIndex, maxIndex;

} Statistics;

typedef struct {
	int numberOfCars;
	float *speeds;
	Statistics* stats;
} Interval;

typedef struct {
	Interval* intervals;
	Statistics* overall;
	float* averageSpeeds;
	bool active;

} Road;

void delay(unsigned int ms)
{
    clock_t elapsed = ms + clock();
    while (elapsed > clock());
}

float sum(float dataset[], int size) {
	float sigma = 0;
	for (int i = 0; i < size; i++) {
		sigma = sigma += dataset[i];
	}
	return sigma;
}

void sort(float dataset[], int low, int high) {
	int left = low, right = high;
    float temp;
    float pivot = dataset[(low + high) / 2];

    while (left <= right) {
		while (dataset[left] < pivot)
            left++;
        while (dataset[right] > pivot)
            right--;
        if (left <= right) {
            temp = dataset[left];
            dataset[left] = dataset[right];
            dataset[right] = temp;
            left++;
            right--;
        }
    }
	
    if (low < right)
        sort(dataset, low, right);
    if (left < high)
        sort(dataset, left, high);
}

Mode* computeModes(float input[], int size){
	Mode* outModes = malloc(sizeof(Mode));
	outModes->modes = malloc(size*sizeof(float));
	outModes->numModes = 0;

	float dataset[size];

	for (int i = 0; i < size; i++) {
		dataset[i] = input[i];
	}


	sort(dataset, 0, size-1);
	
	float tempNum = dataset[0];
	int currentCount = 0;
	int maxCount = 0;
	

	for (int j = 0; j < size; j++) {
		if (tempNum == dataset[j]) {
			++currentCount;
		} else {
			if (currentCount > maxCount) {
				for (int k = 0; k < outModes->numModes; k++) {
					outModes->modes[k] = 0;
				}
				outModes->numModes = 0;
				outModes->modes[outModes->numModes] = tempNum;
				++outModes->numModes;
				maxCount = currentCount;
			} else if (currentCount == maxCount) {
				outModes->modes[outModes->numModes] = tempNum;
				++outModes->numModes;
			}
			tempNum = dataset[j];
			currentCount = 1;
		}
	}
	
	if (currentCount > maxCount) {
		for (int k = 0; k < outModes->numModes; k++) {
			outModes->modes[k] = 0;
		}
		outModes->numModes = 0;
		maxCount = currentCount;
		outModes->modes[outModes->numModes] = tempNum;
		++outModes->numModes;
	} else if (currentCount == maxCount) {
		outModes->modes[outModes->numModes] = tempNum;
		++outModes->numModes;
	}
	
	return outModes;
	
}

float computeMin(float dataset[], int size) {
	float min = dataset[0];
	for (int i = 0; i < size; i++) {
		if(dataset[i] < min) {
			min = dataset[i];
		}
	}

	return min;
}

float computeMax(float dataset[], int size) {
	float max = dataset[0];
	for (int i = 0; i < size; i++) {
		if(dataset[i] > max) {
			max = dataset[i];
		}
	}

	return max;
} 

float computeAvg(float dataset[], int size) {
	return sum(dataset,size) / (float)size;
}

float computeSampleStandardDev(float dataset[], float avg, int size) {
	float sum = 0;
	
	for (int i = 0; i < size; i++) {
		sum += pow((dataset[i] - avg),2);
	}
	
	return sqrt(sum / (float)(size-1));
}

float computePopulationStandardDev(float dataset[], float avg, int size) {
	float sum = 0;
	
	for (int i = 0; i < size; i++) {
		sum += pow((dataset[i] - avg),2);
	}
	
	return sqrt(sum / (float)size);
} 

Statistics* computeStatistics(float dataset[], int size) {
	Statistics* s;
	s = malloc(sizeof(Statistics));
	s->min = computeMin(dataset, size);
	s->max = computeMax(dataset, size);

	for (int i = 0; i < size; i++) {
		if (dataset[i] == s->max) {
			s->maxIndex = i+1;
		}
		if (dataset[i] == s->min) {
			s->minIndex = i+1;
		}
	}

	s->avg = computeAvg(dataset, size);
	if (size > 1) {	
		s->popStdDev = computePopulationStandardDev(dataset, s->avg, size);
		s->smplStdDev = computeSampleStandardDev(dataset, s->avg, size);
	} else {
		printf("WARNING: Not enough data to compute Standard Deviation\n");
		s->popStdDev = 0;
		s->smplStdDev = 0;	
	}
	s->mode = computeModes(dataset, size);
	return s;
};

void writeStatistics(Interval interval, Statistics* overall) {
	output = fopen("log.txt","a+");

	printf( "Cars through intersection: %d\n", interval.numberOfCars);
	printf( "Minimum Detected Speed: %f\n", interval.stats->min);
	printf( "Maximum Detected Speed: %f\n", interval.stats->max);
	printf( "Average Detected Speed: %f\n", interval.stats->avg);
	printf( "Sample Standard Deviation: %f\n", interval.stats->smplStdDev);
	printf( "Population Standard Deviation: %f\n", interval.stats->popStdDev);
	printf( "Mode(s): ");

	for (int i = 0; i < interval.stats->mode->numModes; i++) {
		if(i != interval.stats->mode->numModes) {
			printf( "%f, ", interval.stats->mode->modes[i]);
		} else {
			printf( "%f ", interval.stats->mode->modes[i]);
		}
	}

	printf( "(Appears %d times)\n", interval.stats->mode->numModes);

	printf( "------------------------------------------------------\n");
	printf( "Overall Average Speed: %f\n", overall->avg);
	printf( "Average Speed is SLOWEST during Interval %d (%f)\n", overall->minIndex, overall->min);
	printf( "Average Speed is FASTEST during Interval %d (%f)\n", overall->maxIndex, overall->max);
	if (overall->popStdDev != 0) {
		printf( "Overall Sample Standard Deviation: %f\n", overall->smplStdDev);
		printf( "Overall Population Standard Deviation: %f\n\n\n", overall->popStdDev);
	}

	fclose(output);
}

int main() {
	int road1A = 11;
	int road1B = 3;
	int road2A = 2;
	int road2B = 1;
	int maxIntervals = 4;	
  
	gpio_request(road1A, NULL);
	gpio_direction_input(road1A);
	gpio_request(road1B, NULL);
	gpio_direction_input(road1B);
	gpio_request(road2A, NULL);
	gpio_direction_input(road2A);
	gpio_request(road2B, NULL);
	gpio_direction_input(road2B);	

	int red1 = 19;
	int red2 = 18;
  	int green = 0;

	gpio_request(red1, NULL);
	gpio_direction_output(red1, 0);

	gpio_request(red2, NULL);	
	gpio_direction_output(red2, 0);

	gpio_request(green, NULL);
	gpio_direction_output(green, 1);
	gpio_set_value(green, 1);

	int value;

	Road road1, road2;

	road1.active = true;
	road2.active = false;

	road1.intervals = malloc(maxIntervals*sizeof(Interval));
	road1.overall = malloc(sizeof(Statistics));
	road1.averageSpeeds = malloc(maxIntervals*sizeof(float));

	road2.intervals = malloc(maxIntervals*sizeof(Interval));
	road2.overall = malloc(sizeof(Statistics));
	road2.averageSpeeds = malloc(maxIntervals*sizeof(float));

  	bool pressed = false;
	bool switching = false;
  
	int elapsed = 0;
  	int trigger = 10000;

	int value1, value2, value3;
  
  	clock_t begin, difference;
  	clock_t tStarts, tEnds;
  	float timeElapsed;
  	
  	road1.active = true;
  	road2.active = false;

	float* tempData;
	
	output = fopen("log.txt", "a+");

	time_t runtime;
	char* runtime_string;

	runtime = time(0);
	runtime_string = ctime(&runtime);
	
	fprintf(output, "RUNNING @ %s", runtime_string);
	
	delay(5000);
	
	printf("SMART TRAFFIC BEGINNING\n");
  
  	for (int currentInterval = 0; currentInterval < maxIntervals; currentInterval++) {
		trigger = 15000;
      		if (road1.active) {
			printf( "------- ROAD 1: Interval %d -------\n", (currentInterval/2)+1);
			printf("Switching Red Lights\n");
        		gpio_request(red2, NULL);
        		gpio_set_value(red2, 1);
			delay(3000);
        		gpio_request(red1, NULL);
        		gpio_set_value(red1, 0);
        		road1.intervals[currentInterval/2].numberOfCars = 0;
        		road1.intervals[currentInterval/2].speeds = malloc(sizeof(float));
        		road1.intervals[currentInterval/2].stats = malloc(sizeof(Statistics));
        
      		} else if (road2.active) {
			printf( "------- ROAD 2: Interval %d -------\n", (currentInterval/2)+1);
			printf("Switching Red Lights\n");
        		gpio_request(red1, NULL);
        		gpio_set_value(red1, 1);
			delay(3000);
        		gpio_request(red2, NULL);
        		gpio_set_value(red2, 0);
        		road2.intervals[(currentInterval / 2)].numberOfCars = 0;

        		road2.intervals[(currentInterval / 2)].speeds = malloc(sizeof(float));

        		road2.intervals[(currentInterval / 2)].stats = malloc(sizeof(Statistics));
      		}
      
      		elapsed = 0;
      		begin = clock();
      		do {
        		if (road1.active) {
        			gpio_request(road1A, NULL);
          			value1 = gpio_get_value(road1A);
           			gpio_request(road1B, NULL);
          			value2 = gpio_get_value(road1B);
				gpio_request(road2A, NULL);
				value3 = gpio_get_value(road2A);
          			
				if (value3 == 1 && (switching == false)) {
					switching = true;
					printf( "Car approached from other intersection, Changing lights shortly\n");
					begin = clock();
					elapsed = 0;
					trigger = 1500;				
				}

          			if (value1 == 1 && (pressed == false)) {
              				tStarts = clock();
					printf("Car entered\n");
              				pressed = true;
            			} else if (value2 == 1 && (pressed == true)) {
            				tEnds = clock();
					printf("Car left\n");
              				double timeElapsed = (double)(tEnds - tStarts) / CLOCKS_PER_SEC;
					printf("Time Elapsed: %lf\n", timeElapsed);
                			road1.intervals[currentInterval/2].numberOfCars++;
              				road1.intervals[currentInterval/2].speeds[road1.intervals[currentInterval/2].numberOfCars-1] = (float)(distance / timeElapsed);
					printf("Calculated Speed: %fm/s\n", road1.intervals[currentInterval/2].speeds[road1.intervals[currentInterval/2].numberOfCars-1]);
					if (road1.intervals[currentInterval/2].speeds[road1.intervals[currentInterval/2].numberOfCars-1] > MAX_SPEED) {
						printf("WARNING: Car %d going faster than speed limit\n", road1.intervals[currentInterval].numberOfCars);					
					} 
              				road1.intervals[currentInterval/2].speeds = realloc(road1.intervals[currentInterval/2].speeds, (road1.intervals[currentInterval/2].numberOfCars + 1) * sizeof(float));
              				pressed = false;
            			}
        		} else if (road2.active) {
        			gpio_request(road2A, NULL);
          			value1 = gpio_get_value(road2A);
           			gpio_request(road2B, NULL);
          			value2 = gpio_get_value(road2B);
				gpio_request(road1A, NULL);
				value3 = gpio_get_value(road1A);
          			
				if (value3 == 1 && (switching == false)) {
					switching = true;
					printf( "Car approached from other intersection, Changing lights shortly\n");
					begin = clock();
					elapsed = 0;
					trigger = 1500;				
				}

          			if (value1 == 1 && (pressed == false)) {
              				tStarts = clock();
					printf("Car entered\n");
              				pressed = true;
            			} else if (value2 == 1 && (pressed == true)) {
            				tEnds = clock();
					printf("Car left\n");
              				double timeElapsed = (double)(tEnds - tStarts) / CLOCKS_PER_SEC;
					printf("Time Elapsed: %lf\n", timeElapsed);
                			road2.intervals[currentInterval/2].numberOfCars++;
              				road2.intervals[currentInterval/2].speeds[road2.intervals[currentInterval/2].numberOfCars-1] = (float)(distance / timeElapsed);
					printf("Calculated Speed: %fm/s\n", road2.intervals[currentInterval/2].speeds[road2.intervals[currentInterval/2].numberOfCars-1]); 
					if (road2.intervals[currentInterval/2].speeds[road2.intervals[currentInterval/2].numberOfCars-1] > MAX_SPEED) {
						printf("WARNING: Car %d going faster than speed limit\n", road2.intervals[currentInterval].numberOfCars);					
					}
              				road2.intervals[currentInterval/2].speeds = realloc(road2.intervals[currentInterval/2].speeds, (road2.intervals[currentInterval/2].numberOfCars + 1) * sizeof(float));
              				pressed = false;
            			}
        		}
        		difference = clock() - begin;
        		elapsed = (difference*1000) / CLOCKS_PER_SEC;
      		} while(elapsed < trigger);
      		
		if (pressed) {
			pressed = false;
		}	
		
		if (switching) {
			switching = false;		
		}	

		fclose(output);

      		if (road1.active) {
			if (road1.intervals[currentInterval/2].numberOfCars > 0) {
				road1.intervals[currentInterval/2].stats = computeStatistics(road1.intervals[currentInterval/2].speeds, road1.intervals[currentInterval/2].numberOfCars);

				road1.averageSpeeds[currentInterval/2] = road1.intervals[currentInterval/2].stats->avg;
	
				road1.overall = computeStatistics(road1.averageSpeeds, (currentInterval/2)+1);

				writeStatistics(road1.intervals[currentInterval/2], road1.overall);
				free(road1.intervals[currentInterval/2].stats);
			} else {
				printf("WARNING: No cars passed during Interval %d and no stats were recorded.\n", (currentInterval/2) + 1);			
			}
      			road1.active = false;
        		road2.active = true;
      		} else {
			if (road2.intervals[currentInterval/2].numberOfCars > 0) {
				road2.intervals[currentInterval/2].stats = computeStatistics(road2.intervals[currentInterval/2].speeds, road2.intervals[currentInterval/2].numberOfCars);
				road2.averageSpeeds[currentInterval/2] = road2.intervals[currentInterval/2].stats->avg;
				road2.overall = computeStatistics(road2.averageSpeeds, (currentInterval/2) + 1);
				writeStatistics(road2.intervals[currentInterval/2], road2.overall);
				free(road2.intervals[currentInterval/2].stats);
			} else {
				printf("WARNING: No cars passed during Interval %d and no stats were recorded.\n", (currentInterval/2) + 1);			
			}
      			road2.active = false;
        		road1.active = true;
      		}
    	}
}