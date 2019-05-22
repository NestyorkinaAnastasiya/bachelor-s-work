#include "task.cpp"
client = true;
std::ofstream fTime;
void FindSolution() {
	MPI_Status st;
	std::stringstream ss;
	ss << rank;
	std::string nameFile = "Loading" + ss.str();
	nameFile += ".txt";
	std::ofstream fLoading(nameFile);

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

		StartWork();

		if (client)
			MPI_Recv(&iteration, 1, MPI_INT, 0, 10005, currentComm, &st);
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
	CloseLibraryComponents();
	
	fLoading.close();
}

int main(int argc, char **argv) {
	time(&rawtime);
	timeinfo = localtime(&rawtime);
	if (rank == 0) {		
		strftime(buffer, 80, "%H:%M:%S", timeinfo);
		puts(buffer);
		std::stringstream ss;
		ss << numberOfConnection;
		std::string nameFile = "time_client" + ss.str();
		nameFile += ".txt";
		fTime.open(nameFile);
		Time << "client's processes start in " << buffer << "\n";
		fTime.close();
	}
	LibraryInitialize(true);
	// If work exist
	if (map.size())	{
		GenerateBasicConcepts();
		FindSolution();
		GenerateResult(currentComm);
	}
	MPI_Finalize();
	return 0;
}
