#include "task.cpp"
// Размер глобального списка задач
int problemSize;
// Переменные, необходимые для работы с потоками
int ids[11] = { 0,1,2,3,4,5,6,7,8,9,10 };
// Четыре объекта типа "описатель потока"
pthread_t thrs[8];

int condition = 0;

// Число вычислительных потоков
int countOfWorkers = 4;
// Общее число потоков
int countOfThreads = 6;
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
	while (GetTask(&currTask))
		currTask->Run();

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
				Task t;
				GenerateRecv(&t, i);
				
				// Запускаем полученную задачу
				t.Run();
					
				i--;
			}
		}
	}
	printf("%d:: close thread\n",rank);
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
			for (int j = 0; j < size; j++) {
				MPI_Isend(&t->blockNumber, 1, MPI_INT, j, 1030, MPI_COMM_WORLD, &t->sendReq[0]); 
				MPI_Isend(&peer, 1, MPI_INT, j, 1031, MPI_COMM_WORLD, &t->sendReq[0]); 
			}
	
			GenerateSend(t, peer);
		}
		// Собственные задачи кончились, запускаем счётчик количества обращений
		else {
			// Отправляем сообщение о том, что задачи кончились
			MPI_Send(&send, 1, MPI_INT, peer, 2002, MPI_COMM_WORLD);
			countOfAsks++;
		}
	}	
	printf("%d:: close dispatcher\n",rank);
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
	printf("%d:: close mapController\n",rank);	
}