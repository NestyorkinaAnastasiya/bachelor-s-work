#include "library.h"
// Computational thread
void* worker(void* me) {
	ITask *currTask;
	int countOfProcess = size;
	// Execution of own tasks
	while (GetTask(&currTask)) {
		// It doesn't matter what communicator is current
		currTask->Run();
		// Creating the queue of executed tasks
		pthread_mutex_lock(&mutex_set_task);
		queueRecv.push(currTask);
		pthread_mutex_unlock(&mutex_set_task);
	}

	MPI_Comm Comm = currentComm;

	// If new ranks comes, their queue is empty	
	int  exitTask = 0;
	bool message = false;
	int sign = 1, id, k = 0;
	bool retry = false;

	// Task request from another ranks
	for (int i = 0; i < countOfProcess - 1; i++) {
		// If request from next rank
		if (!retry) {
			if (sign == 1) {
				sign = -1;
				k++;
			}
			else sign = 1;
			id = rank + sign * k;
			if (id > countOfProcess - 1) id -= countOfProcess;
			else if (id < 0) id += countOfProcess;
		}

		if (changeComm) {
			Comm = newComm; // как понять различия между?
		}

		// Send task request
		condition = 0;
		MPI_Send(&condition, 1, MPI_INT, id, 2001, Comm);
		MPI_Status st;
		// Get information about task existing
		MPI_Recv(&exitTask, 1, MPI_INT, id, 2002, Comm, &st);

		// If task exist, worker recieve and execute it
		if (exitTask) {
			ITask *t = new Task;
			pthread_mutex_lock(&mutex_set_task);
			GenerateRecv(t, id, Comm);
			queueRecv.push(t);
			pthread_mutex_unlock(&mutex_set_task);
			t->Run();
			// This rank can have new task for work
			retry = true;
			i--;
		}
		else retry = false;
	}

	return 0;
}
