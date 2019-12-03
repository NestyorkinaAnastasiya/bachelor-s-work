#include "lib_dispatchers.cpp"
void* mapController(void* me) {
	fprintf(stderr, "%d:: map controller run.\n", rank);
	MPI_Comm Comm = currentComm;
	MPI_Status st;
	MPI_Request s;
	bool close = false;
	int message[2];
	while (!close) {
		MPI_Recv(&message, 2, MPI_INT, MPI_ANY_SOURCE, 1030, Comm, &st);
		// Task place was changed	
		if (message[0] >= 0) {
			int peer = st.MPI_SOURCE;
			int map_id = message[0], rank_id = message[1];
			map[map_id] = rank_id;
			
			// Send the message about finished changes; map_id - the number of task
			int map_message[2] = { -2, map_id };			
			fprintf(stderr, "%d:: task %d pass to %d\n", rank, map_id, rank_id);
			MPI_Send(&map_message, 2, MPI_INT, peer, 1030, currentComm);
		} 
		else if (message[0] == -2) {			
			int taskNumber = message[1];			
			int peer = st.MPI_SOURCE;
			
			if (sendedTasksCounter[taskNumber] != 0) {
				sendedTasksCounter[taskNumber]--;
				fprintf(stderr, "%d:: change location in %d for task %d; counter was %d.\n", rank, peer, taskNumber, sendedTasksCounter[taskNumber]);
			}
			if (sendedTasksCounter[taskNumber] == 0) {
				pthread_mutex_lock(&mutex_send_task);		
					sendedTasksCounter.erase(taskNumber);
				pthread_mutex_unlock(&mutex_send_task);
				fprintf(stderr, "%d:: !!! map changed for task %d; sendedTaskCounter.size = %d.\n", rank, taskNumber, sendedTasksCounter.size());
			}				
		}
		// recv SendedTask
		else if (message[0] == -3) {
			int taskNumber = message[1];			
			int peer = st.MPI_SOURCE;
			fprintf(stderr, "%d:: recv task %d in %d!\n", rank, taskNumber, peer);	
			sendedTasks[taskNumber]->Clear();
			pthread_mutex_lock(&mutex_send_task);
				sendedTasks.erase(taskNumber);
			pthread_mutex_unlock(&mutex_send_task);			
			fprintf(stderr, "%d:: delete task %d.\n", rank, taskNumber);			
		}
		// Close mapController
		else if (message[0] == -1) close = true;
		// Communicator changing 
		else if (message[0] == -10) Comm = newComm;
	}
	fprintf(stderr, "%d:: map controller is closed.\n", rank);
	return 0;
}

void* oldMapController(void* me) {
	fprintf(stderr, "%d:: map controller run.\n", rank);
	MPI_Comm Comm = currentComm;
	MPI_Status st;
	MPI_Request s;
	bool close = false;
	int message[2];
	while (!close) {
		MPI_Recv(&message, 2, MPI_INT, MPI_ANY_SOURCE, 1030, Comm, &st);
		// Message from worker in old communicator
		if (message[0] == -3) {
			int taskNumber = message[1];			
			int peer = st.MPI_SOURCE;
			fprintf(stderr, "%d:: recv task %d in %d!\n", rank, taskNumber, peer);	
			sendedTasks[taskNumber]->Clear();
			pthread_mutex_lock(&mutex_send_task);
				sendedTasks.erase(taskNumber);
			pthread_mutex_unlock(&mutex_send_task);			
			fprintf(stderr, "%d:: delete task %d.\n", rank, taskNumber);			
		}
		// Message from mapController in old communicator
		else if (message[0] == -2) {			
			int taskNumber = message[1];			
			int peer = st.MPI_SOURCE;
			
			if (sendedTasksCounter[taskNumber] != 0) {
				sendedTasksCounter[taskNumber]--;
				fprintf(stderr, "%d:: change location in %d for task %d; counter was %d.\n", rank, peer, taskNumber, sendedTasksCounter[taskNumber]);
			}
			if (sendedTasksCounter[taskNumber] == 0) {
				pthread_mutex_lock(&mutex_send_task);		
					sendedTasksCounter.erase(taskNumber);
				pthread_mutex_unlock(&mutex_send_task);
				fprintf(stderr, "%d:: !!! map changed for task %d; sendedTaskCounter.size = %d.\n", rank, taskNumber, sendedTasksCounter.size());
			}				
		}
		// Close mapController
		else if (message[0] == -1) close = true;
	}
	fprintf(stderr, "%d:: map controller is closed.\n", rank);
	return 0;
}