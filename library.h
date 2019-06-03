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
class Library {
	static pthread_t thrs[11];
	// id for threads
	static int ids[11];
	// descriptors for threads
	static int changeComm;
	static bool server_new;
	static int condition;
	// Count of computational threads
	static int countOfWorkers;
	// Count of all threads
	static int countOfThreads;
	static bool STOP;
	static bool startWork;

	static pthread_mutexattr_t attr_set_task, attr_get_task;
	static pthread_attr_t attrs_dispatcher, attrs_server, attrs_mapController, attrs_workers;
	static pthread_cond_t server_cond, comunicator_cond;
	static pthread_mutex_t server_mutexcond, comunicator_mutexcond;

	static void* dispatcher_old(void* me);
	static void* dispatcher(void* me);
	static void* worker(void* me);
	static void* mapController(void* me);
	static void* server(void *me);
	static bool GetTask(ITask **currTask) {
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
	static int GetRank(int &sign, int &k, int countOfProcess);
	static void SendTask(MPI_Status &st, MPI_Comm &CommWorker, MPI_Comm &CommMap);
	static void ExecuteOwnTasks();
	static void ExecuteOtherTask(MPI_Comm &Comm, int id, bool &retry);
	static void ChangeCommunicator(MPI_Comm &Comm, int &newSize);
public:
	static int numberOfConnection;
	static pthread_mutex_t mutex_get_task, mutex_set_task;
	static int rank, size;
	static int rank_old, size_old;
	static std::vector<int> map;
	static int countOfConnect;
	static bool changeExist;
	static std::queue<ITask*> currentTasks, queueRecv, sendedTasks;
	// Communicators
	static MPI_Comm currentComm;
	static MPI_Comm oldComm, newComm, serverComm, reduceComm;
	static void LibraryInitialize(int argc, char **argv, bool clientProgram);
	static void CreateLibraryComponents();
	static void StartWork();
	static void CloseLibraryComponents();
};
int Library::countOfConnect = 2;
MPI_Comm Library::currentComm = MPI_COMM_WORLD;
MPI_Comm Library::oldComm = MPI_COMM_WORLD;
MPI_Comm Library::newComm = MPI_COMM_WORLD;
MPI_Comm Library::serverComm = MPI_COMM_WORLD;
MPI_Comm Library::reduceComm = MPI_COMM_WORLD;
// id for threads
int Library::ids[11] = { 0,1,2,3,4,5,6,7,8,9,10 };
// descriptors for threads
int Library::changeComm = false;
bool Library::server_new = false;
int Library::condition = 0;
// Count of computational threads
int Library::countOfWorkers = 1;
// Count of all threads
int Library::countOfThreads = 3;

bool Library::STOP = false;
bool Library::startWork = false;
bool Library::changeExist = false;
int Library::numberOfConnection = 0;