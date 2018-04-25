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
		for (auto i : newResult) i = 0;
		for (auto i : oldResult) i = 0;
		
		oldMap = newMap;
		newMap = firstMap;
		
		for (int i = 0; i < allTasks.size(); i++)
		{
			currentTasks.push(allTasks[i]);
		}
		// Порождение рабочих потоков
		for (int i = 0; i < countOfWorkers; i++)
			if (0 != pthread_create(&thrs[i], &attrs, worker, &ids[i]))
			{
				perror("Cannot create a thread");
				abort();
			}
		
		// Порождение диспетчера
		if (0 != pthread_create(&thrs[countOfWorkers], &attrs, dispatcher, &ids[countOfWorkers])) {
			perror("Cannot create a thread");
			abort();
		}

		// Ожидание завершения порожденных потоков
		for (int i = 0; i < countOfThreads-1; i++)
		//for (int i = 0; i < countOfWorkers; i++)
			if (0 != pthread_join(thrs[i], NULL)) {
				perror("Cannot join a thread");
				abort();
			}
		sum = 0;
		// Расчёт абсолютной погрешности между текущим и предыдущим решениями
		for (int i = 0; i < newResult.size(); i++)
				sum += (newResult[i] - oldResult[i])*(newResult[i] - oldResult[i]);

		MPI_Allreduce(&sum, &residual, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
		residual = sqrt(residual);
		if (rank == 0)
			printf("%d:: ITERATION %d\n", rank, iteration);
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

	if (size != 1)
	{
		MPI_Allreduce(newResult.data(), globalRes.data(), newResult.size(), MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
		//MPI_Reduce(timer.data(), time.data(), time.size(), MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
		//MPI_Reduce(iterations.data(), resIterations.data(), resIterations.size(), MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
		//MPI_Reduce(res.data(), globalR.data(), res.size(), MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
	}
	else
	{
		globalRes = newResult;
		//time[0] = timer[0];
		//resIterations[0] = iteration;
		//globalR[0] = residual;
	}

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
