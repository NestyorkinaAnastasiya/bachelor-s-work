#include "library.h"
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
	if (exitTask) {
		ITask *t = new Task;
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
	int message = 1;
	MPI_Request req;
	for (int i = 0; i < newSize; i++)
		MPI_ISend(&message, 1, MPI_INT, i, 2001, Comm, &req);
	Comm = newComm;
	newSize = size;
}
// Computational thread
void* worker(void* me) {
	bool close = false;
	MPI_Status st;
	MPI_Request reqCalc, reqChange;
	MPI_Comm Comm;
	bool flagChange = false, flagCalc = false;
	MPI_IRecv(&message, 1, MPI_INT, rank, 1997, Comm, &reqChange);
	int cond;
	// Get message from own rank
	int countOfProcess, newSize = size;
	int message;
	Comm = currentComm;
	while (!close) {
		MPI_IRecv(&cond, 1, MPI_INT, rank, 1999, Comm, &st, &reqCalc);
		while (!flagChange || !flagCalc) {
			MPI_Test(&reqChange, &flagChange, &st);
			MPI_Test(&reqCalc, &flagCalc, &st);
		}
		if (flagChange) {
			ChangeCommunicator(Comm, newSize);
			MPI_IRecv(&message, 1, MPI_INT, rank, 1997, Comm, &reqChange);
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
					MPI_Test(&req, &flagChange, &st);
					if (flagChange) {
						ChangeCommunicator(Comm, newSize);
						MPI_IRecv(&message, 1, MPI_INT, rank, 1997, Comm, &reqChange);
						flag = false;
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
	startWork = true;
	int cond = 1;
	for (int i = 0; i < countOfWorkers; i++)
		MPI_ISend(&cond, 1, MPI_INT, rank, 1999, Comm, &req);
	for (int i = 0; i < countOfWorkers; i++)
		MPI_Recv(&cond, 1, MPI_INT, rank, 1999, Comm);
	
	flags[rank] = changeComm;
	// If any rank changes communicator
	// Как поступать с клиентом..
	// Должно быть послано сообщение с инфой о том, что коммуникатор изменился
	// Подсоединение к вычислениям после изменения коммуникатора....
	MPI_Allreduce(flags.data(), globalFlags.data(), globalFlags.size(), MPI_INT, MPI_SUM, reduceComm);

	// Clear the memory
	while (!sendedTasks.empty()) {
		Task *t = sendedTasks.front();
		t->Clear();
		sendedTasks.pop();
	}
	bool change = false;
	for (int i = 0; i < globalFlags.size() && !charge; i++)
		if (globalFlags[i]) change = true;
	if (change){
		MPI_Recv(&cond, 1, MPI_INT, rank, 1999, newComm);		
		flags.resize(size);
		globalFlags.resize(size);	
	}
	startWork = false;
	fprintf(stderr, "%d:: work has done\n", rank);
}