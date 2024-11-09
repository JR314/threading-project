
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

typedef struct {
  int id;
  int loading_time;
  int crossing_time;
  char direction;
  char* realDirection;
  int sequence;
  int priority;
  char loaded;
} Train;


float init_time;
struct timespec begin, end;
#define BILLION 1000000000.0
struct timespec start, stop;

char* lastDirection;
int consecutiveSameDirectionCount = 0;
int nextTrainToGo = -1;
long simulationTimeTenths = 0;

pthread_mutex_t trackMutex; //synchronization for crossing
pthread_cond_t* trainCond = NULL; //individual condition variables to signal each train
pthread_mutex_t arrayMutex = PTHREAD_MUTEX_INITIALIZER; //synchronization for putting the loaded trains to a dynamic array

//mutex and condition variable for synchronization and signal to start loading
pthread_mutex_t loadingMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t loadingSignal = PTHREAD_COND_INITIALIZER;
int isLoaded = 0;

int trainsCrossing = 0;               // Number of trains currently crossing
pthread_mutex_t crossingMutex;        // Mutex for synchronization
pthread_cond_t crossingDoneSignal;    // Condition variable to signal when crossing is done

Train* trains = NULL;  // Dynamic array for trains
int numTrains = 0;     // Total number of trains

Train** loadedTrains = NULL;  // Array of pointers to loaded Train structures
int numLoadedTrains = 0;

void readInput(const char* filename) {
  FILE *file = fopen(filename, "r");
  if (!file) {
      perror("Unable to open file");
      exit(1);
  }

  char line[100];
  while (fgets(line, sizeof(line), file)) {
        // Check if the line read is valid
        if (strnlen(line, sizeof(line)) < 5) {  // At least: E 1 1\n
            fprintf(stderr, "Invalid line in file: %s\n", line);
            continue;
        }

        numTrains++;
        trains = realloc(trains, numTrains * sizeof(Train));
        if (!trains) {
            perror("Memory allocation failed");
            exit(1);
        }

        Train* train = &trains[numTrains - 1];

        char direction;
        int loadingTime, crossingTime;
        int itemsScanned = sscanf(line, "%c %d %d", &direction, &loadingTime, &crossingTime);

        // Check if the sscanf call was successful and got all required fields
        if (itemsScanned != 3) {
           fprintf(stderr, "Invalid line format: %s\n", line);
           numTrains--;  // Decrement because this line was invalid
           continue;
        }

        // Further check for valid direction
        if (!(direction == 'E' || direction == 'W' || direction == 'e' || direction == 'w')) {
           fprintf(stderr, "Invalid direction in line: %s\n", line);
           numTrains--;  // Decrement because this line was invalid
           continue;
        }

        train->id = numTrains - 1;
        train->sequence = numTrains - 1;
        train->direction = direction;
        train->loading_time = loadingTime * 100000;  // Convert to useconds for usleep
        train->crossing_time = crossingTime * 100000;
        if (direction == 'E' || direction == 'W') {
          train->priority = 1;
        } else if (direction == 'e' || direction == 'w') {
          train->priority = 0;
        }

    }
    fclose(file);

}

//printtime management
float calc_accum_time() {
  if (clock_gettime(CLOCK_MONOTONIC, &stop) == -1) {
    perror("Could not get time.");
    exit(EXIT_FAILURE);
  }

  double accum = (stop.tv_sec - start.tv_sec) + (stop.tv_nsec - start.tv_nsec) / BILLION;
  double accum_now = (double)accum - (double)init_time;

  printf("%02d:%02d:%04.1f ", (int) accum_now / (60 * 60), (int) accum_now / 60, accum_now);

  return accum;
}

void sortLoadedTrains() {

  for(int i = 0; i < numLoadedTrains; i++) {
      for(int j = i+1; j < numLoadedTrains; j++) {
          if(loadedTrains[i]->priority < loadedTrains[j]->priority ||
            (loadedTrains[i]->priority == loadedTrains[j]->priority && loadedTrains[i]->loading_time > loadedTrains[j]->loading_time) ||
            (loadedTrains[i]->priority == loadedTrains[j]->priority && loadedTrains[i]->loading_time == loadedTrains[j]->loading_time && loadedTrains[i]->id > loadedTrains[j]->id)) {
              Train* temp = loadedTrains[i];
              loadedTrains[i] = loadedTrains[j];
              loadedTrains[j] = temp;
          }
      }
  }
}

Train* dispatchBasedOnPriority() {
    if(numLoadedTrains > 0) {
        return loadedTrains[0];
    }
    return NULL;
}

Train* dispatchBasedOnDirection() {

      //check if no train has pass the truck


      if (strcmp(lastDirection, "\0")){
        lastDirection = "East";
      }

      if (loadedTrains[0]->realDirection != lastDirection) {
          return loadedTrains[0];
        } else {
          Train* temp = loadedTrains[0];
          loadedTrains[0] = loadedTrains[1];
          loadedTrains[1] = temp;
          return loadedTrains[0];
        }

    printf("shouldn't be here\n");
    return NULL;
}

Train* dispatchForStarvationPrevention() {

    if (consecutiveSameDirectionCount == 3) {
        for(int i = 0; i < numLoadedTrains; i++) {
            if (strcmp(loadedTrains[i]->realDirection,lastDirection)) {

                consecutiveSameDirectionCount = 0;
                return loadedTrains[i];
            }
        }
    }
    return NULL;
}

void updateDirectionInfo(Train* train) {
    if (lastDirection != train->realDirection) {
        consecutiveSameDirectionCount = 1;
        lastDirection = train->realDirection;
    } else {
        consecutiveSameDirectionCount++;
    }
}

void removeTrainFromLoaded(Train* train) {

  int temp = 0;

  for(int i = 0; i < numLoadedTrains-1; i++) {

        if (loadedTrains[i]->id == train->id){

            temp = 1;

        }
        if (temp){
            loadedTrains[i] = loadedTrains[i+1];
        }

  }
  numLoadedTrains--;
  loadedTrains = realloc(loadedTrains, numLoadedTrains * sizeof(Train*));


}


void* trainThread(void* arg) {

    Train* train = (Train*) arg;

    char* directionString = (train->direction == 'E' || train->direction == 'e') ? "East" : "West";
    //train thread wait for other threads start processing
    pthread_mutex_lock(&loadingMutex);
    while(!isLoaded) {
        pthread_cond_wait(&loadingSignal, &loadingMutex);
    }

    pthread_mutex_unlock(&loadingMutex);
    //loading start
    usleep(train->loading_time);
    //loading end
    calc_accum_time();
    printf("Train %2d is ready to go %4s\n", train->id, directionString);

    // Add to the queue
    pthread_mutex_lock(&arrayMutex);
    numLoadedTrains++;
    loadedTrains = realloc(loadedTrains, numLoadedTrains * sizeof(Train*));
    loadedTrains[numLoadedTrains - 1] = train;
    train->sequence = numLoadedTrains;
    train->realDirection = directionString;
    pthread_mutex_unlock(&arrayMutex);

    // crossing the track
    pthread_mutex_lock(&trackMutex);

    while(nextTrainToGo != train->id) {
      pthread_cond_wait(&trainCond[train->id], &trackMutex);
    }

    calc_accum_time();
    printf("Train %2d is ON the main track going %4s\n", train->id, directionString);
    trainsCrossing++;

    usleep(train->crossing_time);

    trainsCrossing--;
    if (trainsCrossing == 0) {
       pthread_cond_signal(&crossingDoneSignal);
    }

    pthread_mutex_unlock(&trackMutex);
    simulationTimeTenths += train->crossing_time / 100000;

    calc_accum_time();
    printf("Train %2d is OFF the main track after going %4s\n", train->id, directionString);
    numTrains--;
    //crossing end
    return NULL;

}

void* dispatcherThread(void* arg) {

    while(1) {

      if (numTrains == 0) {
        break;
      }

      pthread_mutex_lock(&trackMutex);



        // Sort loadedTrains based on priority, loadingTime and ID.
        sortLoadedTrains();



        // Implement rule 4a: Priority dispatch
        Train* nextTrain = dispatchBasedOnPriority();

        // Implement rule 4c: Starvation prevention

        if (consecutiveSameDirectionCount == 3) {


            nextTrain = dispatchForStarvationPrevention();

        }


        // Implement rule 4b: If two trains have the same priority, check direction
        else if (numLoadedTrains > 1) {// loaded >=2
            if(loadedTrains[0]->priority == loadedTrains[1]->priority) {//same priority
                if (loadedTrains[0]->realDirection != loadedTrains[1]->realDirection) {// opposite direciton

                    nextTrain = dispatchBasedOnDirection();
                }
            }
        }



        if (nextTrain) {

            // Update direction count and last direction
            updateDirectionInfo(nextTrain);

            nextTrainToGo = nextTrain->id;

            pthread_cond_signal(&trainCond[nextTrain->id]);

            // Remove the dispatched train from the loadedTrains list
            removeTrainFromLoaded(nextTrain);

        }


      pthread_mutex_unlock(&trackMutex);
      usleep(10000);  // Sleep for a short time to prevent aggressive CPU usage.

      pthread_mutex_lock(&crossingMutex);

      while (trainsCrossing > 0) {
         pthread_cond_wait(&crossingDoneSignal, &crossingMutex);
      }
      // Now it's safe to proceed since no trains are crossing
      pthread_mutex_unlock(&crossingMutex);

    }

    return NULL;
}



int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input_filename>\n", argv[0]);
        return 1;
    }

    // Read input and initialize trains
    readInput(argv[1]);

    // Initialize mutex and cond var
    if(pthread_mutex_init(&trackMutex, NULL) != 0) {
        perror("Error initializing trackMutex");
        exit(1);
    }

    trainCond = malloc(numTrains * sizeof(pthread_cond_t));
    if (trainCond == NULL) {
        perror("Error allocating memory for condition variables");
        exit(1);
    }

    for (int i = 0; i < numTrains; i++) {
        if(pthread_cond_init(&trainCond[i], NULL) != 0) {
            perror("Error initializing condition variables");
            exit(1);
        }
    }

    // Create train threads
    pthread_t *threads = malloc(numTrains * sizeof(pthread_t));
    if (threads == NULL) {
        perror("Error allocating memory for threads");
        exit(1);
    }


    for (int i = 0; i < numTrains; i++) {
        if (pthread_create(&threads[i], NULL, trainThread, &trains[i]) != 0) {
            perror("Error creating thread");
            exit(1);
        }
    }

    //signal all the trains to start loading
    pthread_mutex_lock(&loadingMutex);
    isLoaded = 1;
    pthread_cond_broadcast(&loadingSignal);

    pthread_mutex_unlock(&loadingMutex);

    pthread_mutex_init(&crossingMutex, NULL);
    pthread_cond_init(&crossingDoneSignal, NULL);



    // Create dispatcher thread
    pthread_t dispatcher;

    if (pthread_create(&dispatcher, NULL, dispatcherThread, NULL) != 0) {
        perror("Error creating dispatcher thread");
        exit(1);
    }
    //initialize the time
    clock_gettime(CLOCK_MONOTONIC, &begin);
    double cum = (begin.tv_sec - start.tv_sec) + (begin.tv_nsec - start.tv_nsec) / BILLION;
    init_time = cum;

    char* string;

    lastDirection = string;


    for (int i = 0; i < numTrains; i++) {
        pthread_join(threads[i], NULL);// Joining train threads
    }

    pthread_join(dispatcher, NULL); // Joining dispatcher thread

    // Cleanup

    pthread_mutex_destroy(&loadingMutex);
    pthread_mutex_destroy(&arrayMutex);
    pthread_mutex_destroy(&trackMutex);
    pthread_mutex_destroy(&crossingMutex);

    for (int i = 0; i < numTrains; i++) {
        pthread_cond_destroy(&trainCond[i]);
    }

    pthread_cond_destroy(&loadingSignal);
    pthread_cond_destroy(&crossingDoneSignal);

    free(trainCond); trainCond = NULL;
    free(threads); threads = NULL;
    free(trains); trains = NULL;
    free(loadedTrains);

    return 0;
}
