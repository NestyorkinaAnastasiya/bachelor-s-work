#include "task.cpp"
std::ofstream fTime;
void FindSolution() {	
	MPI_Status st;
	
	std::stringstream ss;
	ss << rank;
	std::string nameFile = "Loading" + ss.str();
	nameFile += ".txt";
	std::ofstream fLoading(nameFile);

	CreateLibraryComponents();

	for (iteration = 0; iteration < maxiter && CheckConditions(); iteration++) {
		if (rank == 0) printf("%d::  --------------------START ITERATION %d---------------------\n", rank, iteration);
		for (auto &i : newResult) i = 0;
		for (auto &i : oldResult) i = 0;
		auto t_start = std::chrono::high_resolution_clock::now();
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
		
		StartWork();

		/*тут было оно*/
		if (changeExist)
		{
			changeExist = false;
			if (rank == 0)
				for (int k = size_old; k < size; k++)
					MPI_Send(&iteration, 1, MPI_INT, k, 10005, newComm);
		}
		ChangeCommunicator();
		fprintf(stderr, "%d::get to generate result of iteration\n", rank);
		GenerateResultOfIteration(reduceComm);

		while (!queueRecv.empty()) {
			Task *t = queueRecv.front();
			t->SendToNeighbors(currentComm);
			queueRecv.pop();
			allTasks.push(t);
		}
		auto t_end = std::chrono::high_resolution_clock::now();
		if (rank == 0) {
			printf("%d:: res = %e\n", rank, residual);
			printf("%d:: --------------------FINISH ITERATION %d---------------------\n", rank, iteration);

		}
		if (rank == 0) {
			fTime << "iteration " << iteration << "::  " << std::chrono::duration<double, std::milli>(t_end - t_start).count() << " ms\n";
		}
		fLoading << "iteration " << iteration << "::  " << allTasks.size() << "\ttasks\n";
	}	
	CloseLibraryComponents();
	fLoading.close();
}

int main(int argc, char **argv) {
	fTime.open("time_server.txt");
	time(&rawtime);
	timeinfo = localtime(&rawtime);
	strftime(buffer, 80, "%H:%M:%S", timeinfo);
	puts(buffer);
	LibraryInitialize();
	if (rank == 0) 	fTime << "servers's processes start in " << buffer << "\n";
	
	GenerateBasicConcepts();
	GenerateQueueOfTask();
	std::vector<int> tmp(map.size());
	MPI_Allreduce(map.data(), tmp.data(), map.size(), MPI_INT, MPI_SUM, currentComm);
	map = tmp;

	FindSolution();

	GenerateResult(currentComm);

	MPI_Finalize();
	fTime.close();
	return 0;
}
