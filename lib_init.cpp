#include "library.h"

void CreateLibraryComponents() {	
	MPI_Comm_dup(currentComm, &serverComm);
	MPI_Comm_dup(currentComm, &reduceComm);
	MPI_Comm_dup(currentComm, &barrierComm);
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
	if (0 != pthread_create(&thrs[countOfWorkers + 2], &attrs_server, server, &ids[countOfWorkers + 2])) {
		perror("Cannot create a thread");
		abort();
	}
	// Create computational treads
	for (int i = 0; i < countOfWorkers; i++)
		if (0 != pthread_create(&thrs[i], &attrs_workers, worker, &ids[i])) {
			perror("Cannot create a thread");
			abort();
		}
}

void LibraryInitialize(int argc, char **argv, bool clientProgram) {
	int provided = MPI_THREAD_SINGLE;
	MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
	if (provided != MPI_THREAD_MULTIPLE) {
		std::cerr << "not MPI_THREAD_MULTIPLE";
		exit(0);
	}
	MPI_Comm_rank(currentComm, &rank);
	MPI_Comm_size(currentComm, &size);
	flags.resize(size);
	globalFlags.resize(size);
	pthread_mutexattr_init(&attr_get_task);
	pthread_mutex_init(&mutex_get_task, &attr_get_task);	
	pthread_mutexattr_init(&attr_set_task);
	pthread_mutex_init(&mutex_set_task, &attr_set_task);

	pthread_mutex_init(&server_mutexcond, NULL);
	pthread_mutex_init(&comunicator_mutexcond, NULL);
	pthread_cond_init(&server_cond, NULL);
	pthread_cond_init(&comunicator_cond, NULL);

	if (0 != pthread_attr_init(&attrs_workers)) {
		perror("Cannot initialize attributes");
		abort();
	};
	if (0 != pthread_attr_setdetachstate(&attrs_workers, PTHREAD_CREATE_DETACHED)) {
		perror("Error in setting attributes");
		abort();
	}
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
	if (clientProgram) {
		MPI_Comm server;
		MPI_Status st;
		double buf[MAX_DATA];

		char port_name[MPI_MAX_PORT_NAME];
		std::ifstream fPort("port_name.txt");
		for (int i = 0; i < MPI_MAX_PORT_NAME; i++)
		fPort >> port_name[i];
		fPort.close();
		fprintf(stderr, "%d::port exist\n", rank);

		MPI_Comm_connect(port_name, MPI_INFO_NULL, 0, currentComm, &server);
		MPI_Intercomm_merge(server, true, &currentComm);

		fprintf(stderr, "%d::connect to server success\n", rank);

		rank_old = rank;
		MPI_Comm_rank(currentComm, &rank);
		MPI_Comm_size(currentComm, &size);

		fprintf(stderr, "%d:: new rank = %d, new_size = %d\n", rank_old, rank, size);

		int sizeOfMap;
		MPI_Recv(&numberOfConnection, 1, MPI_INT, 0, 10002, currentComm, &st);
		
		MPI_Recv(&sizeOfMap, 1, MPI_INT, 0, 10000, currentComm, &st);
		fprintf(stderr, "%d:: sizeOfMap = %d\n", rank, sizeOfMap);
		if (sizeOfMap) {
			map.resize(sizeOfMap);
			MPI_Recv(map.data(), sizeOfMap, MPI_INT, 0, 10001, currentComm, &st);
			for (int i = 0; i < map.size(); i++)
				printf("%d; ", map[i]);
			CreateLibraryComponents();
			// All server's ranks change their comunicators
			MPI_Recv(&sizeOfMap, 1, MPI_INT, 0, 10003, currentComm, &st);
		}
	}
	else CreateLibraryComponents();
}

void CloseLibraryComponents() {
	MPI_Status st;
	MPI_Request s;
	int exit = -1;
	// Close dispatcher
	MPI_Isend(&exit, 1, MPI_INT, rank, 2001, currentComm, &s);
	pthread_join(thrs[countOfWorkers], NULL);
	fprintf(stderr, "%d::dispetcher close\n", rank);
	// Flag for old dispatcher work
	startWork = true;	
	// Close old dispatcher
	pthread_join(thrs[countOfWorkers + 3], NULL);
	while (numberOfConnection < countOfConnect) {
		int cond, size_new;
		MPI_Recv(&cond, 1, MPI_INT, rank, 2001, currentComm, &st);
		if (rank == 0) {
			size_old = size;
			MPI_Comm_size(newComm, &size_new);
			cond = 0;
			for (int k = size_old; k < size_new; k++)
				MPI_Send(&cond, 1, MPI_INT, k, 10000, newComm);
		}
		cond = 1;
		MPI_Isend(&cond, 1, MPI_INT, rank, 1998, currentComm, &s);
	}
	// Close server
	pthread_join(thrs[countOfWorkers + 2], NULL);
	fprintf(stderr, "%d::server close\n", rank);
	// Close map controller
	MPI_Isend(&exit, 1, MPI_INT, rank, 1030, currentComm, &s);	
	pthread_join(thrs[countOfWorkers + 1], NULL);
	fprintf(stderr, "%d::map controller close\n", rank);
	// Close workers
	for (int i = 0; i < countOfWorkers; i++)
		MPI_Isend(&exit, 1, MPI_INT, rank, 1999, currentComm, &s);
	for (int i = 0; i < countOfWorkers; i++)
		pthread_join(thrs[i], NULL);

	pthread_attr_destroy(&attrs_dispatcher);
	pthread_attr_destroy(&attrs_server);
	pthread_attr_destroy(&attrs_mapController);
	pthread_attr_destroy(&attrs_workers);
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
