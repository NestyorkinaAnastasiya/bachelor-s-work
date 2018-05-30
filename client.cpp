#include "balance.cpp"
#include <iostream>
#include <fstream>
#define MAX_DATA 1000
#define ERROR_INIT_BARRIER -14

pthread_barrier_t barrier;
bool client = true;

void FindSolution()
{
	MPI_Request s;
	MPI_Status st;
	int exit = -1;			
	std::vector<int> flags(size);
	std::vector<int> globalFlags(size);

	// Атрибуты потока
	pthread_attr_t attrs;
	// Инициализация атрибутов потока
	if (0 != pthread_attr_init(&attrs)) {
		perror("Cannot initialize attributes");
		abort();
	};

	// Установка атрибута "присоединённый"
	if (0 != pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_JOINABLE)) {
		perror("Error in setting attributes");
		abort();
	}
	
	// Порождение диспетчера
	if (0 != pthread_create(&thrs[countOfWorkers], &attrs, dispatcher, &ids[countOfWorkers])) {
		perror("Cannot create a thread");
		abort();
	}
	// Порождение контроллера карт
	if (0 != pthread_create(&thrs[countOfWorkers+1], &attrs, mapController, &ids[countOfWorkers+1])) {
		perror("Cannot create a thread");
		abort();
	}

	for (iteration = 0; iteration < maxiter && CheckConditions(); iteration++)
	{	
		if (rank_old == 0) printf("%d::  --------------------START ITERATION %d---------------------\n", rank, iteration);
		for (auto &i : newResult) i = 0;
		for (auto &i : oldResult) i = 0;
	
		// Получаем информацию от соседей и формируем очередь подзадач
		while (!allTasks.empty()) {
			Task *t = allTasks.front();
			if(iteration != 0) t->ReceiveFromNeighbors(currentComm);
			pthread_mutex_lock(&mutex_get_task);				
			currentTasks.push(t);
			pthread_mutex_unlock(&mutex_get_task);
			allTasks.pop();
		}
		
		fprintf(stderr, "%d:: count of tasks = %d\n", rank, currentTasks.size());

		// Порождение рабочих потоков
		for (int i = 0; i < countOfWorkers; i++)
			if (0 != pthread_create(&thrs[i], &attrs, worker, &ids[i])) {
				perror("Cannot create a thread");
				abort();
			}
		
		// Ожидание завершения порожденных потоков
		for (int i = 0; i < countOfWorkers; i++)
			if (0 != pthread_join(thrs[i], NULL)) {
				perror("Cannot join a thread");
				abort();
			}

		fprintf(stderr,"%d:: workers closed\n", rank);

		bool change = false;
		if (!client) {
			flags[rank] = changeComm;
			// Если хотя бы один процесс перешёл в событие изменения коммуникатора
			MPI_Allreduce(flags.data(), globalFlags.data(), globalFlags.size(), MPI_INT, MPI_SUM, reduceComm);
			for (int i = 0; i < globalFlags.size() && !change; i++)
				if (globalFlags[i]) change = true;
		}
	
		if (change) {
			int cond = 4; 
			// Отправка сообщения о необходимости закрыть старый коммуникатор
			MPI_Send(&cond, 1, MPI_INT, rank, 2001, currentComm);
			
			MPI_Comm_dup(newComm, &serverComm);
			MPI_Comm_dup(newComm, &reduceComm);
			MPI_Comm_dup(newComm, &barrierComm);

			while (!changeComm);
			currentComm = newComm;	
			flags.resize(size);
			globalFlags.resize(size);
			changeComm = false;		
		}
		if (client){
			MPI_Recv(&iteration, 1, MPI_INT, 0, 10005, currentComm, &st);
			fprintf(stderr,"%d::start dup\n", rank);
			MPI_Comm_dup(currentComm, &serverComm);
			fprintf(stderr,"%d::dup server sucsess\n", rank);
			// Порождение сервера
			if (0 != pthread_create(&thrs[countOfWorkers+2], &attrs, server, &ids[countOfWorkers+2]))
			{
				perror("Cannot create a thread");
				abort();
			}
			MPI_Comm_dup(currentComm, &reduceComm);
			fprintf(stderr,"%d::dup reduce sucsess\n", rank);
			MPI_Comm_dup(currentComm, &barrierComm);
			client = false;			
		}
		
		GenerateResultOfIteration(reduceComm);
				
		while (!queueRecv.empty()) {
			Task *t = queueRecv.front();
			t->SendToNeighbors(currentComm);				
			allTasks.push(t);
			queueRecv.pop();
		}
		
		if (rank_old == 0) printf("%d:: --------------------FINISH ITERATION %d---------------------\n", rank, iteration);
	}

	MPI_Isend(&exit, 1, MPI_INT, rank, 1030, currentComm, &s);
	MPI_Isend(&exit, 1, MPI_INT, rank, 2001, currentComm, &s);
	pthread_join(thrs[countOfWorkers], NULL);
	fprintf(stderr,"%d::dispetcher close\n", rank);
	pthread_join(thrs[countOfWorkers+1], NULL);	
	fprintf(stderr,"%d::mapController close\n", rank);	
	pthread_join(thrs[countOfWorkers+2], NULL);	
	// Освобождение ресурсов, занимаемых описателем атрибутов
	pthread_attr_destroy(&attrs);
}

int main(int argc, char **argv)
{
	fprintf(stderr,"%d:: I started\n", rank);
	int provided = MPI_THREAD_SINGLE;
	MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
	if (provided != MPI_THREAD_MULTIPLE)	{
		std::cerr << "not MPI_THREAD_MULTIPLE";
		exit(0);
	}

	MPI_Comm_rank(currentComm, &rank);
	//Получаем количество узлов
	MPI_Comm_size(currentComm, &size);

	// Инициализация мьютекса
	pthread_mutexattr_t attr_get_task;
	pthread_mutexattr_init(&attr_get_task);
	pthread_mutex_init(&mutex_get_task, &attr_get_task);
	
	pthread_mutexattr_t attr_set_task;
	pthread_mutexattr_init(&attr_set_task);
	pthread_mutex_init(&mutex_set_task, &attr_set_task);

	
	MPI_Comm server;
	MPI_Status st;
	double buf[MAX_DATA];

	char port_name[MPI_MAX_PORT_NAME];
	std::ifstream fPort("port_name.txt");
	for(int i = 0; i < MPI_MAX_PORT_NAME; i++)
		fPort >> port_name[i]; 
	fPort.close();
	fprintf(stderr,"%d::port exist\n", rank);
	/* assume server’s name is cmd-line arg */
	MPI_Comm_connect(port_name, MPI_INFO_NULL, 0, currentComm, &server);
	MPI_Intercomm_merge(server, true, &currentComm); 
	//currentComm = newComm;
	fprintf(stderr,"%d::connect to server success\n", rank);
	rank_old = rank;
	MPI_Comm_rank(currentComm, &rank);
        MPI_Comm_size(currentComm, &size);
	fprintf(stderr,"%d:: new rank = %d, new_size = %d\n", rank_old, rank, size);

	int sizeOfMap;
	MPI_Recv(&numberOfConnection, 1, MPI_INT, 0, 10002, currentComm, &st);
	MPI_Recv(&sizeOfMap, 1, MPI_INT, 0, 10000, currentComm, &st);
	fprintf(stderr,"%d:: sizeOfMap = %d\n", rank, sizeOfMap);
	// Если передан размер, а не сообщение о завершении общей работы
	if(sizeOfMap)       
        {
		map.resize(sizeOfMap);
		MPI_Recv(map.data(), sizeOfMap, MPI_INT, 0, 10001, currentComm, &st);
		for(int i = 0; i < map.size(); i++)
			printf("%d; ", map[i]);
		GenerateBasicConcepts();
		FindSolution();	
	
		GenerateResult(currentComm);    
	}

	MPI_Finalize();
	return 0;
}
