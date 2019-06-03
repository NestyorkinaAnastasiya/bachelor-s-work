#include "task.cpp"
// allTasks is generated by user
std::queue<ITask*> allTasks;
bool client = true;
time_t rawtime;
struct tm * timeinfo;

char buffer[80];
std::ofstream fTime;

double residual = 1;
double eps = 1e-8;
Library lib;

bool CheckConditions() {
	if (residual > eps) return true;
	else return false;
}
void FindSolution() {
	MPI_Status st;
	std::stringstream ss;
	ss << Library::rank;
	std::string nameFile = "Loading" + ss.str();
	nameFile += ".txt";
	std::ofstream fLoading(nameFile);
	if (client) {
		MPI_Recv(&iteration, 1, MPI_INT, 0, 10005, Library::currentComm, &st);
		client = false;
	}
	for (iteration = 0; iteration < maxiter && CheckConditions(); iteration++) {
		/* if (rank_old == 0)*/ printf("%d::  --------------------START ITERATION %d---------------------\n", Library::rank, iteration);
		for (auto &i : newResult) i = 0;
		for (auto &i : oldResult) i = 0;

		while (!allTasks.empty()) {
			Task *t = dynamic_cast<Task*>(allTasks.front());
			if (iteration != 0) t->ReceiveFromNeighbors(Library::currentComm);
			Library::queueRecv.push(t);
			allTasks.pop();
		}

		while (!Library::queueRecv.empty()) {
			Task *t = dynamic_cast<Task*>(Library::queueRecv.front());
			if (iteration != 0) t->WaitBorders();
			pthread_mutex_lock(&Library::mutex_get_task);
			Library::currentTasks.push(t);
			pthread_mutex_unlock(&Library::mutex_get_task);
			Library::queueRecv.pop();
		}
		fprintf(stderr, "%d:: count of tasks = %d\n", Library::rank, Library::currentTasks.size());

		lib.StartWork();
				
		GenerateResultOfIteration(Library::educeComm);

		while (!Library::queueRecv.empty()) {
			Task *t = dynamic_cast<Task*>(Library::queueRecv.front());
			t->SendToNeighbors(Library::currentComm);
			allTasks.push(t);
			Library::queueRecv.pop();
		}
		fLoading << "iteration " << iteration << "::  " << allTasks.size() << "\ttasks\n";
		if (Library::rank_old == 0) printf("%d:: --------------------FINISH ITERATION %d---------------------\n", Library::rank, iteration);
	}
	lib.CloseLibraryComponents();
	
	fLoading.close();
}

int main(int argc, char **argv) {
	time(&rawtime);
	timeinfo = localtime(&rawtime);
	if (Library::rank == 0) {
		strftime(buffer, 80, "%H:%M:%S", timeinfo);
		puts(buffer);
		std::stringstream ss;
		ss << Library::numberOfConnection;
		std::string nameFile = "time_client" + ss.str();
		nameFile += ".txt";
		fTime.open(nameFile);
		fTime << "client's processes start in " << buffer << "\n";
		fTime.close();
	}
	lib.LibraryInitialize(argc, argv, true);
	// If work exist
	if (Library::map.size())	{
		GenerateBasicConcepts();
		FindSolution();
		GenerateResult(Library::currentComm);
	}
	MPI_Finalize();
	return 0;
}
