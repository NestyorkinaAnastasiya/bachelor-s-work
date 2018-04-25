#define HAVE_STRUCT_TIMESPEC
#include <pthread.h>
#define MSMPI_NO_DEPRECATE_20
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctime>
#include <iostream>
#include <queue>
#include <vector>
#include <array>
#include <fstream>
#include <math.h>
#include <stddef.h>

pthread_mutex_t mutex;
// Максимальное кол-во итераций
int maxiter = 100;
double eps = 1e-8;
double residual = 1;
int dim;
int size, rank;
// Шаги сетки
double	/*hx = 0.1,
		hy = 0.1,
		hz = 0.1;*/
		/*hx = 0.2,
		hy = 0.2,
		hz = 0.2;*/

	hx = 0.5,
	hy = 0.5,
	hz = 0.5;

/*hx = 1,
hy = 1,
hz = 1;*/

int sizeBlockY = 2,
sizeBlockZ = 2;
std:: vector <int> tasks_y, tasks_z, oldMap, newMap, firstMap;
int k_y, k_z;
// Координаты начала и конца области
double	begX = 0, endX = 2,
begY = 0, endY = 2,
begZ = 0, endZ = 2;
MPI_Datatype MPI_POINT;

struct Point
{
	double x, y, z;
	int globalNumber;

	void set(double x1, double y1, double z1, int glN)
	{
		x = x1;
		y = y1;
		z = z1;
		globalNumber = glN;
	};
};
std::vector<double> oldResult, newResult;
std::vector <double> globalRes;
std::vector <double> globalOldRes;
int iteration = 0;
// Количество интервалов по координате
int intervalsX, intervalsY, intervalsZ;
// Количество интервалов по х на процесс
int tasksPerProcess;

struct Task
{
	double *oldData, *newData;
	int blockNumber;
	int localNumber;
	int tasks_x;
	int flag = 1, firstStart = 1, existRecv[6] = { 1,1,1,1,1,1 };
	//	LeftX, RightX, LowY, UpY, LowZ, UpZ;
	std::array<std::vector<double>, 6> borders;
	std::array <MPI_Request, 6>  sendReq, recvReq;
	std::array<int, 6> neighbors;
	std::array<std::vector <int>, 6> shadowBorders;
	std::vector <double> oldU, newU, F;
	std::vector <Point> points;
	std::vector <int> numbersOfKU;	
	bool BelongToShadowBorders(int node);
	bool BelongToKU(int node);
	void Calculate1Node(int i);
	void Run();
};
std::vector<Task> t;

// Функция принадлежности узла node теневой границе
bool Task::BelongToShadowBorders(int node)
{
	for (int i = 0; i < shadowBorders.size(); i++)
		for (int j = 0; j < shadowBorders[i].size(); j++)
			if (shadowBorders[i][j] == node) return true;
	return false;
}

// Функция принадлежности узла node краевым условиям
bool Task::BelongToKU(int node)
{
	for (int i = 0; i < numbersOfKU.size(); i++)
		if (numbersOfKU[i] == node) return true;
	return false;
}

void Task::Calculate1Node(int i)
{
	double result;
	// Смещения, для расчёта соседей по y и по z
	int offsetY = tasks_x + 1, offsetZ = (tasks_x+ 1)*(tasks_y[localNumber % k_y] + 1);

	// Если узел принадлежит к границе, то накладываются первые краевые условия
	if (BelongToKU(i))	
		newData[i] = F[i];
	else {
		result = (oldData[i - 1] + oldData[i + 1]) / pow(hx, 2) +
			(oldData[i - offsetY] + oldData[i + offsetY]) / pow(hy, 2) +
			(oldData[i - offsetZ] + oldData[i + offsetZ]) / pow(hz, 2) - F[i];
		result *= pow(hx, 2)*pow(hy, 2)*pow(hz, 2) / 2 / (pow(hy, 2)*pow(hz, 2) +
			pow(hz, 2)*pow(hx, 2) + pow(hx, 2)*pow(hy, 2));
		newData[i] = result;
	}
}

void Task::Run()
{
	
	MPI_Status st;
	if (flag == 1) {
		oldData = oldU.data();
		newData = newU.data();
	}
	else {
		oldData = newU.data();
		newData = oldU.data();
	}
	// Рассчёт новых границ их передача и принятие границ от других блоков
	for (int i = 0; i < shadowBorders.size(); i++)
		if (neighbors[i] != -1) {
			if (firstStart == 1) {
				borders[i].resize(shadowBorders[i].size());
				for (auto &el : borders[i]) el = 1;
			}
			int id, map_id;
			// Смещения относительно  теневых границ
			switch (i) {
			case 0: id = 2; map_id = 1; break;
			case 1: id = -2;  map_id = 0; break;
			case 2: id = 2 * (tasks_x + 1); map_id = 3; break;
			case 3: id = -2 * (tasks_x + 1); map_id = 2; break;
			case 4: id = 2 * (tasks_x + 1) * (tasks_y[localNumber % k_y] + 1); map_id = 5; break;
			case 5: id = -2 * (tasks_x + 1) * (tasks_y[localNumber % k_y] + 1); map_id = 4; break;
			}

			/*int flag_test = 0;
			if (existRecv[i] != 1) {	
				while (!flag_test) {	
					MPI_Irecv(borders[i].data(), borders[i].size(), MPI_DOUBLE, oldMap[neighbors[i]], blockNumber * 6 + i, MPI_COMM_WORLD, &recvReq[i]);
					MPI_Test(&recvReq[i], &flag_test, &st);
				}
			}*/
			if (existRecv[i] != 1)
				MPI_Recv(borders[i].data(), borders[i].size(), MPI_DOUBLE, oldMap[neighbors[i]], blockNumber * 6 + i, MPI_COMM_WORLD, &st);
			for (int j = 0; j < shadowBorders[i].size(); j++) {
				if (!BelongToShadowBorders(shadowBorders[i][j] + id)) {
					oldData[shadowBorders[i][j]] = borders[i][j];
					Calculate1Node(shadowBorders[i][j] + id);
					borders[i][j] = newData[shadowBorders[i][j] + id];
				}
			}

			MPI_Isend(borders[i].data(), borders[i].size(), MPI_DOUBLE, firstMap[neighbors[i]], neighbors[i] * 6 + map_id, MPI_COMM_WORLD, &sendReq[i]);
			if (existRecv[i] == 1) existRecv[i] = 0;
		}
		if (firstStart == 1) firstStart = 0;
	
	// Проходимся по сетке
	for (int i = 0; i < oldU.size(); i++)
		if (!BelongToShadowBorders(i))
		{
			Calculate1Node(i);
		}
	int id;
	for (int i = 0; i < newU.size(); i++)
	{
		bool flag_ = false;
		
		for (int j = 0; j < shadowBorders.size() / 2; j++)
		{
			switch (2 * j + 1) {
			case 1: id = 1; break;
			case 3: id = (tasks_x + 1); break;
			case 5: id = (tasks_x + 1) * (tasks_y[localNumber % k_y] + 1); break;
			}
			for (int k = 0; k < shadowBorders[2 * j + 1].size(); k++)
				if (shadowBorders[2 * j + 1][k] - id == i ) { flag_ = true; break; }
			if (flag_) break;
		}
		if (!BelongToShadowBorders(i) && !flag_)
		{
			newResult[points[i].globalNumber] = newData[i];
			oldResult[points[i].globalNumber] = oldData[i];
		}
	}
	if (flag == 0)	flag = 1;
	else flag = 0;
}
std::vector<Task*> allTasks;

// Учёт первых краевых условий
double CalcF1BC(double x, double y, double z)
{
	return 2;
}

// Правая часть уравнения Пуассона
double CalcF(double x, double y, double z)
{
	return 0;
}

int CalculateNumberBeg(int residue, int &taskPerIterval, int rank_, int end)
{
	int number_beg;
	if (residue != 0 && rank_ < residue)
		taskPerIterval++;
	// Если процесс не входит в область задач с лишними подзадачами
	if (residue != 0 && residue <= rank_)
	{
		// Смещаем по области задач с лишними подзадачами
		number_beg = residue * (taskPerIterval + 1);
		// -//- по остальным
		number_beg += (rank_ - residue)*taskPerIterval;
	}
	else // Если входит, то количество задач на процесс совпадает с предыдущими
	{
		number_beg = rank_ * taskPerIterval;
	}

	//	Нам нужны теневые границы
	// Для первого и последнего процесса +1 теневая грань
	if ((rank_ == 0 || rank_ == end))
	{
		if (rank_ == end) number_beg--;
		taskPerIterval++;
	}
	else // для остальных +2
	{
		taskPerIterval += 2;
		number_beg--;
	}
	return number_beg;
}

void GenerateQueueOfTask()
{	
	int          len[5] = { 1,1,1,1,1 };
	MPI_Aint     pos[5] = { offsetof(Point,x), offsetof(Point,y),	offsetof(Point,z), offsetof(Point,globalNumber), sizeof(Point) };
	MPI_Datatype typ[5] = { MPI_DOUBLE,MPI_DOUBLE,MPI_DOUBLE,MPI_INT,MPI_UB };

	MPI_Type_struct(4, len, pos, typ, &MPI_POINT);
	MPI_Type_commit(&MPI_POINT);
	

	double	l;
	double x, y, z;
	std::vector<int> globalNumbersOfKU;
	// Бьём задачи по координате х
	l = abs(endX - begX);	intervalsX = l / hx;
	tasksPerProcess = intervalsX / size;
	// Если количество интервалов не кратно числу процессов
	// то распределяем оставшиеся задачи по первым процессам
	int residue = intervalsX % size;

	l = abs(endY - begY);	intervalsY = l / hy;
	l = abs(endZ - begZ);	intervalsZ = l / hz;
	dim = (intervalsX + 1) * (intervalsY + 1) * (intervalsZ + 1);

	// Формирование глобальных номеров краевых условий
	for (int i = 0; i < intervalsZ + 1; i++)
		for (int j = 0; j < intervalsY + 1; j++)
			for (int k = 0; k < intervalsX + 1; k++)
				if (i == 0 || i == intervalsZ || j == 0 || j == intervalsY || k == 0 || k == intervalsX)
					globalNumbersOfKU.push_back(k + (intervalsX + 1)*j + (intervalsX + 1)*(intervalsY + 1)*i);


	// Глобальный номер первого элемента сетки
	int number_beg_x = 0, firstX = begX;
	if (size != 1)
	{
		number_beg_x = CalculateNumberBeg(residue, tasksPerProcess, rank, size - 1);
		firstX += number_beg_x * hx;
	}

	int number_beg_z = 0, number_beg_y = 0;
	//if (rank == 1) printf("%d:: number_beg_x = %d\n", rank, number_beg_x);
	// Начало и конец области с теневыми
	k_y = intervalsY / sizeBlockY, k_z = intervalsZ / sizeBlockZ;
	int r_y = intervalsY % sizeBlockY, r_z = intervalsZ % sizeBlockZ;
	t.resize(k_y*k_z);
	tasks_z.resize(k_z);
	tasks_y.resize(k_y);
	for (int i_z = 0; i_z < k_z; i_z++)
	{
		// количество интервалов
		tasks_z[i_z] = sizeBlockZ;
		number_beg_z = CalculateNumberBeg(r_z, tasks_z[i_z], i_z, k_z - 1);
		z = begZ + number_beg_z * hz;
		// по количеству точек
		for (int i = 0; i < tasks_z[i_z] + 1; i++)
		{
			for (int j_y = 0; j_y < k_y; j_y++)
			{
				tasks_y[j_y] = sizeBlockY;
				number_beg_y = CalculateNumberBeg(r_y, tasks_y[j_y], j_y, k_y - 1);
				y = begY + number_beg_y * hy;
				int idBlock = i_z * k_y + j_y;
				//if (rank == 0) printf ("id_block %d k_y = %d, k_z = %d\n",idBlock,k_y,k_z );
				for (int j = 0; j < tasks_y[j_y] + 1; j++)
				{
					x = firstX;

					for (int k = 0; k < tasksPerProcess + 1; k++)
					{
						Point p;

						int number = number_beg_x + k + (intervalsX + 1)*(number_beg_y + j) +
							(intervalsX + 1)*(intervalsY + 1)*(number_beg_z + i);
						p.set(x, y, z, number);
						t[idBlock].points.push_back(p);
						t[idBlock].oldU.push_back(1);
						bool ku = false;
						for (int z = 0; z < globalNumbersOfKU.size(); z++)
							if (globalNumbersOfKU[z] == number) ku = true;
						// Расчёт правых частей
						if (ku) {
							t[idBlock].F.push_back(CalcF1BC(x, y, z));
							// Локальные адреса краевых условий
							t[idBlock].numbersOfKU.push_back(t[idBlock].oldU.size() - 1);
						}
						else t[idBlock].F.push_back(CalcF(x, y, z));

						// Локальнные адреса теневых границ
						if (k == 0 && rank != 0)
							t[idBlock].shadowBorders[0].push_back(t[idBlock].oldU.size() - 1);
						else if (k == tasksPerProcess && rank != size - 1)
							t[idBlock].shadowBorders[1].push_back(t[idBlock].oldU.size() - 1);

						if (j == 0 && y != begY)
							t[idBlock].shadowBorders[2].push_back(t[idBlock].oldU.size() - 1);
						else if (j == tasks_y[j_y] && y != endY)
							t[idBlock].shadowBorders[3].push_back(t[idBlock].oldU.size() - 1);

						if (i == 0 && z != begZ)
							t[idBlock].shadowBorders[4].push_back(t[idBlock].oldU.size() - 1);
						else if (i == tasks_z[i_z] && z != endZ)
							t[idBlock].shadowBorders[5].push_back(t[idBlock].oldU.size() - 1);
						x += hx;
					}
					y += hy;
				}
			}
			z += hz;
		}
	}
	std::vector<int> tmp_map(t.size()*size);
	oldMap.resize(t.size()*size);
	for (int i = 0; i < t.size(); i++)
	{
		t[i].newU.resize(t[i].oldU.size());
		t[i].blockNumber = rank + size * i;
		t[i].localNumber = i;
		t[i].tasks_x = tasksPerProcess;
		tmp_map[t[i].blockNumber] = rank;
		// Первоначальное расположение теневых границ в узлах
		if (rank) {
			t[i].neighbors[0] = t[i].blockNumber - 1;
		}
		else {
			t[i].neighbors[0] = -1;
		}
		if (rank != size - 1) {
			t[i].neighbors[1] = t[i].blockNumber + 1;
		}
		else {
			t[i].neighbors[1] = -1;
		}
		if (t[i].shadowBorders[2].size()) {
			t[i].neighbors[2] = t[i].blockNumber - size;
		}
		else {
			t[i].neighbors[2] = -1;
		}
		if (t[i].shadowBorders[3].size()) {
			t[i].neighbors[3] = t[i].blockNumber + size;
		}
		else {
			t[i].neighbors[3] = -1;
		}

		if (t[i].shadowBorders[4].size()) {
			t[i].neighbors[4] = t[i].blockNumber - size * k_y;
		}
		else {
			t[i].neighbors[4] = -1;
		}
		if (t[i].shadowBorders[5].size()) {
			t[i].neighbors[5] = t[i].blockNumber + size * k_y;
		}
		else {
			t[i].neighbors[5] = -1;
		}
		allTasks.push_back(&t[i]);
	}

	newResult.resize(dim);
	oldResult.resize(dim);
	globalRes.resize(dim);
	globalOldRes.resize(dim);
	firstMap.resize(oldMap.size());
	newMap.resize(oldMap.size());
	for(int i = 0; i<oldMap.size(); i++)
		oldMap[i] = 0;

	MPI_Allreduce(tmp_map.data(), oldMap.data(), oldMap.size(), MPI_INT, MPI_SUM, MPI_COMM_WORLD);
	firstMap = newMap = oldMap;
}

void GenerateSend(Task *t, int peer)
{
	MPI_Status st;
	// Проверка на то, были ли уже отправлены данные этой задаче на текущее расположение 
	if (t->firstStart != 1)
		for (int i = 0; i < 6; i++)
			if (t->neighbors[i] != -1) {
				int flag_test = 0;
				if (t->existRecv[i] != 1) {	
					MPI_Irecv(t->borders[i].data(), t->borders[i].size(), MPI_DOUBLE, oldMap[t->neighbors[i]], t->blockNumber * 6 + i, MPI_COMM_WORLD, &t->recvReq[i]);
					MPI_Test(&t->recvReq[i], &flag_test, &st);
				}
				if (flag_test) t->existRecv[i] = 1;						
			}

				
	// Отправляем параметры задачи

	MPI_Send(t->neighbors.data(), 6, MPI_INT, peer, 1018, MPI_COMM_WORLD);

	MPI_Send(&t->blockNumber, 1, MPI_INT, peer, 1019, MPI_COMM_WORLD);
	MPI_Send(&t->tasks_x, 1, MPI_INT, peer, 1020, MPI_COMM_WORLD);
	MPI_Send(&t->localNumber, 1, MPI_INT, peer, 1021, MPI_COMM_WORLD);

	MPI_Send(&t->flag, 1, MPI_INT, peer, 1022, MPI_COMM_WORLD); 
	MPI_Send(&t->existRecv, 6, MPI_INT, peer, 1023, MPI_COMM_WORLD);
	MPI_Send(&t->firstStart, 1, MPI_INT, peer, 1024, MPI_COMM_WORLD);

	int sizes[14];
	for (int j = 0; j < 6; j++)
		sizes[j] = t->borders[j].size();
	for (int j = 6; j < 12; j++) 
		sizes[j] = t->shadowBorders[j-6].size();
	sizes[12] = t->oldU.size();
	sizes[13] = t->numbersOfKU.size();
			
	MPI_Send(&sizes, 14, MPI_INT, peer, 1000, MPI_COMM_WORLD);

	// Отправляем данные
	for (int j = 0; j < 6; j++)
		MPI_Send(t->borders[j].data(), t->borders[j].size(), MPI_DOUBLE, peer, 1001 + j, MPI_COMM_WORLD);
	for (int j = 0; j < 6; j++)
		MPI_Send(t->shadowBorders[j].data(), t->shadowBorders[j].size(), MPI_INT, peer, 1007 + j, MPI_COMM_WORLD);
		
	MPI_Send(t->oldU.data(), 1, MPI_DOUBLE, peer, 1013, MPI_COMM_WORLD);
	MPI_Send(t->newU.data(), 1, MPI_DOUBLE, peer, 1014, MPI_COMM_WORLD);
	MPI_Send(t->F.data(), 1, MPI_DOUBLE, peer, 1015, MPI_COMM_WORLD);
	MPI_Send(t->points.data(), t->points.size(), MPI_POINT, peer, 1016, MPI_COMM_WORLD);
	MPI_Send(t->numbersOfKU.data(), t->numbersOfKU.size(), MPI_INT, peer, 1017, MPI_COMM_WORLD);
}

void GenerateRecv(Task *t, int i)
{
	MPI_Status st;

	MPI_Recv(t->neighbors.data(), 6, MPI_INT, i, 1018, MPI_COMM_WORLD, &st);

	MPI_Recv(&t->blockNumber, 1, MPI_INT, i, 1019, MPI_COMM_WORLD, &st);
	MPI_Recv(&t->tasks_x, 1, MPI_INT, i, 1020, MPI_COMM_WORLD, &st);
	MPI_Recv(&t->localNumber, 1, MPI_INT, i, 1021, MPI_COMM_WORLD, &st);

	MPI_Recv(&t->flag, 1, MPI_INT, i, 1022, MPI_COMM_WORLD, &st); 
	MPI_Recv(&t->existRecv, 6, MPI_INT, i, 1023, MPI_COMM_WORLD, &st);
	MPI_Recv(&t->firstStart, 1, MPI_INT, i, 1024, MPI_COMM_WORLD, &st);

	// Cначала получаем массив размеров и выделяем память под задачу
	int sizes[14];
	MPI_Recv(&sizes, 14, MPI_INT, i, 1000, MPI_COMM_WORLD, &st);				
		
	printf("%d:: get task start %d from %d\n", rank, t->blockNumber, i);
	/*for (int i = 0; i < 14; i++)
		printf("%d::%d::%d\t",rank, i, sizes[i]);
	printf("\n");*/
	// Получаем данные
	for (int j = 0; j < 6; j++) {
		t->borders[j].resize(sizes[j]);
		printf("B %d::::task %d:: %d::%d\n",rank, t->blockNumber, j, sizes[j]);
		MPI_Recv(t->borders[j].data(), t->borders[j].size(), MPI_DOUBLE, i, 1001 + j, MPI_COMM_WORLD, &st);
	}
	for (int j = 0; j < 6; j++) {
		t->shadowBorders[j].resize(sizes[j + 6]);
		printf("S %d::task %d:: %d::%d\n",rank, t->blockNumber, j + 6, sizes[j + 6]);
		MPI_Recv(t->shadowBorders[j].data(), t->shadowBorders[j].size(), MPI_INT, i, 1007 + j, MPI_COMM_WORLD, &st);
	}

	t->oldU.resize(sizes[12]);
	t->newU.resize(t->oldU.size());
	t->F.resize(t->oldU.size());
	t->points.resize(t->oldU.size());
	t->numbersOfKU.resize(sizes[13]);

	MPI_Recv(t->oldU.data(), 1, MPI_DOUBLE, i, 1013, MPI_COMM_WORLD, &st);
	MPI_Recv(t->newU.data(), 1, MPI_DOUBLE, i, 1014, MPI_COMM_WORLD, &st);
	MPI_Recv(t->F.data(), 1, MPI_DOUBLE, i, 1015, MPI_COMM_WORLD, &st);
	MPI_Recv(t->points.data(), t->points.size(), MPI_POINT, i, 1016, MPI_COMM_WORLD, &st);
	MPI_Recv(t->numbersOfKU.data(), t->numbersOfKU.size(), MPI_INT, i, 1017, MPI_COMM_WORLD, &st);
	
	printf("%d:: get task %d from %d\n", rank, t->blockNumber, i);
}

