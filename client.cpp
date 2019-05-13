#include "task.cpp"
client = true;
std::ofstream fTime;
void FindSolution() {
	MPI_Request s;
	MPI_Status st;
	int exit = -1;
	std::vector<int> flags(size);
	std::vector<int> globalFlags(size);
	std::stringstream ss;
	ss << rank;
	std::string nameFile = "Loading" + ss.str();
	nameFile += ".txt";
	std::ofstream fLoading(nameFile);
	CreateLibraryComponents();
	for (iteration = 0; iteration < maxiter && CheckConditions(); iteration++) {
		/* if (rank_old == 0)*/ printf("%d::  --------------------START ITERATION %d---------------------\n", rank, iteration);
		for (auto &i : newResult) i = 0;
		for (auto &i : oldResult) i = 0;

		while (!allTasks.empty()) {
			Task *t = allTasks.front();
			if (iteration != 0) t->ReceiveFromNeighbors(currentComm);
			queueRecv.push(t);
			allTasks.pop();
		}

		while (!queueRecv.empty()) {
			Task *t = queueRecv.front();
			if (iteration != 0) t->WaitBorders();
			pthread_mutex_lock(&mutex_get_task);
			currentTasks.push(t);
			pthread_mutex_unlock(&mutex_get_task);
			queueRecv.pop();
		}
		fprintf(stderr, "%d:: count of tasks = %d\n", rank, currentTasks.size());

		// Create computational treads
		for (int i = 0; i < countOfWorkers; i++)
			if (0 != pthread_create(&thrs[i], &attrs, worker, &ids[i])) {
				perror("Cannot create a thread");
				abort();
			}
		// Wait computational threads
		for (int i = 0; i < countOfWorkers; i++)
			if (0 != pthread_join(thrs[i], NULL)) {
				perror("Cannot join a thread");
				abort();
			}

		fprintf(stderr, "%d:: workers closed\n", rank);
		if (client)
			MPI_Recv(&iteration, 1, MPI_INT, 0, 10005, currentComm, &st);
		//ChangeCommunicator();
		GenerateResultOfIteration(reduceComm);

		while (!queueRecv.empty()) {
			Task *t = queueRecv.front();
			t->SendToNeighbors(currentComm);
			allTasks.push(t);
			queueRecv.pop();
		}
		fLoading << "iteration " << iteration << "::  " << allTasks.size() << "\ttasks\n";
		if (rank_old == 0) printf("%d:: --------------------FINISH ITERATION %d---------------------\n", rank, iteration);
	}

	MPI_Isend(&exit, 1, MPI_INT, rank, 1030, currentComm, &s);
	MPI_Isend(&exit, 1, MPI_INT, rank, 2001, currentComm, &s);
	pthread_join(thrs[countOfWorkers], NULL);
	fprintf(stderr, "%d::dispetcher close\n", rank);
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
		server_new = false;
	}
	pthread_join(thrs[countOfWorkers + 1], NULL);
	fprintf(stderr, "%d::mapController close\n", rank);
	pthread_join(thrs[countOfWorkers + 2], NULL);
	pthread_attr_destroy(&attrs);
	fLoading.close();
}

int main(int argc, char **argv)
{
	time(&rawtime);
	timeinfo = localtime(&rawtime);
	LibraryInitialize();
	if (rank == 0) {		
		strftime(buffer, 80, "%H:%M:%S", timeinfo);
		puts(buffer);
	}

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
	if (rank_old == 0) {
		std::stringstream ss;
		ss << numberOfConnection;
		std::string nameFile = "time_client" + ss.str();
		nameFile += ".txt";
		fTime.open(nameFile);
		fTime << "servers's processes start in " << buffer << "\n";
		fTime.close();
	}
	MPI_Recv(&sizeOfMap, 1, MPI_INT, 0, 10000, currentComm, &st);
	fprintf(stderr, "%d:: sizeOfMap = %d\n", rank, sizeOfMap);

	// If work exist
	if (sizeOfMap)
	{
		map.resize(sizeOfMap);
		MPI_Recv(map.data(), sizeOfMap, MPI_INT, 0, 10001, currentComm, &st);
		for (int i = 0; i < map.size(); i++)
			printf("%d; ", map[i]);
		GenerateBasicConcepts();
		FindSolution();

		GenerateResult(currentComm);
	}

	MPI_Finalize();
	return 0;
}
