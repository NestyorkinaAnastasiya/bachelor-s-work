#include "lib_server.cpp"

void CreateLibraryComponents() {
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
	// Create computational treads
	for (int i = 0; i < countOfWorkers; i++)
		if (0 != pthread_create(&thrs[i], &attrs_workers, worker, &ids[i])) {
			perror("Cannot create a thread");
			abort();
		}
	// Create server
	if (0 != pthread_create(&thrs[countOfWorkers + 2], &attrs_server, server, &ids[countOfWorkers + 2])) {
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
	if (rank == 0) fprintf(stderr, "%d:: start init.....\n", rank, iteration);
	MPI_Comm_size(currentComm, &size);
	size_old = size;
	pthread_mutexattr_init(&attr_get_task);
	pthread_mutex_init(&mutex_get_task, &attr_get_task);	
	pthread_mutexattr_init(&attr_set_task);
	pthread_mutex_init(&mutex_set_task, &attr_set_task);

	pthread_mutex_init(&server_mutexcond, NULL);
	pthread_mutex_init(&comunicator_mutexcond, NULL);
	pthread_cond_init(&server_cond, NULL);
	pthread_cond_init(&comunicator_cond, NULL);
	newComm = currentComm;
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
	if (rank == 0) fprintf(stderr, "%d:: finish init.....\n", rank);
	if (clientProgram) {
		MPI_Comm server;
		MPI_Status st;
		double buf[MAX_DATA];

		char port_name[MPI_MAX_PORT_NAME];
		std::ifstream fPort("port_name.txt");
		for (int i = 0; i < MPI_MAX_PORT_NAME; i++)
		fPort >> port_name[i];
		fPort.close();
		fprintf(stderr, "%d:: port exist\n", rank);

		MPI_Comm_connect(port_name, MPI_INFO_NULL, 0, currentComm, &server);
		MPI_Intercomm_merge(server, true, &currentComm);

		fprintf(stderr, "%d:: connect to server success\n", rank);

		MPI_Comm_rank(currentComm, &rank);
		MPI_Comm_size(currentComm, &size);
		rank_old = rank;
		size_old = size;

		fprintf(stderr, "%d:: new rank = %d, new_size = %d\n", rank_old, rank, size);

		int sizeOfMap;
		MPI_Recv(&numberOfConnection, 1, MPI_INT, 0, 10002, currentComm, &st);
		fprintf(stderr, "%d:: numberOfConnection = %d\n", rank, numberOfConnection);
		MPI_Recv(&sizeOfMap, 1, MPI_INT, 0, 10000, currentComm, &st);
		fprintf(stderr, "%d:: sizeOfMap = %d\n", rank, sizeOfMap);
		if (sizeOfMap) {
			map.resize(sizeOfMap);
			MPI_Recv(map.data(), sizeOfMap, MPI_INT, 0, 10001, currentComm, &st);
		/*	for (int i = 0; i < map.size(); i++)
				printf("%d; ", map[i]);
			fprintf(stderr, "\n");*/
			MPI_Recv(&condition, 1, MPI_INT, 0, 30000, newComm, &st);
			fprintf(stderr, "%d:: condition = %d.\n", rank, condition);
			MPI_Comm_dup(currentComm, &serverComm);
			MPI_Comm_dup(currentComm, &reduceComm);			
			//MPI_Comm_dup(currentComm, &barrierComm);
			// All server's ranks change their comunicators
			//MPI_Recv(&sizeOfMap, 1, MPI_INT, 0, 10003, currentComm, &st);
			fprintf(stderr, "%d:: create library components....\n", rank);
			CreateLibraryComponents();
			fprintf(stderr, "%d:: finish creating library components....\n", rank);
		}
	}	
	else {
		fprintf(stderr, "%d:: create library components....\n", rank);
		MPI_Comm_dup(currentComm, &serverComm);
		MPI_Comm_dup(currentComm, &reduceComm);
		CreateLibraryComponents();
		fprintf(stderr, "%d:: finish creating library components....\n", rank);
	}
}

void CloseLibraryComponents() {
	MPI_Status st;
	MPI_Request s;
	int exit = -1;
	// Close dispatcher
	MPI_Isend(&exit, 1, MPI_INT, rank, 2001, currentComm, &s);
	pthread_join(thrs[countOfWorkers], NULL);
	fprintf(stderr, "%d:: dispetcher close\n", rank);
	// Close old dispatcher
	pthread_join(thrs[countOfWorkers + 3], NULL);
	// Close workers
	for (int i = 0; i < countOfWorkers; i++)
		MPI_Isend(&exit, 1, MPI_INT, rank, 1999, currentComm, &s);
	for (int i = 0; i < countOfWorkers; i++)
		pthread_join(thrs[i], NULL);
	// Close map controller
	MPI_Isend(&exit, 1, MPI_INT, rank, 1030, currentComm, &s);	
	pthread_join(thrs[countOfWorkers + 1], NULL);
	fprintf(stderr, "%d::map controller close\n", rank);
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
		//cond = 1;
		MPI_Send(&cond, 1, MPI_INT, rank, 1998, currentComm);
	}
	// Close server
	pthread_join(thrs[countOfWorkers + 2], NULL);
	fprintf(stderr, "%d::server close\n", rank);
	pthread_attr_destroy(&attrs_dispatcher);
	pthread_attr_destroy(&attrs_server);
	pthread_attr_destroy(&attrs_mapController);
	pthread_attr_destroy(&attrs_workers);
}
