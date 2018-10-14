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
typedef double myType;
int countOfConnect = 2;
// Максимальное кол-во итераций
int maxiter = 10000;
double eps = 1e-8;
double residual = 1;
int dim;
int size, rank;
// Шаги сетки

double	hx = 0.1,
	hy = 0.1,
	hz = 0.1;
	/*hx = 0.25,
	hy = 0.25,
	hz = 0.25;*/

	/*hx = 0.5,
	hy = 0.5,
	hz = 0.5;*/

int countOfBlockY = 4,
countOfBlockZ = 4;
std:: vector <int> map;
// Координаты начала и конца области
double	begX = 0, endX = 2,
begY = 0, endY = 2,
begZ = 0, endZ = 2;
MPI_Datatype MPI_POINT;

struct Point
{
	myType x, y, z;
	int globalNumber;

	void set(myType x1, myType y1, myType z1, int glN)
	{
		x = x1;
		y = y1;
		z = z1;
		globalNumber = glN;
	};
};
std::vector<myType> oldResult, newResult;
std::vector <myType> globalRes;
std::vector <myType> globalOldRes;
int iteration = 0;
// Количество интервалов по координате
int intervalsX, intervalsY, intervalsZ;

struct Task
{
	double *oldData, *newData;
	int blockNumber;
	int localNumber;
	int tasks_x, tasks_y;
	int flag = 1;
	//	LeftX, RightX, LowY, UpY, LowZ, UpZ;
	std::array<std::vector<myType>, 6> bordersSend, bordersRecv;
	std::array <MPI_Request, 6>  sendReq, recvReq;
	std::array<int, 6> neighbors;
	std::array<std::vector <int>, 6> shadowBorders;
	std::vector <myType> oldU, newU, F;
	std::vector <Point> points;
	std::vector <int> numbersOfKU;	
	bool BelongToShadowBorders(int node);
	bool BelongToKU(int node);
	void Calculate1Node(int i);
	void Calculate1NodeOfBorders(int node);
	void ReceiveFromNeighbors(MPI_Comm Comm);
	void SendToNeighbors(MPI_Comm Comm);
	void WaitBorders();
	void Run();
	~Task();
};

std::vector<Task> t;

Task::~Task()
{ 
 	for (int i = 0; i < 6; i++) {
		bordersRecv[i].clear();
		bordersSend[i].clear();
		shadowBorders[i].clear();
	}
	oldU.clear();
	newU.clear();
	F.clear();
	points.clear();
	numbersOfKU.clear();
}

// Функция принадлежности узла node теневой границе
bool Task::BelongToShadowBorders(int node)
{
	for (int i = 0; i < 6; i++)
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

void Task::ReceiveFromNeighbors(MPI_Comm Comm)
{
	MPI_Status st;
	for (int i = 0; i < 6; i++)
		if (neighbors[i] != -1)
			MPI_Recv(bordersRecv[i].data(), bordersRecv[i].size(), MPI_DOUBLE, MPI_ANY_SOURCE, blockNumber * 6 + i, Comm, &st);
}

void Task::SendToNeighbors(MPI_Comm Comm)
{	int map_id;	
	for (int i = 0; i < 6; i++) {	
		switch (i) {
			case 0: map_id = 1; break;
			case 1: map_id = 0; break;
			case 2: map_id = 3; break;
			case 3: map_id = 2; break;
			case 4: map_id = 5; break;
			case 5: map_id = 4; break;
		}
		if (neighbors[i] != -1)
			MPI_Isend(bordersSend[i].data(), bordersSend[i].size(), MPI_DOUBLE, map[neighbors[i]], neighbors[i] * 6 + map_id, Comm, &sendReq[i]);
	}
}
void Task::WaitBorders()
{	
	MPI_Status st;
	for (int i = 0; i < 6; i++) {	
		if (neighbors[i] != -1)
			MPI_Wait(&sendReq[i], &st);
	}
}

void Task::Calculate1Node(int i)
{
	// Смещения, для расчёта соседей по y и по z
	int offsetY = tasks_x + 1, offsetZ = (tasks_x + 1)*(tasks_y + 1);
	double 	hx_2 = pow(hx, 2), hy_2 = pow(hy, 2), hz_2 = pow(hz, 2); 
	
	// Если узел принадлежит к границе, то накладываются первые краевые условия
	if (BelongToKU(i)) newData[i] = F[i];
	else {
		double x_left, x_right, y_low, y_up, z_low, z_up;
		
		for (int l = 0; l < shadowBorders.size(); l++) {
			bool flag = false;	
			int k;
			for (k = 0; k < shadowBorders[l].size(); k++)
				if (shadowBorders[l][k] == i) { 
					flag = true; break;
				}
			
			switch (l) {
				case 0: {
					if (flag) x_left = bordersRecv[l][k]; 
					else x_left = oldData[i - 1];
				} break;
				case 1: {
					if (flag) x_right = bordersRecv[l][k]; 
					else x_right = oldData[i + 1];
				} break;
				case 2: {
					if (flag) y_low = bordersRecv[l][k];
					else y_low = oldData[i - offsetY];
				} break;
				case 3: {
					if (flag) y_up = bordersRecv[l][k];  
					else y_up = oldData[i + offsetY];
				} break;
				case 4: {
					if (flag) z_low = bordersRecv[l][k]; 
					else z_low = oldData[i - offsetZ]; 
				} break;
				case 5:	{
					if (flag) z_up = bordersRecv[l][k];
					else z_up = oldData[i + offsetZ]; 
				} break;
			}	
		}

		double result = (x_left + x_right) / hx_2 + (y_low + y_up) / hy_2 + (z_low + z_up) / hz_2 - F[i];
		result *= hx_2 * hy_2 * hz_2 / 2 / (hy_2 * hz_2 + hz_2 * hx_2 + hx_2 * hy_2);
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

	std::array<std::vector<double>,6> tmp;
	// Проходимся по сетке
	for (int i = 0; i < oldU.size(); i++)
			Calculate1Node(i);
	
	for (int i = 0; i < shadowBorders.size(); i++)
		if (neighbors[i] != -1) {
			int id;
			// Смещения относительно  теневых границ
			switch (i) {
			case 0: id = 1; break;
			case 1: id = -1; break;
			case 2: id = (tasks_x + 1);  break;
			case 3: id = -1 * (tasks_x + 1); break;
			case 4: id = (tasks_x + 1) * (tasks_y + 1);  break;
			case 5: id = -1 * (tasks_x + 1) * (tasks_y + 1); break;
			}
			tmp[i].resize(bordersSend[i].size());
			for (int j = 0; j < shadowBorders[i].size(); j++) {				
					tmp[i][j] = newData[shadowBorders[i][j] + id];
			}
		}
	
	int id;
	for (int i = 0; i < newU.size(); i++)
	{
		bool flag_ = false;
		for (int j = 0; j < shadowBorders.size() / 2; j++)
		{
			for (int k = 0; k < shadowBorders[2 * j + 1].size(); k++)
				if (shadowBorders[2 * j + 1][k] == i ) { flag_ = true; break; }
			if (flag_) break;
		}
		if (!flag_) {
			newResult[points[i].globalNumber] = newData[i];
			oldResult[points[i].globalNumber] = oldData[i];
		}
	}	
	bordersSend = tmp;
	if (flag == 0)	flag = 1;
	else flag = 0;
}
std::queue<Task*> allTasks;

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

int CalculateNumberBeg(int residue, int &taskPerIterval, int rank_)
{
	int number_beg;
	if (residue != 0 && rank_ < residue) taskPerIterval++;
	// Если процесс не входит в область задач с лишними подзадачами
	if (residue != 0 && residue <= rank_) {
		// Смещаем по области задач с лишними подзадачами
		number_beg = residue * (taskPerIterval + 1);
		// -//- по остальным
		number_beg += (rank_ - residue)*taskPerIterval;
	}
	else // Если входит, то количество задач на процесс совпадает с предыдущими
		number_beg = rank_ * taskPerIterval;

	return number_beg;
}

void GenerateBasicConcepts()
{
	int          len[5] = { 1,1,1,1,1 };
	MPI_Aint     pos[5] = { offsetof(Point,x), offsetof(Point,y), offsetof(Point,z), offsetof(Point,globalNumber), sizeof(Point) };
	MPI_Datatype typ[5] = { MPI_DOUBLE,MPI_DOUBLE,MPI_DOUBLE,MPI_INT,MPI_UB };

	MPI_Type_struct(4, len, pos, typ, &MPI_POINT);
	MPI_Type_commit(&MPI_POINT);
	

	double	l;
	// Бьём задачи по координате х
	l = abs(endX - begX);	intervalsX = l / hx;
	l = abs(endY - begY);	intervalsY = l / hy;
	l = abs(endZ - begZ);	intervalsZ = l / hz;
	dim = (intervalsX + 1) * (intervalsY + 1) * (intervalsZ + 1);
	newResult.resize(dim);
	oldResult.resize(dim);
	globalRes.resize(dim);
	globalOldRes.resize(dim);
}

void GenerateQueueOfTask()
{	
	double	l;
	double x, y, z;
	std::vector<int> globalNumbersOfKU;
	// Бьём задачи по координате х
	int tasksPerProcess = intervalsX / size;
	// Если количество интервалов не кратно числу процессов
	// то распределяем оставшиеся задачи по первым процессам
	int residue = intervalsX % size;
	
	// Формирование глобальных номеров краевых условий
	for (int i = 0; i < intervalsZ + 1; i++)
		for (int j = 0; j < intervalsY + 1; j++)
			for (int k = 0; k < intervalsX + 1; k++)
				if (i == 0 || i == intervalsZ || j == 0 || j == intervalsY || k == 0 || k == intervalsX)
					globalNumbersOfKU.push_back(k + (intervalsX + 1)*j + (intervalsX + 1)*(intervalsY + 1)*i);

	// Глобальный номер первого элемента сетки
	int number_beg_x = 0, firstX = begX;
	if (size != 1)	{
		number_beg_x = CalculateNumberBeg(residue, tasksPerProcess, rank);
		firstX += number_beg_x * hx;
	}

	int number_beg_z = 0, number_beg_y = 0;
	
	// Количество интервалов в одном блоке
	int k_y = intervalsY / countOfBlockY, k_z = intervalsZ / countOfBlockZ;
	int r_y = intervalsY % countOfBlockY, r_z = intervalsZ % countOfBlockZ;
	fprintf (stderr,"k_y = %d, k_z = %d, r_y = %d, r_z = %d\n ", k_y,k_z,r_y,r_z);
	t.resize(countOfBlockY * countOfBlockZ);
	map.resize(countOfBlockY * countOfBlockZ * size);	

	std:: vector <int> tasks_y, tasks_z;
	tasks_y.resize(countOfBlockY);  tasks_z.resize(countOfBlockZ);
	for (int i_z = 0; i_z < countOfBlockZ; i_z++)
	{
		// количество интервалов
		tasks_z[i_z] = k_z;
		number_beg_z = CalculateNumberBeg(r_z, tasks_z[i_z], i_z);		
		z = begZ + number_beg_z * hz;
		// по количеству точек
		for (int i = 0; i < tasks_z[i_z] + 1; i++)
		{
			for (int j_y = 0; j_y < countOfBlockY; j_y++)
			{
				tasks_y[j_y] = k_y;
				number_beg_y = CalculateNumberBeg(r_y, tasks_y[j_y], j_y);
				y = begY + number_beg_y * hy;
				int idBlock = i_z * countOfBlockY + j_y;
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
						x = firstX + hx*(k+1);
					}
					y = begY + number_beg_y * hy + hy*(j+1);
				}
			}
			z = begZ + number_beg_z * hz + hz*(i+1);
		}
	}

	for (int i = 0; i < t.size(); i++)
	{
		t[i].newU.resize(t[i].oldU.size());
		t[i].blockNumber = rank + size * i;
		t[i].localNumber = i;
		t[i].tasks_x = tasksPerProcess;
		t[i].tasks_y = 	tasks_y[i % countOfBlockY];
		
		map[t[i].blockNumber] = rank;
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
			t[i].neighbors[4] = t[i].blockNumber - size * countOfBlockY;
		}
		else {
			t[i].neighbors[4] = -1;
		}
		if (t[i].shadowBorders[5].size()) {
			t[i].neighbors[5] = t[i].blockNumber + size * countOfBlockY;
		}
		else {
			t[i].neighbors[5] = -1;
		}
		for (int j = 0; j < t[i].shadowBorders.size(); j++)
			if (t[i].neighbors[j] != -1) {
				t[i].bordersSend[j].resize(t[i].shadowBorders[j].size());
				for (auto &el : t[i].bordersSend[j]) el = 1;
				t[i].bordersRecv[j].resize(t[i].shadowBorders[j].size());
				for (auto &el : t[i].bordersSend[j]) el = 1;
			}
		allTasks.push(&t[i]);
		
	}
	
}

void GenerateSend(Task *t, int peer, MPI_Comm Comm)
{
	MPI_Status st;

	// Отправляем параметры задачи
	MPI_Send(t->neighbors.data(), 6, MPI_INT, peer, 1018, Comm);
	MPI_Send(&t->blockNumber, 1, MPI_INT, peer, 1019, Comm);
	MPI_Send(&t->tasks_x, 1, MPI_INT, peer, 1020, Comm);
	MPI_Send(&t->localNumber, 1, MPI_INT, peer, 1021, Comm);
	MPI_Send(&t->flag, 1, MPI_INT, peer, 1022, Comm); 
	MPI_Send(&t->tasks_y, 1, MPI_INT, peer, 1023, Comm);

	int sizes[14];
	for (int j = 0; j < 6; j++)
		sizes[j] = t->bordersSend[j].size();
	for (int j = 0; j < 6; j++) 
		sizes[j+6] = t->shadowBorders[j].size();
	sizes[12] = t->oldU.size();
	sizes[13] = t->numbersOfKU.size();
			
	MPI_Send(&sizes, 14, MPI_INT, peer, 1000, Comm);

	// Отправляем данные
	for (int j = 0; j < 6; j++) {
		MPI_Send(t->bordersSend[j].data(), sizes[j], MPI_DOUBLE, peer, 1001 + j, Comm);
		MPI_Send(t->bordersRecv[j].data(), sizes[j], MPI_DOUBLE, peer, 1001 + j, Comm);
	}

	for (int j = 0; j < 6; j++)
		MPI_Send(t->shadowBorders[j].data(), sizes[j + 6], MPI_INT, peer, 1007 + j, Comm);
		
	MPI_Send(t->oldU.data(), t->oldU.size(), MPI_DOUBLE, peer, 1013, Comm);
	MPI_Send(t->newU.data(), t->newU.size(), MPI_DOUBLE, peer, 1014, Comm);
	MPI_Send(t->F.data(), t->F.size(), MPI_DOUBLE, peer, 1015, Comm);
	MPI_Send(t->points.data(), t->points.size(), MPI_POINT, peer, 1016, Comm);
	MPI_Send(t->numbersOfKU.data(), t->numbersOfKU.size(), MPI_INT, peer, 1017, Comm);
	//t->~Task();
}

void GenerateRecv(Task *t, int i, MPI_Comm Comm)
{
	MPI_Status st;

	MPI_Recv(t->neighbors.data(), 6, MPI_INT, i, 1018, Comm, &st);
	MPI_Recv(&t->blockNumber, 1, MPI_INT, i, 1019, Comm, &st);
	MPI_Recv(&t->tasks_x, 1, MPI_INT, i, 1020, Comm, &st);
	MPI_Recv(&t->localNumber, 1, MPI_INT, i, 1021, Comm, &st);
	MPI_Recv(&t->flag, 1, MPI_INT, i, 1022, Comm, &st); 
	MPI_Recv(&t->tasks_y, 1, MPI_INT, i, 1023, Comm, &st);

	// Cначала получаем массив размеров и выделяем память под задачу
	int sizes[14];
	MPI_Recv(&sizes, 14, MPI_INT, i, 1000, Comm, &st);				
		
	// Получаем данные
	for (int j = 0; j < 6; j++) {
		t->bordersSend[j].resize(sizes[j]);
		t->bordersRecv[j].resize(sizes[j]);
		MPI_Recv(t->bordersSend[j].data(), sizes[j], MPI_DOUBLE, i, 1001 + j, Comm, &st);	
		MPI_Recv(t->bordersRecv[j].data(), sizes[j], MPI_DOUBLE, i, 1001 + j, Comm, &st);	
	}
	for (int j = 0; j < 6; j++) {
		t->shadowBorders[j].resize(sizes[j + 6]);
		MPI_Recv(t->shadowBorders[j].data(), sizes[j + 6], MPI_INT, i, 1007 + j, Comm, &st);
	}

	t->oldU.resize(sizes[12]);
	t->newU.resize(t->oldU.size());
	t->F.resize(t->oldU.size());
	t->points.resize(t->oldU.size());
	t->numbersOfKU.resize(sizes[13]);

	MPI_Recv(t->oldU.data(), t->oldU.size(), MPI_DOUBLE, i, 1013, Comm, &st);
	MPI_Recv(t->newU.data(), t->newU.size(), MPI_DOUBLE, i, 1014, Comm, &st);
	MPI_Recv(t->F.data(), t->F.size(), MPI_DOUBLE, i, 1015, Comm, &st);
	MPI_Recv(t->points.data(), t->points.size(), MPI_POINT, i, 1016, Comm, &st);
	MPI_Recv(t->numbersOfKU.data(), t->numbersOfKU.size(), MPI_INT, i, 1017, Comm, &st);
	
	fprintf(stderr,"%d:: get task %d from %d\n", rank, t->blockNumber, i);
}

void GenerateResult(MPI_Comm Comm)
{
	std::vector <double> res(size), globalR(size);
	std::vector <int> iterations(size), resIterations(size);
	iterations[rank] = iteration;
	res[rank] = residual;	
	MPI_Allreduce(newResult.data(), globalRes.data(), newResult.size(), MPI_DOUBLE, MPI_SUM, Comm);
	MPI_Reduce(iterations.data(), resIterations.data(), resIterations.size(), MPI_INT, MPI_SUM, 0, Comm);
	MPI_Reduce(res.data(), globalR.data(), res.size(), MPI_DOUBLE, MPI_SUM, 0, Comm);
	
	
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
		for (int i = 0; i < size; i++)
			printf("rank %d::\tcountOfIter = %d;\tresidual = %e\n", i, resIterations[i], globalR[i]);
		printf("\n--------------------------------------------------------------------\n");
	}
}
void GenerateResultOfIteration(MPI_Comm reduceComm)
{
	double sum = 0;
	// Расчёт абсолютной погрешности между текущим и предыдущим решениями
	for (int i = 0; i < newResult.size(); i++)
		sum += (newResult[i] - oldResult[i])*(newResult[i] - oldResult[i]);
	
	MPI_Allreduce(&sum, &residual, 1, MPI_DOUBLE, MPI_SUM, reduceComm);
	residual = sqrt(residual);
}

bool CheckConditions()
{
	if (residual > eps) return true;
	else return false;
}
