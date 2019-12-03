#include "lib_worker.cpp"
/*void DeleteSendedTask(int taskNumber) {
	
	fprintf(stderr, "%d:: start delete task %d.\n", rank, taskNumber);
	ITask *t = sendedTasks[taskNumber];
	t->Clear();
	pthread_mutex_lock(&mutex_send_task);
		sendedTasks.erase(taskNumber);
		sendedTasksCounter.erase(taskNumber);
		sendedTasksSuccessfullyRecv.erase(taskNumber);	
	pthread_mutex_unlock(&mutex_send_task);
	/*if (sendedTasks.empty() && currentTasks.empty()) {
		MPI_Request s;
		int cond = 1;
		MPI_Isend(&cond, 1, MPI_INT, rank, 40000, currentComm, &s); //Comm??
	}*/
	/*fprintf(stderr, "%d:: finish delete task %d.\n", rank, taskNumber);
}*/

void SendTask(MPI_Status &st, MPI_Comm &CommWorker, MPI_Comm &CommMap){
	// reciever rank
	int peer = st.MPI_SOURCE;
	int send = 0;
	ITask *t;
	MPI_Request s;
	// Try get task
	if (GetTask(&t)) {
		send = 1;
		// Send the message about task existing 
		MPI_Isend(&send, 1, MPI_INT, peer, 2002, CommWorker, &s);
		
		int taskNumber = t->blockNumber;
		pthread_mutex_lock(&mutex_send_task);
			sendedTasks.insert({taskNumber, t}); 
			sendedTasksCounter.insert({taskNumber, size_new - 1});	
		pthread_mutex_unlock(&mutex_send_task);
		
		t->GenerateSend(peer, CommWorker);
		
		int to_map_message[2] = { taskNumber, peer };
		// Send the future task place to all ranks 	
		for (int j = 0; j < size_new; j++) {
			if (j != rank) {
				//MPI_Isend(&to_map_message, 2, MPI_INT, j, 1030, CommMap, &s);
				MPI_Send(&to_map_message, 2, MPI_INT, j, 1030, CommMap);
			}
			else map[taskNumber] = peer;
		}
		
	}	// Send the message about task failure
	else MPI_Isend(&send, 1, MPI_INT, peer, 2002, CommWorker, &s);
}

// Dispatcher for work in old communicator (only tasks sending) 
void* dispatcher_old(void* me) {
	fprintf(stderr, "%d:: dispetcher_old run\n", rank);	
	MPI_Request req;
	MPI_Comm oldComm_ = currentComm, newComm_ = newComm;
	ITask *t;
	int cond = 2;
	bool close = false;
	while (!close) {
		MPI_Status st;
		MPI_Recv(&cond, 1, MPI_INT, MPI_ANY_SOURCE, 2001, oldComm_, &st);
		// Task request
		if (cond == 0) SendTask(st, currentComm, newComm_);
		else if (cond == 4) { close = true;	}
	}		
	
	fprintf(stderr, "%d:: old dispatcher is closed.\n", rank);
	return 0;
}

// Dispatcher
void* dispatcher(void* me) {
	size_new = size;
	fprintf(stderr, "%d:: dispatcher run.\n", rank);
	MPI_Comm Comm = currentComm;
	ITask *t;
	int cond;
	bool close = false;
	while (!close) {
		MPI_Status st;
		// Get message from any ranks
		MPI_Recv(&cond, 1, MPI_INT, MPI_ANY_SOURCE, 2001, Comm, &st);
		// Task request
		if (cond == 0) SendTask(st, Comm, Comm);
		// Communicator is changing
		else if (cond == 1) {
			cond = -10;
			int to_map_message[2] = { cond, cond };
			MPI_Barrier(currentComm);
			// Message to mapController about communicator changing
			MPI_Send(&to_map_message, 2, MPI_INT, rank, 1030, currentComm);
			// Communicators should be changed in single time because of map control
			MPI_Barrier(currentComm);
			rank_old = rank;
			size_old = size;
			MPI_Request req;
			MPI_Comm oldComm_ = currentComm;
			Comm = newComm;
			MPI_Comm_rank(Comm, &rank);
			MPI_Comm_size(Comm, &size_new);
			// Sending current places of tasks to new ranks
			if (rank == 0) {
				int sizeOfMap = map.size();
				for (int k = size_old; k < size_new; k++) {
					MPI_Send(&sizeOfMap, 1, MPI_INT, k, 10000, newComm);
					MPI_Send(map.data(), map.size(), MPI_INT, k, 10001, newComm);
				}
			}
			MPI_Barrier(currentComm);
			pthread_attr_t attrs;
			if (0 != pthread_attr_init(&attrs)) {
				perror("Cannot initialize attributes");
				abort();
			};
			if (0 != pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_DETACHED)) {
				perror("Error in setting attributes");
				abort();
			}
			// Create dispatcher which is working in old communicator
			if (0 != pthread_create(&thrs[countOfWorkers + 3], &attrs, dispatcher_old, &ids[countOfWorkers + 3])) {
				perror("Cannot create a thread");
				abort();
			}
			
			// Create map controller which is working in old communicator
			if (0 != pthread_create(&thrs[countOfWorkers + 4], &attrs, oldMapController, &ids[countOfWorkers + 4])) {
				perror("Cannot create a thread");
				abort();
			}
			
			cond = 2;
			MPI_Send(&cond, 1, MPI_INT, rank_old, 1999, oldComm_);
			fprintf(stderr, "%d:: new dispatcher run.\n", rank);

		} // Close dispatcher 
		else if (cond == -1) close = true;
		
	}
	fprintf(stderr, "%d:: dispatcher is closed.\n", rank);
	return 0;
}
