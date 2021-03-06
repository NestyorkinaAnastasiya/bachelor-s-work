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
	MPI_Request s;
	// Get information about task existing
	MPI_Recv(&existTask, 1, MPI_INT, id, 2002, Comm, &st);

	// If task exist, worker recieve and execute it
	if (existTask) {
		Task *t = new Task();		
		t->GenerateRecv(id, Comm);		
		
		pthread_mutex_lock(&mutex_set_task);
		queueRecv.push(t);
		pthread_mutex_unlock(&mutex_set_task);
		
		int to_map_message[2] = { -3, t->blockNumber };
		MPI_Isend(&to_map_message, 2, MPI_INT, id, 1030, Comm, &s);		
		
		t->Run();
		fprintf(stderr, "%d:: worker executed %d task.\n", rank, t->blockNumber );
		// This rank can have new task for work
		retry = true; 
		MPI_Wait(&s, &st);
		return;
		
	}
	else retry = false;
	fprintf(stderr, "%d:: rank %d haven't tasks.\n", rank, id );
}
void ChangeCommunicator(MPI_Comm &Comm, int &newSize) {
	fprintf(stderr, "%d:: worker is changing communicator.\n", rank);
	int message = 3;
	MPI_Request req;
	// The message about finished changing of communicator
	fprintf(stderr, "%d:: worker send message to %d ranks.\n", rank, newSize);
	for (int i = 0; i < newSize; i++)
		MPI_Send(&message, 1, MPI_INT, i, 1999, Comm);
	Comm = newComm;
	newSize = size_new;
	fprintf(stderr, "%d:: worker finished changing communicator.\n", rank);
}
// Computational thread
void* worker(void* me) {
	fprintf(stderr, "%d:: worker run.\n", rank);
	bool close = false;
	MPI_Status st;
	MPI_Request reqCalc, reqChange;
	MPI_Comm Comm = currentComm;
	int flagChange = 0, flagCalc = 0;
	int cond, message;
	// Get message from own rank
	MPI_Irecv(&message, 1, MPI_INT, rank, 1997, Comm, &reqChange);	
	MPI_Irecv(&cond, 1, MPI_INT, rank, 1996, Comm, &reqCalc);
	int countOfProcess, newSize = size;	
	while (!close) {
		MPI_Test(&reqChange, &flagChange, &st);
		MPI_Test(&reqCalc, &flagCalc, &st);
		if (flagChange != 0) {
			ChangeCommunicator(Comm, newSize);
			MPI_Irecv(&message, 1, MPI_INT, rank, 1997, Comm, &reqChange);
			MPI_Irecv(&cond, 1, MPI_INT, rank, 1996, Comm, &reqCalc);
			flagChange = false;
		}
		if (flagCalc != 0){
			if (cond == 1) {
				fprintf(stderr, "%d:: worker is executing own tasks.\n", rank);
				ExecuteOwnTasks();
				countOfProcess = newSize;
				// If new ranks comes, their queue is empty	
				int sign = 1, id, k = 0;
				bool retry = false;
				// Task request from another ranks
				fprintf(stderr, "%d:: worker is executing another tasks.\n", rank);
				id = GetRank(sign, k, countOfProcess);
				for (int i = 0; i < countOfProcess - 1;) {
					MPI_Test(&reqChange, &flagChange, &st);
					if (flagChange) {
						ChangeCommunicator(Comm, newSize);
						MPI_Irecv(&message, 1, MPI_INT, rank, 1997, Comm, &reqChange);
						flagChange = false;
					}
					
					fprintf(stderr, "%d:: worker try execute another tasks from %d rank.\n", rank, id);
					ExecuteOtherTask(Comm, id, retry);
					if (!retry) { 
						id = GetRank(sign, k, countOfProcess);
						i++;
					}
				}
				MPI_Send(&cond, 1, MPI_INT, rank, 1999, Comm);
				fprintf(stderr, "%d:: worker finished job.\n", rank);
				MPI_Irecv(&cond, 1, MPI_INT, rank, 1996, Comm, &reqCalc);
				flagCalc = 0;
			}
			else if (cond == -1) close = true;
		}
	}
	fprintf(stderr, "%d:: worker is closed.\n", rank);
	return 0;
}

void ChangeMainCommunicator() {
	changeExist = true;
	MPI_Request req;
	int cond = 4;
	// Send message to close old dispatcher
	MPI_Send(&cond, 1, MPI_INT, rank, 2001, currentComm);
	cond = -1;
	int to_map_message[2] = { cond, cond };
	// Close old mapController 
	MPI_Send(&to_map_message, 2, MPI_INT, rank, 1030, currentComm);
	currentComm = newComm;
	// Send message to server about changed communicator
	MPI_Isend(&cond, 1, MPI_INT, rank, 1998, oldComm, &req);
	fprintf(stderr, "%d:: connection is done.\n", rank);
}

void StartWork(bool clientProgram) {
	MPI_Status st;
	MPI_Request req;
	bool barrier = false;
	int cond = 1, message = 1;
	int count = 0, countOfConnectedWorkers = 0;
	bool connection = false;
	MPI_Barrier(barrierComm);
	
	std::vector<int> flags(size);
	std::vector<int> globalFlags(size);
	if (!clientProgram || condition) {
		for (int i = 0; i < countOfWorkers; i++)
			MPI_Send(&message, 1, MPI_INT, rank, 1996, currentComm);
		while (count < countOfWorkers || connection) {
			//fprintf(stderr, "%d:: waiting for message...\n", rank);
			MPI_Recv(&cond, 1, MPI_INT, MPI_ANY_SOURCE, 1999, currentComm, &st);
			fprintf(stderr, "%d:: get condition %d.\n", rank, cond);
			if (cond == 2) {
				// Send message to dispatcher about connection continue
				fprintf(stderr, "%d:: connection....\n", rank);
				connection = true;
				condition = 2;
				flags[rank] = condition;
				MPI_Allreduce(flags.data(), globalFlags.data(), globalFlags.size(), MPI_INT, MPI_SUM, reduceComm);
				for (int i = 0; i < globalFlags.size(); i++)
					if (globalFlags[i] == 0) { barrier = true; condition = 0;}
				// Send the message about calculation condition
				if (rank == 0) {
					fprintf(stderr, "%d:: send condition to client size_old = %d, size = %d.\n", rank, size_old, size_new);
					for (int k = size_old; k < size_new; k++)
						MPI_Send(&condition, 1, MPI_INT, k, 30000, newComm);
				}
				MPI_Comm_dup(newComm, &serverComm);
				MPI_Comm_dup(newComm, &reduceComm);
				MPI_Comm_dup(newComm, &barrierComm);
				flags.resize(size_new); globalFlags.resize(size_new);
				for (int i = 0; i < countOfWorkers; i++)
					MPI_Send(&cond, 1, MPI_INT, rank_old, 1997, currentComm);
			}
			else if (cond == 3) {
				countOfConnectedWorkers++;
				fprintf(stderr, "%d:: %d connected workers. sizeOld = %d\n", rank, countOfConnectedWorkers, size_old);
				if (countOfConnectedWorkers == size_old * countOfWorkers) {
					ChangeMainCommunicator();
					connection = false;
					countOfConnectedWorkers = 0;
					fprintf(stderr, "%d:: tutaaaa", rank);
					MPI_Barrier(barrierComm);
					size = size_new;
				}
			}
			else if (cond == 1) count++;
		}
	
		if (!barrier) {
			condition = 0;
			// Exchange of condition
			flags[rank] = condition;
			MPI_Allreduce(flags.data(), globalFlags.data(), globalFlags.size(), MPI_INT, MPI_SUM, reduceComm);
			for (int i = 0; i < globalFlags.size() && !barrier; i++)
				if (globalFlags[i] == 2) barrier = true;
			if (barrier) {
				// Send the message about calculation condition
				if (rank == 0) {
					fprintf(stderr, "%d:: send condition to client size_old = %d, size = %d.\n", rank, size_old, size_new);
					for (int k = size_old; k < size_new; k++)
						MPI_Send(&condition, 1, MPI_INT, k, 30000, newComm);
				}
				MPI_Comm_dup(newComm, &serverComm);
				MPI_Comm_dup(newComm, &reduceComm);
				MPI_Comm_dup(newComm, &barrierComm);
				for (int i = 0; i < countOfWorkers; i++)
					MPI_Send(&cond, 1, MPI_INT, rank_old, 1997, currentComm);
				connection = true;
				while (connection) {
					MPI_Recv(&cond, 1, MPI_INT, MPI_ANY_SOURCE, 1999, currentComm, &st);
					if (cond == 3) {
						countOfConnectedWorkers++;
						fprintf(stderr, "%d:: %d connected workers after calculations. sizeOld = %d\n", rank, countOfConnectedWorkers, size_old);
						if (countOfConnectedWorkers == size_old * countOfWorkers) {
							ChangeMainCommunicator();
							connection = false;
							countOfConnectedWorkers = 0;		
							fprintf(stderr, "%d:: zdesyaaaaaaa", rank);							
							MPI_Barrier(barrierComm);
							size = size_new;
						}
					}
				}
			}
		}
		fprintf(stderr, "%d:: sended tasks count = %d\n", rank, sendedTasksCounter.size());
	
		while (!sendedTasksCounter.empty());
	}
	fprintf(stderr, "%d:: barrier after calculations\n", rank);
	MPI_Barrier(barrierComm);
}
