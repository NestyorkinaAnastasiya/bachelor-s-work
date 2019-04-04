#include "balance.cpp"
std::ofstream fTime;
#include <sstream>
void FindSolution() {
        pthread_attr_t attrs;
        if (0 != pthread_attr_init(&attrs)) {
                perror("Cannot initialize attributes");
                abort();
        };
        if (0 != pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_JOINABLE)) {
                perror("Error in setting attributes");
                abort();
        }
        double sum;
        MPI_Request s;
        MPI_Status st;
        int exit = -1;
        // Create dispatcher
        if (size != 0)
                if (0 != pthread_create(&thrs[countOfWorkers], &attrs, dispatcher, &ids[countOfWorkers])) {
                        perror("Cannot create a thread");
                        abort();
                }
        // Create mapController
        if (0 != pthread_create(&thrs[countOfWorkers+1], &attrs, mapController, &ids[countOfWorkers+1])) {
                perror("Cannot create a thread");
                abort();
        }
        MPI_Comm_dup(currentComm, &serverComm);
        MPI_Comm_dup(currentComm, &reduceComm);
        MPI_Comm_dup(currentComm, &barrierComm);
        // Create server
        if (0 != pthread_create(&thrs[countOfWorkers+2], &attrs, server, &ids[countOfWorkers+2])) {
                perror("Cannot create a thread");
                abort();
        }
        std::vector<int> flags(size);
        std::vector<int> globalFlags(size);
        std::stringstream ss;
        ss << rank;
        std::string nameFile = "Loading" + ss.str();
        nameFile += ".txt";
        std::ofstream fLoading(nameFile);

        for (iteration = 0; iteration < maxiter && CheckConditions(); iteration++) {
                if (rank == 0) printf("%d::  --------------------START ITERATION %d---------------------\n", rank, iteration);
                for (auto &i : newResult) i = 0;
                for (auto &i : oldResult) i = 0;
                auto t_start = std::chrono::high_resolution_clock::now();
                while (!allTasks.empty()) {
                        Task *t = allTasks.front();
                        if(iteration != 0) t->ReceiveFromNeighbors(currentComm);
                        queueRecv.push(t);
                        allTasks.pop();
                }

                while (!queueRecv.empty()) {
                        Task *t = queueRecv.front();
                        if(iteration != 0) t->WaitBorders();
                        pthread_mutex_lock(&mutex_get_task); //?? can speed up??
                        currentTasks.push(t);
                        pthread_mutex_unlock(&mutex_get_task);
                        queueRecv.pop();
                }

                fprintf(stderr, "%d:: count of tasks = %d\n", rank, currentTasks.size());

                // Create computational threads
                for (int i = 0; i < countOfWorkers; i++)
                        if (0 != pthread_create(&thrs[i], &attrs, worker, &ids[i])) {
                                perror("Cannot create a thread");
                                abort();
                        }

                // Wait computational threads
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
                // If communicator was changed, we need close the old communicator
                if (change) {
                        fprintf(stderr,"%d:: get to change communicator\n", rank);
                        int cond = 4;
                        // Close the dispatcher working in old communicator
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
                        while (!changeComm);  // RECV
                        currentComm = newComm;
                        flags.resize(size);
                        globalFlags.resize(size);
                        changeComm = false;
                        server_new = false;//SEND
                        if (rank == 0){
                                time ( &rawtime );
                                timeinfo = localtime ( &rawtime );
                                strftime (buffer,80,"%H:%M:%S",timeinfo);
                                puts (buffer);
                                fTime << "new connection in " << buffer << "\n";
                        }

                }
                fprintf(stderr,"%d::get to generate result of iteration\n", rank);
                GenerateResultOfIteration(reduceComm);

                while (!queueRecv.empty()) {
                        Task *t = queueRecv.front();
                        t->SendToNeighbors(currentComm);
                        queueRecv.pop();
                        allTasks.push(t);

                }
                auto t_end = std::chrono::high_resolution_clock::now();
                if (rank == 0) {
                        printf("%d:: res = %e\n",rank, residual);
                        printf("%d:: --------------------FINISH ITERATION %d---------------------\n", rank, iteration);

                }
                if (rank == 0){
                        fTime << "iteration " << iteration << "::  " << std::chrono::duration<double, std::milli>(t_end - t_start).count() << " ms\n";
                }
                fLoading << "iteration " << iteration << "::  " << allTasks.size() << "\ttasks\n";
        }

        MPI_Send(&exit, 1, MPI_INT, rank, 1030, currentComm);
        MPI_Send(&exit, 1, MPI_INT, rank, 2001, currentComm);
        pthread_join(thrs[countOfWorkers], NULL);
        fprintf(stderr,"%d::dispetcher close\n", rank);

        // If ranks connected after all computing
        while (numberOfConnection < countOfConnect) {
                int cond, size_new;
                MPI_Recv(&cond, 1, MPI_INT, rank, 2001, currentComm, &st);
                if (rank == 0){
                        size_old = size;
                        MPI_Comm_size(newComm, &size_new);

                        cond = 0;
                        for(int k = size_old; k < size_new; k++)
                                MPI_Send(&cond, 1, MPI_INT, k, 10000, newComm);
                }
                server_new = false;
        }

        pthread_join(thrs[countOfWorkers+1], NULL);
        fprintf(stderr,"%d::mapController close\n", rank);
        pthread_join(thrs[countOfWorkers+2], NULL);
        fprintf(stderr,"%d::server close\n", rank);
        fLoading.close();
        pthread_attr_destroy(&attrs);
}

int main(int argc, char **argv) {
        if (rank == 0) {
                fTime.open("time_server.txt");
                time ( &rawtime );
                timeinfo = localtime ( &rawtime );
                strftime (buffer,80,"%H:%M:%S",timeinfo);
                puts (buffer);
                fTime << "servers's processes start in " << buffer << "\n";
        }

        int provided = MPI_THREAD_SINGLE;
        MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
        if (provided != MPI_THREAD_MULTIPLE) {
                std::cerr << "not MPI_THREAD_MULTIPLE";
                exit(0);
        }

        MPI_Comm_rank(currentComm, &rank);
        MPI_Comm_size(currentComm, &size);

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
        fTime.close();
        return 0;
}
