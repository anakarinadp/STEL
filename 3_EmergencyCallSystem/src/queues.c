#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <sys/stat.h>
#include "lib/linked_list.h"

#define EVENTS_LIST 100000
#define LAMBDA 10 // Calls per minute (600/hour)
#define L 1000 // Finite buffer size of PC
#define DM 1.5 // Mean time of exponential distribution (PC calls)
#define DM_TRANSF 0.75
#define M 8 // service positions at PC 
#define M_INEM 8 // service positions at INEM
#define K 20000 // Population size of potential clients

#define MU 0.75 // Mean = 45s
#define SIGMA 0.25 // Standard deviation = 15s

#define ARRIVAL 1
#define DEPARTURE 2
#define ARRIVAL_EMERGENCY 3
#define DEPARTURE_EMERGENCY 4

/*
int saveInCSV(char* filename, int* histogram, int hist_size) {
  FILE *CSV;
  int i;
  fprintf(stdout, "\n\tSaving the histogram in \"%s\"\n", filename);

  CSV = fopen(filename,"w+");
  if(CSV == NULL) {
    perror("fopen");
    return -1;
  }
  fprintf(CSV, "Index, Delay\n");
  for (i = 0; i < hist_size; i++) {
    fprintf(CSV, "%d, %d\n", i, histogram[i]);
  }
  fclose(CSV);

  return 0;
}
*/

double gaussianDistribution()
{
	// Note: M_E and M_PI belong to "math.h" library
	double Z1;
	static int generate = 0;
	generate = !generate;

	if (!generate)
	   return Z1 * SIGMA + MU;

	double U1, U2;
	U1 = (rand()%RAND_MAX + 1) / (double)RAND_MAX;
	U2 = (rand()%RAND_MAX + 1) / (double)RAND_MAX;

	double Z0;
	Z0 = sqrt(-2.0 * log(U1)) * cos(2*M_PI * U2); 
	Z1 = sqrt(-2.0 * log(U1)) * sin(2*M_PI * U2);
	return Z0 * SIGMA + MU;
}

double calcTime(double lt, int type) {
  double u, C, S, E;
  u = (rand()%RAND_MAX + 1) / (double)RAND_MAX;

  if (type == DEPARTURE) {
    // service time
    do {
       S = -(log(u)*DM);
    } while((S < 1) || (S > 4));// Min time=1min, Max time=4min
    return S;
  } else if (type == DEPARTURE_EMERGENCY) { 
  	printf("Inside calcTime DEPARTURE_EMERGENCY\n");
  	do {
  		E = gaussianDistribution();
  		printf("E value: %f\n", E);
  	} while((E < 0.5) || (E > 1.25)); // Min time=30s, Max time=0.75s
   	return E;
  } else { // It's an arrival
  	// Poisson process
    // mean time between consecutive arrivals
    C = -(log(u)/(double)lt);
    return C;
  }
  /*
  E = -(log(u)*DM_TRANSF);
  if (E < 35) {
    calcTime(lt, type);
  }
  if (E > 75) {
    calcTime(lt, type);
  }
  return E;
 */  
}



/*
double boxMuller(void)
{
  double U1, U2, W, mult;
  static double X1, X2;
  static int call = 0;

	  if (call == 1)
	    {
	      call = !call;
	      return (mu + sigma * (double) X2);
	    }

	  do
	    {
	      U1 = -1 + ((double) rand () / RAND_MAX) * 2;
	      U2 = -1 + ((double) rand () / RAND_MAX) * 2;
	      W = U1*U1 + U2*U2;
	    }
	  while (W >= 1 || W == 0);

	  mult = sqrt ((-2 * log (W)) / W);
	  X1 = U1 * mult;
	  X2 = U2 * mult;

	  call = !call;

	  return (mu + sigma * (double) X1);
}
*/

list * addNewEvent(double t, double lt, int type, list* event_list) {
  double event_time = 0;
  event_time = t + calcTime(lt, type);
  event_list = add(event_list, type, event_time);
  return event_list;
}

void calcDelayProb(double t, int tot_delay, int* hist, int h_size) {
  int num_delay = 0;
  for (int i = 0; i < (int)t; i++) {
    num_delay += hist[i];
  }
  printf("\tDelay: %d\n", num_delay);
  printf("\tTotal delay: %d\n", tot_delay);
  fprintf(stdout, "\tThe probability is Pa(a<%.3f) = %.3f%%\n", t, ((float)num_delay) / tot_delay * 100);
}

// Function to tell the user the mean time he will have to wait based on system load
void tellDelay(float tot_delay, int arrivals) {
  printf("Delay: %.3f\n", tot_delay);
  printf("Arrival events: %d\n", arrivals);
  printf("Mean time of delay predicted: %.3f seconds\n", tot_delay / (float)(arrivals) * 60);
}

// Function that generates a random event that could be an emergency or not (60% prob of emeregency)
int arrivalOrEmergency () {
  srand(time(NULL));
  if ((rand() % 100) < 60) {
  	return ARRIVAL_EMERGENCY; 
  } else {
  	return ARRIVAL;
  }
}

// Function to manage the calls received at INEM
double callsInem(list* events_inem, list* buffer_inem, double lambda_total) {
	int channels_inem = 0;
	double delay = 0, aux_time = 0;

	if (events_inem->type == ARRIVAL) {
		if (channels_inem < M_INEM) { // If the channels aren't full
	        events_inem = addNewEvent(events_inem->_time, lambda_total, DEPARTURE, events_inem);
	        channels_inem++; // A channel serves the call
		} else { // Otherwise, the event needs to wait on buffer
	        buffer_inem = add(buffer_inem, ARRIVAL, events_inem->_time);
      	}
    }
	else { // Departure event
	  aux_time = events_inem->_time;
      events_inem = rem(events_inem);
      if (buffer_inem != NULL) { // As a channel is free now, it can serve an event waiting in the buffer
        events_inem = addNewEvent(aux_time, lambda_total, DEPARTURE, events_inem);
        delay = aux_time - buffer_inem->_time;
        buffer_inem = rem(buffer_inem);
        return delay;
      } else { // If the buffer is empty
        channels_inem--;
      }
    }
    return 0;
}

int main(int argc, char* argv[]) {
  // useful variables
  int i = 0, index = 0, losses = 0, turn = 0;
  int *histogram, hist_size = 0, aux_hist = 0;
  double lambda_total = 0, aux_time = 0, delay_time = 0;
  char filename[50];

  // PC variables
  int arrival_events = 0, channels = 0, buffer_size = L, buffer_events = 0;
  list *events = NULL, *buffer = NULL;

  // INEM variables
  int inem_events = 0;
  list *events_inem = NULL, *buffer_inem = NULL;

  double delay_pc_inem = 0, event_time = 0;
  //int emergency = 0, delay_count = 0;;
  //double t = -1;

  srand(time(NULL));
  /*
  strcat(filename, argv[1]);
  histogram = (int*)calloc(1, sizeof(int));
  */
  events = add(events, arrivalOrEmergency(), 0);


  // We process the list of events
  for(i = 1; i < EVENTS_LIST; i++) {
  	aux_time = events->_time;
  	if (turn)
  	{
  		printf("Inside inem events\n");
  		delay_pc_inem += callsInem(events_inem, buffer_inem, lambda_total);
  		turn = !turn;
  	}
    if (events->type == ARRIVAL) { // PC arrival event
    	printf("Arrival\n");
      arrival_events++;  // counting the arrival events
      lambda_total = (K - channels - (L-buffer_size)) * LAMBDA; // arrival rate of the system

      // If isn't an emergency, call is processed by PC
      if (channels < M) { // if the channels aren't full
      	printf("Channels not full\n");
        events = addNewEvent(events->_time, lambda_total, DEPARTURE, events);
        printf("Event added\n");
    	print_elems(events);
        channels++; // A channel serves the call
      } else if (buffer_size > 0) { // if the buffer has space
          buffer = add(buffer, ARRIVAL, events->_time);
          buffer_size--;
          buffer_events++;
          tellDelay(delay_time, arrival_events);
      } else { // if the channels and buffer are occupied we get a lost event
        losses++;
      }
      // Remove the actual arrival event and add a new one
      events = rem(events);
      events = addNewEvent(aux_time, lambda_total, arrivalOrEmergency(), events);

    } else if (events->type == DEPARTURE) { // Departure event
    	printf("Departure\n");
      events = rem(events);
      if (buffer != NULL) { // As a channel is free now it can serve an event waiting in the buffer
        events = addNewEvent(aux_time, lambda_total, DEPARTURE, events);

        delay_time += aux_time - buffer->_time;
        /*
        index = (int)((aux_time - buffer->_time)*60);
        if (index + 1 > hist_size) {
          aux_hist = hist_size;
          hist_size = index + 1;
          histogram = (int*)realloc(histogram, hist_size * sizeof(int));
          for (i = aux_hist; i < hist_size; i++) {
            histogram[i] = 0;
          }
          if (histogram == NULL) {
            perror("realloc");
            return -1;
          }
        }
        histogram[index]++;
        */

        buffer = rem(buffer);
        buffer_size++;
      } else { // If the buffer is empty
        channels--;
      }
    } else if (events->type == ARRIVAL_EMERGENCY) { 
    	printf("Emergency arrival\n");
      inem_events++;  // Counting the arrival events
      lambda_total = (K - channels - (L-buffer_size)) * LAMBDA; // arrival rate of the system
      delay_pc_inem = calcTime(lambda_total, DEPARTURE_EMERGENCY);
      printf("Delay PC-INEM: %.2f\n", delay_pc_inem);
  	  event_time = events->_time + delay_pc_inem;
      events = add(events, DEPARTURE_EMERGENCY, event_time);
      print_elems(events);
      // Remove the actual arrival event and add a new one
      events = rem(events);
      // The arrival of that call originates a new arrival event
      events = addNewEvent(aux_time, lambda_total, arrivalOrEmergency(), events);
    } else { // It's a DEPARTURE_EMERGENCY, what means that the call is transfered to INEM
      printf("Departure emergency\n");
      events_inem = add(events_inem, ARRIVAL, events->_time);
      events = rem(events);
      delay_pc_inem += callsInem(events_inem, buffer_inem, lambda_total);
    }
  }
/*
  if(saveInCSV(filename, histogram, hist_size) < 0) {
    perror("saveInCSV");
    return -1;
  }
  fprintf(stdout, "\tFile \"%s\" saved successfully\n\n", filename);
*/

  fprintf(stdout, "\nProbability of a call being delayed at the entry to the PC system (buffer events = %d): %.3f%%\n",
                     buffer_events, buffer_events / (float)(arrival_events+inem_events) * 100);

  fprintf(stdout, "\nProbability of a call being lost (losses = %d) at the entry to the PC system (total arrivals = %d): %.3f%%\n",
                     losses, arrival_events, losses / (float)(arrival_events+inem_events) * 100);

  fprintf(stdout, "\nAverage delay time of calls in the PC system entry: %.3fmin = %.3fsec\n",
                     delay_time / (float)(arrival_events+inem_events), delay_time / (float)(arrival_events+inem_events) * 60);

  fprintf(stdout, "\nAverage delay time of calls from PC to INEM: %.3fmin = %.3fsec\n\n",
                     delay_pc_inem / (float)inem_events, delay_pc_inem / (float)(inem_events) * 60);

  //free(histogram);

  return 0;
}
