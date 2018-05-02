#include "task.cpp"
// Размер глобального списка задач
int problemSize;
// Переменные, необходимые для работы с потоками
int ids[11] = { 0,1,2,3,4,5,6,7,8,9,10 };
// Четыре объекта типа "описатель потока"
pthread_t thrs[8];

int condition = 0;

// Число вычислительных потоков
int countOfWorkers = 1;
// Общее число потоков
int countOfThreads = 3;
std::queue<Task*> currentTasks;

bool GetTask(Task **currTask)
{	
	// Блокируем доступ других потоков для избежания ошибок
	// в следствии некорректной работы с очередью
	pthread_mutex_lock(&mutex);
	// Если очередь задач пуста
	if (currentTasks.empty())
	{
		// Снимаем замок
		pthread_mutex_unlock(&mutex);
		return false;
	}
	else
	{
		// Достаём задачу из очереди
		*currTask = currentTasks.front();
		currentTasks.pop();
	}
	pthread_mutex_unlock(&mutex);
	return true;
}

// Функция вычислительного потока
void* worker(void* me)
{	
	//Текущая задача
	Task *currTask;
	// Пока есть свои задачи - выполняем свои
	while (GetTask(&currTask)) {
		currTask->Run();
		allTasks.push(currTask);
	}

	int  exitTask = 0;
	// Запрашиваем по одной задаче от каждого узла кроме самого себя
	for (int i = 0; i < size; i++)
	{
		// Если узел не равен текущему
		if (i != rank)
		{	// Отправляем запрос на получение задачи 
			condition = 0;
			MPI_Send(&condition, 1, MPI_INT, i, 2001, MPI_COMM_WORLD);
			MPI_Status st;
			// Получаем результат запроса в виде информации о том,
			// есть ли задача у узла или нет
			MPI_Recv(&exitTask, 1, MPI_INT, i, 2002, MPI_COMM_WORLD, &st);

			// Если такая задача есть, то получаем данные задачи
			if (exitTask)
			{
				Task *t = new Task;
				printf("%d:: i = %d\n",rank,i);
				GenerateRecv(t, i);
				//pthread_mutex_lock(&mutex);
				/*printf("%d:: block = %d, lock = %d, tpp = %d, f = %d, fs = %d\n",rank,t->blockNumber,t->localNumber, t->tasks_x, t->flag, t->firstStart );
				printf("existRecv: %d %d %d %d %d %d\n", t->existRecv[0], t->existRecv[1], t->existRecv[2], t->existRecv[3], t->existRecv[4],t->existRecv[5]);
				printf("neighbors: %d %d %d %d %d %d\n", t->neighbors[0], t->neighbors[1],t->neighbors[2],t->neighbors[3],t->neighbors[4],t->neighbors[5]);
				
				printf("oldU:\n");
				for (int z = 0; z< t->oldU.size();z++)
					printf("oldU[%d] = %lf\n", z, t->oldU[z]);
				printf("\n");
				printf("newU:\n");
				for (int z = 0; z< t->newU.size();z++)
					printf("newU[%d] = %lf\n", z, t->newU[z]);
				printf("\n");
				printf("F:\n");
				for (int z = 0; z< t->F.size();z++)
					printf("F[%d] = %lf\n", z, t->F[z]);
				printf("\n");
				
				printf("POINTS:\n");
				for (int z = 0; z< t->points.size();z++)
					printf("p[%d]: x = %lf, y = %lf, z = %lf, glN = %d\n", z, t->points[z].x, t->points[z].y, t->points[z].z, t->points[z].globalNumber);
				printf("\n");
				printf("borders:\n");
				for (int k =0; k<6; k++)
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
				
				// Запускаем полученную задачу
				t->Run();
				allTasks.push(t);
				pthread_mutex_lock(&mutex);
				std:: cerr << i <<"\n";
				pthread_mutex_unlock(&mutex);
				printf("%d:: TASK FINISHHH\n",rank);
				//std:: cerr << "TASK FINISHHH\n";
				i--;
			}
		}
	}
	//allTasks.back().blockNumber
	printf("%d:: close thread\n",rank);
	pthread_mutex_lock(&mutex);
	//std:: cerr << rank <<":: close thread\n";
	pthread_mutex_unlock(&mutex);
	return 0;
}

// Диспетчер
// (отвечает за пересылку задач другим узлам)
void* dispatcher(void* me)
{
	Task *t;
	int cond;
	// Количество обращений к диспетчеру
	int countOfAsks = 0;
	// Пока не получены всевозможные сообщения от макситального кол-ва узлов
	while (countOfAsks < (size - 1)*countOfWorkers)
	{
		MPI_Status st;
		// Получаем запрос о выдачи задачи от любого узла
		MPI_Recv(&cond, 1, MPI_INT, MPI_ANY_SOURCE, 2001, MPI_COMM_WORLD, &st);
		// Получаем номер этого узла
		int peer = st.MPI_SOURCE;
		// Флаг ответа на то, есть задачи в узле, или нет
		int send = 0;
		// Если в очереди есть задача, получаем её
		if (GetTask(&t)) {
			send = 1;	
			// Отправляем сообщение о том, что можем отправить задачу
			MPI_Send(&send, 1, MPI_INT, peer, 2002, MPI_COMM_WORLD);

			// Отправляем своё будущее расположение всем процессам 
			for (int j = 0; j < size; j++) 
				if (j != rank) {
					MPI_Isend(&t->blockNumber, 1, MPI_INT, j, 1030, MPI_COMM_WORLD, &t->sendReq[0]); 
					MPI_Isend(&peer, 1, MPI_INT, j, 1031, MPI_COMM_WORLD, &t->sendReq[0]); 
				}
				else { newMap[t->blockNumber] = peer; }
	
			GenerateSend(t, peer);
			printf("%d::SEND TASK f = %d, e0 = %d, e1 = %d, e2 = %d, e3 = %d, e4 = %d, e5 = %d, fs = %d\n",rank,t->flag, t->existRecv[0], t->existRecv[1], t->existRecv[2], t->existRecv[3], t->existRecv[4], t->existRecv[5], t->firstStart);
		}
		// Собственные задачи кончились, запускаем счётчик количества обращений
		else {
			// Отправляем сообщение о том, что задачи кончились
			MPI_Send(&send, 1, MPI_INT, peer, 2002, MPI_COMM_WORLD);
			countOfAsks++;
		}
	}	
	//printf("%d:: close dispatcher\n",rank);
	pthread_mutex_lock(&mutex);
	std:: cerr << rank <<":: close dispatcher\n";
	pthread_mutex_unlock(&mutex);
	return 0;

}

void* mapController(void* me)
{
	MPI_Status st;
	bool flag = true;
	int map_id, rank_id;
	while (flag)
	{
		MPI_Recv(&map_id, 1, MPI_INT, MPI_ANY_SOURCE, 1030, MPI_COMM_WORLD, &st);
		// Получаем номер этого узла
		int peer = st.MPI_SOURCE;
		if (map_id >= 0) {
			MPI_Recv(&rank_id, 1, MPI_INT, peer, 1031, MPI_COMM_WORLD, &st);
			newMap[map_id] = rank_id;
		}
		else flag = false;
	}
	//printf("%d:: close mapController\n",rank);	

	pthread_mutex_lock(&mutex);
	std:: cerr << rank <<":: close mapController\n";
	pthread_mutex_unlock(&mutex);
}