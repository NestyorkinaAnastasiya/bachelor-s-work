#include "task.cpp"
int GetRank(int &sign, int &k, int countOfProcess) {
	if (sign == 1) {
		sign = -1;
		k++;
	}
	else sign = 1;
	int id = rank + sign * k;
	if (id > countOfProcess - 1) id -= countOfProcess;
	else if (id < 0) id += countOfProcess;
	return id;
}
void ExecuteOwnTasks() {
	ITask *currTask;
	// Execution of own tasks
	while (GetTask(&currTask)) {
		// It doesn't matter what communicator is current
		currTask->Run();
		// Creating the queue of executed tasks
		pthread_mutex_lock(&mutex_set_task);
		queueRecv.push(currTask);
		pthread_mutex_unlock(&mutex_set_task);
	}
}
void ExecuteOtherTask(MPI_Comm &Comm, int id, bool &retry) {
	// Send task request
	int existTask = 0;
	MPI_Send(&existTask, 1, MPI_INT, id, 2001, Comm);
	MPI_Status st;
	// Get information about task existing
	MPI_Recv(&existTask, 1, MPI_INT, id, 2002, Comm, &st);

	// If task exist, worker recieve and execute it
	if (existTask) {
		Task *t = new Task();
		pthread_mutex_lock(&mutex_set_task);
		t->GenerateRecv(id, Comm);
		queueRecv.push(t);
		pthread_mutex_unlock(&mutex_set_task);
		t->Run();
		// This rank can have new task for work
		retry = true;
	}
	else retry = false;
}
void ChangeCommunicator(MPI_Comm &Comm, int &newSize) {
	int message = 3;
	MPI_Request req;
	for (int i = 0; i < newSize; i++)
		MPI_Isend(&message, 1, MPI_INT, i, 1999, Comm, &req);
	Comm = newComm;
	newSize = size;
}
// Computational thread
void* worker(void* me) {
	bool close = false;
	MPI_Status st;
	MPI_Request reqCalc, reqChange;
	MPI_Comm Comm = currentComm;
	int flagChange = false, flagCalc = false;
	int cond, message;
	// Get message from own rank
	MPI_Irecv(&message, 1, MPI_INT, rank, 1997, Comm, &reqChange);	
	int countOfProcess, newSize = size;	
	while (!close) {
		MPI_Irecv(&cond, 1, MPI_INT, rank, 1999, Comm, &reqCalc);
		while (!flagChange || !flagCalc) {
			MPI_Test(&reqChange, &flagChange, &st);
			MPI_Test(&reqCalc, &flagCalc, &st);
		}
		if (flagChange) {
			ChangeCommunicator(Comm, newSize);
			MPI_Irecv(&message, 1, MPI_INT, rank, 1997, Comm, &reqChange);
			flagChange = false;
		}
		if (flagCalc){
			if (cond == 1) {
				ExecuteOwnTasks();
				countOfProcess = newSize;
				// If new ranks comes, their queue is empty	
				int sign = 1, id, k = 0;
				bool retry = false;
				// Task request from another ranks
				for (int i = 0; i < countOfProcess - 1; i++) {
					// If request from next rank
					if (!retry)	id = GetRank(sign, k, countOfProcess);
					MPI_Test(&reqChange, &flagChange, &st);
					if (flagChange) {
						ChangeCommunicator(Comm, newSize);
						MPI_Irecv(&message, 1, MPI_INT, rank, 1997, Comm, &reqChange);
						flagChange = false;
					}
					ExecuteOtherTask(Comm, id, retry);
					if (retry) i--;
				}
				MPI_Send(&cond, 1, MPI_INT, rank, 1999, Comm);
			}
			else if (cond == -1) close = true;
			flagCalc = false;
		}
	}
	return 0;
}
void StartWork() {
	MPI_Status st;
	MPI_Request req;
	int cond = 1;
	for (int i = 0; i < countOfWorkers; i++)
		MPI_Isend(&cond, 1, MPI_INT, rank, 1999, currentComm, &req);
	
	int count = 0, countOfConnectedWorkers = 0;
	bool connection = false;
	while (count < countOfWorkers || connection) {
		MPI_Recv(&cond, 1, MPI_INT, rank, 1999, currentComm, &st);
		if (cond == 2) {
			countOfConnectedWorkers = 0;
			connection = true;
			for (int i = 0; i < countOfWorkers; i++)
				MPI_Isend(&cond, 1, MPI_INT, rank_old, 1997, currentComm, &req);
			MPI_Comm_dup(newComm, &serverComm);
			MPI_Comm_dup(newComm, &reduceComm);
		}
		else if (count == 3) {
			countOfConnectedWorkers++;
			if (countOfConnectedWorkers == size_old * countOfWorkers) {
				changeExist = true;
				cond = 4;				
				// Send message to close old dispatcher
				MPI_Send(&cond, 1, MPI_INT, rank, 2001, currentComm);
				currentComm = newComm;
				// Send message to clients about changed communicator
				if (rank == 0) {
					for (int k = size_old; k < size; k++)
						MPI_Send(&cond, 1, MPI_INT, k, 10003, currentComm);
				}
				// Send message to server about changed communicator
				changeComm = false;
				//MPI_Isend(&cond, 1, MPI_INT, rank, 1998, currentComm, &req);
				connection = false;
			}
		}
		else count++;
	}
	// Clear the memory
	while (!sendedTasks.empty()) {
		ITask *t = sendedTasks.front();
		t->Clear();
		sendedTasks.pop();
	}
	fprintf(stderr, "%d:: work has done\n", rank);
}
