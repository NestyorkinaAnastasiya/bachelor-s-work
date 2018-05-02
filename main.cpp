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
	// Порождение диспетчера
	if (0 != pthread_create(&thrs[countOfWorkers+1], &attrs, mapController, &ids[countOfWorkers+1]))
	{
		perror("Cannot create a thread");
		abort();
	}

	for (iteration = 0; iteration < maxiter && residual > eps; iteration++)
	{	
		if (rank == 0) printf("%d::  --------------------START ITERATION %d---------------------\n", rank, iteration);
		for (auto i : newResult) i = 0;
		for (auto i : oldResult) i = 0;
		
		oldMap = newMap;
		while (!allTasks.empty()) {
			Task *t = allTasks.front();					
			currentTasks.push(t);
			allTasks.pop();
		}
		
		/*printf("%d:: block = %d, lock = %d, tpp = %d, f = %d, fs = %d\n",rank,currentTasks.back()->blockNumber,currentTasks.back()->localNumber, currentTasks.back()->tasks_x, currentTasks.back()->flag, currentTasks.back()->firstStart );
		printf("existRecv: %d %d %d %d %d %d\n", currentTasks.back()->existRecv[0], currentTasks.back()->existRecv[1], currentTasks.back()->existRecv[2], currentTasks.back()->existRecv[3], currentTasks.back()->existRecv[4], currentTasks.back()->existRecv[5]);
		printf("neighbors: %d %d %d %d %d %d\n", currentTasks.back()->neighbors[0], currentTasks.back()->neighbors[1], currentTasks.back()->neighbors[2], currentTasks.back()->neighbors[3], currentTasks.back()->neighbors[4], currentTasks.back()->neighbors[5]);
				
		printf("oldU:\n");
		for (int z = 0; z < currentTasks.back()->oldU.size();z++)
			printf("oldU[%d] = %lf\n", z, currentTasks.back()->oldU[z]);
		printf("\n");
		printf("newU:\n");
		for (int z = 0; z <  currentTasks.back()->newU.size();z++)
			printf("newU[%d] = %lf\n", z,  currentTasks.back()->newU[z]);
		printf("\n");*/
		printf("%d:: SIZE = %d\n",rank,currentTasks.size());

		// Порождение рабочих потоков
		for (int i = 0; i < countOfWorkers; i++)
			if (0 != pthread_create(&thrs[i], &attrs, worker, &ids[i])) {
				perror("Cannot create a thread");
				abort();
			}
		
		// Порождение диспетчера
		if (size != 1)
		if (0 != pthread_create(&thrs[countOfWorkers], &attrs, dispatcher, &ids[countOfWorkers])) {
			perror("Cannot create a thread");
			abort();
		}

		// Ожидание завершения порожденных потоков
		if (size != 1) {
			for (int i = 0; i < countOfThreads-1; i++)
				if (0 != pthread_join(thrs[i], NULL)) {
					perror("Cannot join a thread");
					abort();
				}
		}
		else {	
			for (int i = 0; i < countOfWorkers; i++)
				if (0 != pthread_join(thrs[i], NULL)) {
					perror("Cannot join a thread");
					abort();
				}
		}

		sum = 0;
		// Расчёт абсолютной погрешности между текущим и предыдущим решениями
		for (int i = 0; i < newResult.size(); i++)
			sum += (newResult[i] - oldResult[i])*(newResult[i] - oldResult[i]);

		MPI_Allreduce(&sum, &residual, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
		residual = sqrt(residual);
		printf("%d:: res = %lf\n",rank, residual );
		MPI_Allreduce(newResult.data(), globalRes.data(), newResult.size(), MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);

		/*if (rank == 0) {
			
			// Вывод результата
			//if (iteration == 55){
				printf("\n--------------------------------------------------------------------\n\n");
				double sum = 0;
				for (int i = 0; i < globalRes.size(); i++)
				{
					sum += (2. - globalRes[i])*(2. - globalRes[i]);
					printf("%.14lf\n", newResult[i]);
				}	
				sum = sqrt(sum);
				printf("||result|| = %.10e\n", sum);
				printf("\n--------------------------------------------------------------------\n");
			//}
		}*/
		if (rank == 0) printf("%d:: --------------------FINISH ITERATION %d---------------------\n", rank, iteration);
	}
	MPI_Request s;
	int exit = -1;
	MPI_Isend(&exit, 1, MPI_INT, rank, 1030, MPI_COMM_WORLD, &s);
	if (0 != pthread_join(thrs[countOfThreads-1], NULL)) {
		perror("Cannot join a thread");
		abort();
	}
	// Освобождение ресурсов, занимаемых описателем атрибутов
	pthread_attr_destroy(&attrs);
}

int main(int argc, char **argv)
{
	int provided = MPI_THREAD_SINGLE;
	MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
	if (provided != MPI_THREAD_MULTIPLE)	{
		std::cerr << "not MPI_THREAD_MULTIPLE";
		exit(0);
	}
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	//Получаем количество узлов
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	MPI_Status st;
	// Инициализация мьютекса
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutex_init(&mutex, &attr);

	GenerateQueueOfTask();
	FindSolution();


	MPI_Allreduce(newResult.data(), globalRes.data(), newResult.size(), MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
	//MPI_Reduce(timer.data(), time.data(), time.size(), MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
	//MPI_Reduce(iterations.data(), resIterations.data(), resIterations.size(), MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
	//MPI_Reduce(res.data(), globalR.data(), res.size(), MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
	
	/*unsigned int allTime = 0;
	for (int i = 0; i < size; i++)
		if (allTime < time[i])
			allTime = time[i];*/

	// Вывод результата
	if (rank == 0)
	{
		printf("\n--------------------------------------------------------------------\n\n");
		double sum = 0;
		for (int i = 0; i < globalRes.size(); i++)
		{
			sum += (2. - globalRes[i])*(2. - globalRes[i]);
			printf("%.14lf\n", globalRes[i]);
		}
		sum = sqrt(sum);
		printf("||result|| = %.10e\n", sum);
		//printf("dimention: %d\tcountOfProcess: %d\tallTime: %d\n\n",dim, size, allTime);
		/*for (int i = 0; i < size; i++)
			printf("rank %d::\ttime = %d;\tcountOfIter = %d;\tresidual = %e\n", i, time[i], resIterations[i], globalR[i]);*/
		printf("\n--------------------------------------------------------------------\n");
	}

	MPI_Finalize();
	return 0;
}
