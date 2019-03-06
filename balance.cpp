#include "task.cpp"
#include <time.h>
#include <chrono>
time_t rawtime; 
struct tm * timeinfo; 
char buffer [80];

#define MAX_DATA 1000
// id for threads
int ids[11] = { 0,1,2,3,4,5,6,7,8,9,10 };
// descriptors for threads
pthread_t thrs[11];
// Communicators
MPI_Comm currentComm = MPI_COMM_WORLD;
MPI_Comm newComm, serverComm, reduceComm, barrierComm;
int changeComm = false;
bool server_new = false;
int condition = 0;
int rank_old, size_old;
// Count of computational threads
int countOfWorkers = 1;
// Count of all threads
int countOfThreads = 3;
int numberOfConnection = 0;
bool STOP = false;
std::queue<Task*> currentTasks, queueRecv;

pthread_mutex_t mutex_get_task, mutex_set_task;

bool GetTask(Task **currTask) {	
	pthread_mutex_lock(&mutex_get_task);
	if (currentTasks.empty()) {
		pthread_mutex_unlock(&mutex_get_task);
		return false;
	}
	else {
		*currTask = currentTasks.front();
		currentTasks.pop();
	}
	pthread_mutex_unlock(&mutex_get_task);
	return true;
}

// Computational thread
void* worker(void* me) {	
	Task *currTask;
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
		if(!retry){
			if (sign == 1) {
				sign = -1;
				k++;
			}	
			else sign = 1;

			id = rank + sign*k;

			if (id > size - 1) id -= countOfProcess;
			else if (id < 0) id += countOfProcess;	
		}
		
		if(changeComm) Comm = newComm;

		// Send task request
		condition = 0;
		MPI_Send(&condition, 1, MPI_INT, id, 2001, Comm);
		MPI_Status st;
		// Get information about task existing
		MPI_Recv(&exitTask, 1, MPI_INT, id, 2002, Comm, &st);

		// If task exist, worker recieve and execute it
		if (exitTask) {
			Task *t = new Task;
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

// Dispatcher
void* dispatcher(void* me) {
	MPI_Comm Comm = currentComm;
	Task *t;
	int cond;
	bool close = false;
	while(!close) {
		MPI_Status st;
		// Get message from any ranks
		MPI_Recv(&cond, 1, MPI_INT, MPI_ANY_SOURCE, 2001, Comm , &st);
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
				GenerateSend(t, peer, Comm);
			} // Send the message about task failure
			else MPI_Send(&send, 1, MPI_INT, peer, 2002, Comm);
		} // Communicator is changing
		else if (cond == 1) {
			rank_old = rank;
			size_old = size;
			
			// Communicators should be changed in single time because of map control
			MPI_Barrier(currentComm);
			
			MPI_Request req;
			cond = -10;
			// Message to mapController about communicator changing
			MPI_Send(&cond, 1, MPI_INT, rank, 1030, Comm);			
            Comm = newComm;		
			MPI_Comm_rank(Comm, &rank);
            MPI_Comm_size(Comm, &size);
			changeComm = true;	//SEND
			// Sending current places of tasks to new ranks
			if (rank == 0) { 
				int sizeOfMap = map.size();
				for(int k = size_old; k < size; k++) {
					MPI_Send(&sizeOfMap, 1, MPI_INT, k, 10000, newComm);
					MPI_Send(map.data(), map.size(), MPI_INT, k, 10001, newComm);
				}
			}
			
			pthread_attr_t attrs;
			if (0 != pthread_attr_init(&attrs))
			{
				perror("Cannot initialize attributes");
				abort();
			};

			if (0 != pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_DETACHED))
			{
				perror("Error in setting attributes");
				abort();
			}

			// Create dispatcher which is working in old communicator
			if(0!=pthread_create(&thrs[countOfWorkers+3], &attrs, dispatcher_old, &ids[countOfWorkers+3]))
       			{
      		         	perror("Cannot create a thread");
              			abort();
      			}	

		} // Close dispatcher 
		else if (cond == -1) close = true;	
	}	
	return 0;
}

// Dispatcher for work in old communicator (only tasks sending) 
void* dispatcher_old(void* me) {
	fprintf (stderr,"%d::dispetcher_old run\n", rank);
	Task *t;
	int cond; 
	bool close = false;
	while(!close) {
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
				
				GenerateSend(t, peer, currentComm);
			} 
			else MPI_Send(&send, 1, MPI_INT, peer, 2002, currentComm);
		} 
		else if (cond == 4) close = true;
	}
	fprintf (stderr,"%d:: dispetcher_old close\n",rank);
	
	return 0;
}

void* mapController(void* me) {
	MPI_Comm Comm = currentComm;
	MPI_Status st;
	bool close = false;
	int map_id, rank_id;
	while (!close) {
		MPI_Recv(&map_id, 1, MPI_INT, MPI_ANY_SOURCE, 1030, Comm, &st);
		// Task place was changed	
		if (map_id >= 0) {
			int peer = st.MPI_SOURCE;
			MPI_Recv(&rank_id, 1, MPI_INT, peer, 1031, Comm, &st);
			map[map_id] = rank_id;
		}
		// Close mapController
		else if (map_id == -1) close = true;
		// Communicator changing 
		else if (map_id == -10) Comm = newComm;
	}
	return 0;
}

void* server(void *me)
{
	MPI_Comm client;
	MPI_Status status;
	char port_name[MPI_MAX_PORT_NAME];
	int old_size, new_size;	
	
	// Open port
	if (rank == 0)
	{
		MPI_Open_port(MPI_INFO_NULL, port_name);
		std::ofstream fPort("port_name.txt");
		for (int i = 0; i < MPI_MAX_PORT_NAME; i++)
			fPort << port_name[i];
		fPort.close();
	}
	for (; numberOfConnection < countOfConnect; )
	{
		// The previous connection must be finished
		while(server_new); //RECV
		old_size = size;
		
		// Waiting for new ranks
		MPI_Comm_accept(port_name, MPI_INFO_NULL, 0, serverComm, &client);
		// Creating new communicator for joint ranks group
		MPI_Intercomm_merge(client, false, &newComm);
              	server_new = true;
		MPI_Comm_size(newComm, &new_size);
		MPI_Request req;
		int message = 1;
		numberOfConnection++;
		// send to new ranks information about connections count
		if (rank == 0) 
			for(int k = old_size; k < new_size; k++) 
				MPI_Send(&numberOfConnection, 1, MPI_INT, k, 10002, newComm);
	
		// Send to dispatcher message about new communicator
		MPI_Send(&message, 1, MPI_INT, rank, 2001, currentComm);
	}
	return 0;
}

// Âûâîä çàäà÷è :			pthread_mutex_lock(&mutex);
				/*printf("%d:: block = %d, lock = %d, tpp = %d, f = %d",rank,t->blockNumber,t->localNumber, t->tasks_x, t->flag);
				printf("neighbors: %d %d %d %d %d %d\n", t->neighbors[0], t->neighbors[1],t->neighbors[2],t->neighbors[3],t->neighbors[4],t->neighbors[5]);
				
				printf("oldU:\n");
				for (int z = 0; z < t->oldU.size();z++)
					printf("oldU[%d] = %lf\n", z, t->oldU[z]);
				printf("newU:\n");
				for (int z = 0; z < t->newU.size();z++)
					printf("newU[%d] = %lf\n", z, t->newU[z]);
				printf("F:\n");
				for (int z = 0; z < t->F.size();z++)
					printf("F[%d] = %lf\n", z, t->F[z]);
							
				printf("POINTS:\n");
				for (int z = 0; z< t->points.size();z++)
					printf("p[%d]: x = %lf, y = %lf, z = %lf, glN = %d\n", z, t->points[z].x, t->points[z].y, t->points[z].z, t->points[z].globalNumber);
				printf("borders:\n");
				for (int k = 0; k<6; k++)
				{
					for (int z = 0; z< t->borders[k].size();z++)
						printf("b[%d][%d] = %lf\n", k, z, t->borders[k][z]);
					printf("\n");
				}
				printf("SHADOWborders:\n");
				for (int k =0; k<6; k++)
				{
					for (int z = 0; z< t->shadowBorders[k].size();z++)
						printf("sb[%d][%d] = %d\n", k, z, t->shadowBorders[k][z]);
					printf("\n");
				}
				printf("KU:\n");
				for (int z = 0; z< t->numbersOfKU.size();z++)
					printf("KU[%d] = %d\n", z, t->numbersOfKU[z]);
				printf("\n");*/

				//pthread_mutex_unlock(&mutex);
