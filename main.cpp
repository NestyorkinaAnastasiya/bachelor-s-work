#include "balance.cpp"

void FindSolution()
{
	// Атрибуты потока
	pthread_attr_t attrs;
	
	// Инициализация атрибутов потока
	if (0 != pthread_attr_init(&attrs))
	{
		perror("Cannot initialize attributes");
		abort();
	};

	// Установка атрибута "присоединённый"
	if (0 != pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_JOINABLE))
	{
		perror("Error in setting attributes");
		abort();
	}
	double sum;
	MPI_Request s;
	MPI_Status st;
	int exit = -1;
	// Порождение диспетчера
	if (size != 0)
		if (0 != pthread_create(&thrs[countOfWorkers], &attrs, dispatcher, &ids[countOfWorkers])) {
			perror("Cannot create a thread");
			abort();
		}
	// Порождение контроллера карт
	if (0 != pthread_create(&thrs[countOfWorkers+1], &attrs, mapController, &ids[countOfWorkers+1]))
	{
		perror("Cannot create a thread");
		abort();
	}
	MPI_Comm_dup(currentComm, &serverComm);
	MPI_Comm_dup(currentComm, &reduceComm);
	MPI_Comm_dup(currentComm, &barrierComm);
	// Порождение сервера
	if (0 != pthread_create(&thrs[countOfWorkers+2], &attrs, server, &ids[countOfWorkers+2]))
	{
		perror("Cannot create a thread");
		abort();
	}
	std::vector<int> flags(size);
	std::vector<int> globalFlags(size);

	for (iteration = 0; iteration < maxiter && CheckConditions(); iteration++)
	{	
		
		if (rank == 0) printf("%d::  --------------------START ITERATION %d---------------------\n", rank, iteration);
		for (auto &i : newResult) i = 0;
		for (auto &i : oldResult) i = 0;
		
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

		fprintf(stderr,"%d::workers closed\n", rank);	
		bool change = false;
		flags[rank] = changeComm;
		
		MPI_Allreduce(flags.data(), globalFlags.data(), globalFlags.size(), MPI_INT, MPI_SUM, reduceComm);
		for (int i = 0; i < globalFlags.size() && !change; i++)
			if (globalFlags[i]) change = true;
		// Если происходило изменение коммуникатора в ходе работы потоков
		// то посылаем сообщение, характеризующее закрытие старого коммуникатора
		if (change) {
			
			int cond = 4; 
			// Отправка сообщения о необходимости закрыть старый коммуникатор			
			MPI_Send(&cond, 1, MPI_INT, rank, 2001, currentComm);
			fprintf(stderr,"%d::send to old_dispetcher sucsess\n", rank);
			
			if (rank == 0) 
				for(int k = size_old; k < size; k++) 
				MPI_Send(&iteration, 1, MPI_INT, k, 10005, newComm);
			
			fprintf(stderr,"%d::start dup\n", rank);
			MPI_Comm_dup(newComm, &serverComm);
			fprintf(stderr,"%d::dup server sucsess\n", rank);
			MPI_Comm_dup(newComm, &reduceComm);
			fprintf(stderr,"%d::dup reduce sucsess\n", rank);
			MPI_Comm_dup(newComm, &barrierComm);
			while (!changeComm); // условная переменная
			currentComm = newComm;
			flags.resize(size);
			globalFlags.resize(size);
			changeComm = false;
			server_new = false;
		}
			
		GenerateResultOfIteration(reduceComm);

		while (!queueRecv.empty()) {
			Task *t = queueRecv.front();
			t->SendToNeighbors(currentComm);
			queueRecv.pop();		
			allTasks.push(t);
			
		}

		if (rank == 0) {
			printf("%d:: res = %e\n",rank, residual );
			printf("%d:: --------------------FINISH ITERATION %d---------------------\n", rank, iteration);
			
		}
	}
	MPI_Isend(&exit, 1, MPI_INT, rank, 1030, currentComm, &s);
	MPI_Isend(&exit, 1, MPI_INT, rank, 2001, currentComm, &s);
	pthread_join(thrs[countOfWorkers], NULL);
	fprintf(stderr,"%d::dispetcher close\n", rank);
	// Если процессы подсоединились поcле всех вычислений
	if (rank == 0){
		int cond, flag, size_new;
		while (numberOfConnection < countOfConnect) {
			MPI_Recv(&cond, 1, MPI_INT, rank, 2001, currentComm, &st);
			size_old = size;
			// Вычисляем новый размер и rаnk
       			MPI_Comm_size(newComm, &size_new);
	
			cond = 0;
			for(int k = size_old; k < size_new; k++) 
				MPI_Send(&cond, 1, MPI_INT, k, 10000, newComm);			
				
			server_new = false;

		}
	}
	pthread_join(thrs[countOfWorkers+1], NULL);	
	fprintf(stderr,"%d::mapController close\n", rank);
	pthread_join(thrs[countOfWorkers+2], NULL);
	fprintf(stderr,"%d::server close\n", rank);
	
	// Освобождение ресурсов, занимаемых описателем атрибутов
	//pthread_attr_destroy(&attrs);
}

int main(int argc, char **argv)
{
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

	GenerateBasicConcepts();	

	GenerateQueueOfTask();
	
	std::vector<int> tmp(map.size());
	MPI_Allreduce(map.data(), tmp.data(), map.size(), MPI_INT, MPI_SUM, currentComm);
	map = tmp;

	FindSolution();

	GenerateResult(currentComm);
	
	MPI_Finalize();
	return 0;
}
