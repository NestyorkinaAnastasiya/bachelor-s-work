#include "task.cpp"
// ������ ����������� ������ �����
int problemSize;
// ����������, ����������� ��� ������ � ��������
int ids[11] = { 0,1,2,3,4,5,6,7,8,9,10 };
// ������ ������� ���� "��������� ������"
pthread_t thrs[8];

int condition = 0;

// ����� �������������� �������
int countOfWorkers = 4;
// ����� ����� �������
int countOfThreads = 6;
std::queue<Task*> currentTasks;

bool GetTask(Task **currTask)
{
	// ��������� ������ ������ ������� ��� ��������� ������
	// � ��������� ������������ ������ � ��������
	pthread_mutex_lock(&mutex);
	// ���� ������� ����� �����
	if (currentTasks.empty())
	{
		// ������� �����
		pthread_mutex_unlock(&mutex);
		return false;
	}
	else
	{
		// ������ ������ �� �������
		*currTask = currentTasks.front();
		currentTasks.pop();
	}
	pthread_mutex_unlock(&mutex);
	return true;
}

// ������� ��������������� ������
void* worker(void* me)
{
	//������� ������
	Task *currTask;
	// ���� ���� ���� ������ - ��������� ����
	while (GetTask(&currTask))
		currTask->Run();

	int  exitTask = 0;
	// ����������� �� ����� ������ �� ������� ���� ����� ������ ����
	for (int i = 0; i < size; i++)
	{
		// ���� ���� �� ����� ��������
		if (i != rank)
		{	// ���������� ������ �� ��������� ������ 
			condition = 0;
			MPI_Send(&condition, 1, MPI_INT, i, 2001, MPI_COMM_WORLD);
			MPI_Status st;
			// �������� ��������� ������� � ���� ���������� � ���,
			// ���� �� ������ � ���� ��� ���
			MPI_Recv(&exitTask, 1, MPI_INT, i, 2002, MPI_COMM_WORLD, &st);

			// ���� ����� ������ ����, �� �������� ������ ������
			if (exitTask)
			{
				Task t;
				GenerateRecv(&t, i);
				
				// ��������� ���������� ������
				t.Run();
					
				i--;
			}
		}
	}
	printf("%d:: close thread\n",rank);
	return 0;
}

// ���������
// (�������� �� ��������� ����� ������ �����)
void* dispatcher(void* me)
{
	Task *t;
	int cond;
	// ���������� ��������� � ����������
	int countOfAsks = 0;
	// ���� �� �������� ������������ ��������� �� ������������� ���-�� �����
	while (countOfAsks < (size - 1)*countOfWorkers)
	{
		MPI_Status st;
		// �������� ������ � ������ ������ �� ������ ����
		MPI_Recv(&cond, 1, MPI_INT, MPI_ANY_SOURCE, 2001, MPI_COMM_WORLD, &st);
		// �������� ����� ����� ����
		int peer = st.MPI_SOURCE;
		// ���� ������ �� ��, ���� ������ � ����, ��� ���
		int send = 0;
		// ���� � ������� ���� ������, �������� �
		if (GetTask(&t)) {
			send = 1;	
			// ���������� ��������� � ���, ��� ����� ��������� ������
			MPI_Send(&send, 1, MPI_INT, peer, 2002, MPI_COMM_WORLD);

			// ���������� ��� ������� ������������ ���� ��������� 
			for (int j = 0; j < size; j++) {
				MPI_Isend(&t->blockNumber, 1, MPI_INT, j, 1030, MPI_COMM_WORLD, &t->sendReq[0]); 
				MPI_Isend(&peer, 1, MPI_INT, j, 1031, MPI_COMM_WORLD, &t->sendReq[0]); 
			}
	
			GenerateSend(t, peer);
		}
		// ����������� ������ ���������, ��������� ������� ���������� ���������
		else {
			// ���������� ��������� � ���, ��� ������ ���������
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
		// �������� ����� ����� ����
		int peer = st.MPI_SOURCE;
		if (map_id >= 0) {
			MPI_Recv(&rank_id, 1, MPI_INT, peer, 1031, MPI_COMM_WORLD, &st);
			newMap[map_id] = rank_id;
		}
		else flag = false;
	}
	printf("%d:: close mapController\n",rank);	
}