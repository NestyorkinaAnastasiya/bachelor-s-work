#include "lib_map.cpp"
void* server(void *me) {
	MPI_Comm client;
	MPI_Status st;
	int cond;
	char port_name[MPI_MAX_PORT_NAME];
	int old_size, new_size;

	// Open port
	if (rank == 0) {
		MPI_Open_port(MPI_INFO_NULL, port_name);
		std::ofstream fPort("port_name.txt");
		for (int i = 0; i < MPI_MAX_PORT_NAME; i++)
			fPort << port_name[i];
		fPort.close();
	}
	for (; numberOfConnection < countOfConnect; ) {
		// The previous connection must be finished
		if (numberOfConnection) 
			MPI_Recv(&cond, 1, MPI_INT, rank, 1998, newComm, &st);
		old_size = size;

		// Waiting for new ranks
		MPI_Comm_accept(port_name, MPI_INFO_NULL, 0, serverComm, &client);
		oldComm = currentComm;
		// Creating new communicator for joint ranks group
		MPI_Intercomm_merge(client, false, &newComm);
		server_new = true;
		MPI_Comm_size(newComm, &new_size);
		MPI_Request req;
		int message = 1;
		numberOfConnection++;
		// send to new ranks information about connections count
		if (rank == 0)
			for (int k = old_size; k < new_size; k++)
				MPI_Send(&numberOfConnection, 1, MPI_INT, k, 10002, newComm);

		// Send to dispatcher message about new communicator
		MPI_Send(&message, 1, MPI_INT, rank, 2001, currentComm);
		int k = pthread_cond_wait(&server_cond, &server_mutexcond);
	}
	return 0;
}