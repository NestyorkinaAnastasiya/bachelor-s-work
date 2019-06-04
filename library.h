#pragma once
#define HAVE_STRUCT_TIMESPEC
#include <pthread.h>
#define MSMPI_NO_DEPRECATE_20
#include <mpi.h>
#include <time.h>
#include <chrono>
#include <queue>
#include <vector>
#include <array>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <ctime>
#include <math.h>
#include <stddef.h>
#include <sstream>
#define MAX_DATA 1000
class ITask {
public:
	int blockNumber;
	void virtual Run() = 0;
	void virtual Clear() = 0;
	void virtual GenerateRecv(int sender, MPI_Comm Comm) = 0;
	void virtual GenerateSend(int reciever, MPI_Comm Comm) = 0;
};

pthread_t thrs[11];
// id for threads
int ids[11] = { 0,1,2,3,4,5,6,7,8,9,10 };
// descriptors for threads
int changeComm = false;
bool server_new = false;
// Count of computational threads
int countOfWorkers = 1;
// Count of all threads
int countOfThreads = 3;

pthread_mutexattr_t attr_set_task, attr_get_task;
pthread_attr_t attrs_dispatcher, attrs_server, attrs_mapController, attrs_workers;
pthread_cond_t server_cond, comunicator_cond;
pthread_mutex_t server_mutexcond, comunicator_mutexcond;

bool GetTask(ITask **currTask) {
		pthread_mutex_lock(&mutex_get_task);
		if (currentTasks.empty()) {
			pthread_mutex_unlock(&mutex_get_task);
			return false;
		}
		else {
			*currTask = currentTasks.front();
			currentTasks.pop();
		}
		pthread_mutex_unlock(&mutex_get_task);
		return true;
	}

int GetRank(int &sign, int &k, int countOfProcess);
void SendTask(MPI_Status &st, MPI_Comm &CommWorker, MPI_Comm &CommMap);
void ExecuteOwnTasks();
void ExecuteOtherTask(MPI_Comm &Comm, int id, bool &retry);
void ChangeCommunicator(MPI_Comm &Comm, int &newSize);

int numberOfConnection = 0;
pthread_mutex_t mutex_get_task, mutex_set_task;
int rank, size;
int rank_old, size_old;
std::vector<int> map;
int countOfConnect = 2;
bool changeExist = false;
std::queue<ITask*> currentTasks, queueRecv, sendedTasks;
// Communicators
static MPI_Comm currentComm = oldComm = newComm = 
	serverComm = reduceComm	= MPI_COMM_WORLD;
void LibraryInitialize(int argc, char **argv, bool clientProgram);
void CreateLibraryComponents();
void StartWork();
void CloseLibraryComponents();
void* dispatcher_old(void* me);
void* dispatcher(void* me);
void* worker(void* me);
void* mapController(void* me);
void* server(void *me);