#README.txt
===========

This program simulates train traffic on a single railroad track, where trains can come from two directions, East (E) and West (W).
Trains from each direction can have either high priority (denoted by uppercase 'E' and 'W')
or low priority (denoted by lowercase 'e' and 'w').
Trains must load before crossing the track. The simulation adheres to the following rules:

1. High priority trains must be dispatched before low priority trains.
2. If two trains have the same priority, the train from the direction opposite to the last train that crossed should be dispatched.
3. If three trains in a row have crossed from the same direction, the next train, regardless of its priority, must be dispatched from the opposite direction to prevent starvation.

Program Design:
---------------
The program uses the following data structures and threads:

1. Train: A struct that stores information about each train, including its direction, loading time, crossing time, priority, and other necessary data.
2. trainThread: A thread function that simulates the loading and crossing of a train.
3. dispatcherThread: A thread function that dispatches trains according to the rules mentioned above.

The program uses several synchronization primitives to coordinate the actions of the train and dispatcher threads:

1. trackMutex: A mutex that ensures that only one train can cross the track at a time.
2. trainCond: An array of condition variables, one for each train, used to signal each train when it can cross the track.
3. loadingMutex: A mutex used to coordinate the loading of trains.
4. loadingSignal: A condition variable used to signal the trains when they can start loading.
5. arrayMutex: A mutex used to synchronize access to the dynamic array of loaded trains.
6. crossingMutex: A mutex used to synchronize access to the count of trains currently crossing the track.
7. crossingDoneSignal: A condition variable used to signal the dispatcher thread when all trains have finished crossing.

Code Structure:
---------------
1. main(): Reads input, initializes data structures and threads, starts the simulation, and then cleans up.
2. readInput(): Reads the input file and initializes the array of Train structs.
3. calc_accum_time(): Calculates and prints the accumulated time since the start of the simulation.
4. sortLoadedTrains(): Sorts the dynamic array of loaded trains according to the dispatch rules.
5. dispatchBasedOnPriority(): Dispatches a train based on its priority.
6. dispatchBasedOnDirection(): Dispatches a train based on its direction, in case two trains have the same priority.
7. dispatchForStarvationPrevention(): Dispatches a train from the opposite direction if three trains in a row have crossed from the same direction.
8. updateDirectionInfo(): Updates the count of consecutive trains from the same direction and the last direction a train came from.
9. removeTrainFromLoaded(): Removes a train from the dynamic array of loaded trains once it has been dispatched.
10. trainThread(): Simulates the loading and crossing of a train.
11. dispatcherThread(): Dispatches trains according to the rules.
