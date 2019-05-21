#include "library.h"
// Dispatcher for work in old communicator (only tasks sending) 
void* dispatcher_old(void* me) {
	fprintf(stderr, "%d::dispetcher_old run\n", rank);
	existOldDispatcher = true;
	MPI_Request req;
	ITask *t;
	int cond = 2;
	for (int i = 0; i < countOfWorkers; i++)
		MPI_ISend(&cond, 1, MPI_INT, rank_old, 1997, currentComm, &req);
	bool close = false;
	for (int i = 0; i < size_old * countOfWorkers && !close; ) {
		MPI_Status st;
		MPI_Recv(&cond, 1, MPI_INT, MPI_ANY_SOURCE, 2001, currentComm, &st);
		if (cond == 0) {
			int peer = st.MPI_SOURCE;
			int send = 0;
			if (GetTask(&t)) {
				send = 1;
				MPI_Send(&send, 1, MPI_INT, peer, 2002, currentComm);
				
				for (int j = 0; j < size; j++) {
					if (j != rank) {
						MPI_Send(&t->blockNumber, 1, MPI_INT, j, 1030, newComm);
						MPI_Send(&peer, 1, MPI_INT, j, 1031, newComm);
					}
					else map[t->blockNumber] = peer;
				}

				t->GenerateSend(peer, currentComm);
				sendedTasks.push(t);
			}
			else MPI_Send(&send, 1, MPI_INT, peer, 2002, currentComm);
		}
		else if (cond == 1) { i++; }
		else if (cond == 4) { close = true;	}
	}
	
	MPI_Comm_dup(newComm, &serverComm);
	MPI_Comm_dup(newComm, &reduceComm);
	// Flags should be changed in single time
	MPI_Barrier(currentComm);
	currentComm = newComm;
	flags.resize(size);
	globalFlags.resize(size);
	
	MPI_Barrier(reduceComm);
	if (close == true) cond = -1;
	else cond = 1;
	MPI_Isend(&cond, 1, MPI_INT, rank, 1999, currentComm, &req);
	MPI_Send(&cond, 1, MPI_INT, rank, 1998, currentComm);
	changeComm = false;
	fprintf(stderr, "%d:: dispetcher_old close\n", rank);
	existOldDispatcher = false;

	return 0;
}

// Dispatcher
void* dispatcher(void* me) {
	MPI_Comm Comm = currentComm;
	ITask *t;
	int cond;
	bool close = false;
	while (!close) {
		MPI_Status st;
		// Get message from any ranks
		MPI_Recv(&cond, 1, MPI_INT, MPI_ANY_SOURCE, 2001, Comm, &st);
		// Task request
		if (cond == 0) {
			// reciever rank
			int peer = st.MPI_SOURCE;
			int send = 0;
			// Try get task
			if (GetTask(&t)) {
				send = 1;
				// Send the message about task existing 
				MPI_Send(&send, 1, MPI_INT, peer, 2002, Comm);

				// Send the future task place to all ranks 
				for (int j = 0; j < size; j++) {
					if (j != rank) {
						MPI_Send(&t->blockNumber, 1, MPI_INT, j, 1030, Comm);
						MPI_Send(&peer, 1, MPI_INT, j, 1031, Comm);
					}
					else map[t->blockNumber] = peer;
				}
				t->GenerateSend(peer, Comm);
				sendedTasks.push(t);
			} // Send the message about task failure
			else MPI_Send(&send, 1, MPI_INT, peer, 2002, Comm);
		} // Communicator is changing
		else if (cond == 1) {
			// Message to mapController about communicator changing
			MPI_Send(&cond, 1, MPI_INT, rank, 1030, Comm);
			// Communicators should be changed in single time because of map control
			MPI_Barrier(currentComm);
			rank_old = rank;
			size_old = size;
			MPI_Request req;
			cond = -10;
			Comm = newComm;
			MPI_Comm_rank(Comm, &rank);
			MPI_Comm_size(Comm, &size);
			changeComm = true;	//SEND
			// Sending current places of tasks to new ranks
			if (rank == 0) {
				int sizeOfMap = map.size();
				for (int k = size_old; k < size; k++) {
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

		} // Close dispatcher 
		else if (cond == -1) close = true;
	}
	return 0;
}