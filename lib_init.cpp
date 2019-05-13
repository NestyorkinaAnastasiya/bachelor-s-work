#include "library.h"
void LibraryInitialize() {
	int provided = MPI_THREAD_SINGLE;
	MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
	if (provided != MPI_THREAD_MULTIPLE) {
		std::cerr << "not MPI_THREAD_MULTIPLE";
		exit(0);
	}
	MPI_Comm_rank(currentComm, &rank);
	MPI_Comm_size(currentComm, &size);

	pthread_mutexattr_init(&attr_get_task);
	pthread_mutex_init(&mutex_get_task, &attr_get_task);	
	pthread_mutexattr_init(&attr_set_task);
	pthread_mutex_init(&mutex_set_task, &attr_set_task);

	pthread_mutex_init(&server_mutexcond, NULL);
	pthread_mutex_init(&comunicator_mutexcond, NULL);
	pthread_cond_init(&server_cond, NULL);
	pthread_cond_init(&comunicator_cond, NULL);

	if (0 != pthread_attr_init(&attrs_dispatcher)) {
		perror("Cannot initialize attributes");
		abort();
	};
	if (0 != pthread_attr_setdetachstate(&attrs_dispatcher, PTHREAD_CREATE_DETACHED)) {
		perror("Error in setting attributes");
		abort();
	}
	if (0 != pthread_attr_init(&attrs_server)) {
		perror("Cannot initialize attributes");
		abort();
	};
	if (0 != pthread_attr_setdetachstate(&attrs_server, PTHREAD_CREATE_DETACHED)) {
		perror("Error in setting attributes");
		abort();
	}
	if (0 != pthread_attr_init(&attrs_mapController)) {
		perror("Cannot initialize attributes");
		abort();
	};
	if (0 != pthread_attr_setdetachstate(&attrs_mapController, PTHREAD_CREATE_DETACHED)) {
		perror("Error in setting attributes");
		abort();
	}
}

void CreateLibraryComponents()
{	
	// Create dispatcher
	if (0 != pthread_create(&thrs[countOfWorkers], &attrs_dispatcher, dispatcher, &ids[countOfWorkers])) {
		perror("Cannot create a thread");
		abort();
	}
	// Create mapController
	if (0 != pthread_create(&thrs[countOfWorkers + 1], &attrs_mapController, mapController, &ids[countOfWorkers + 1])) {
		perror("Cannot create a thread");
		abort();
	}
	// Create server
	if (!client) {
		MPI_Comm_dup(currentComm, &serverComm);
		MPI_Comm_dup(currentComm, &reduceComm);
		MPI_Comm_dup(currentComm, &barrierComm);
		if (0 != pthread_create(&thrs[countOfWorkers + 2], &attrs_server, server, &ids[countOfWorkers + 2])) {
			perror("Cannot create a thread");
			abort();
		}
	}
}
/*
void ChangeCommunicator() {
	if (!client) {
		bool change = false;
		flags[rank] = changeComm;
		// If any rank changes communicator
		MPI_Allreduce(flags.data(), globalFlags.data(), globalFlags.size(), MPI_INT, MPI_SUM, reduceComm);

		// Clear the memory
		while (!sendedTasks.empty()) {
			Task *t = sendedTasks.front();
			t->Clear();
			sendedTasks.pop();
		}

		for (int i = 0; i < globalFlags.size() && !change; i++)
			if (globalFlags[i]) change = true;

		// If communicator was changed, we need close the old communicator
		// ! need to pass this in thread because of impossibility a several connection per iteration !
		if (change) {
			int cond = 4;
			MPI_Request s;
			// Send the close message to old dispatcher
			MPI_ISend(&cond, 1, MPI_INT, rank, 2001, oldComm, &s);
			// Waiting for the closure of the old communicator
			MPI_Recv(&cond, 1, MPI_INT, rank, 2001, oldComm);
			// Waiting for the closure of the old communicator
			//int k = pthread_cond_wait(&comunicator_cond, &comunicator_mutexcond);
			if (rank == 0) {
				time(&rawtime);
				timeinfo = localtime(&rawtime);
				strftime(buffer, 80, "%H:%M:%S", timeinfo);
				puts(buffer);
				fTime << "new connection in " << buffer << "\n";
			}
		}
	}
	else {
		fprintf(stderr, "%d::start dup\n", rank);
		MPI_Comm_dup(currentComm, &serverComm);
		fprintf(stderr, "%d::dup server sucsess\n", rank);
		// Create server
		if (0 != pthread_create(&thrs[countOfWorkers + 2], &attrs_server, server, &ids[countOfWorkers + 2])) {
			perror("Cannot create a thread");
			abort();
		}
		MPI_Comm_dup(currentComm, &reduceComm);
		fprintf(stderr, "%d::dup reduce sucsess\n", rank);
		client = false;
	}
}

*/
// Вывод задачи :			pthread_mutex_lock(&mutex);
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
