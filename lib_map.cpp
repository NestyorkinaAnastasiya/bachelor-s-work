#include "lib_dispatchers.cpp"
void* mapController(void* me) {
	fprintf(stderr, "%d:: map controller run.\n", rank);
	MPI_Comm Comm = currentComm;
	MPI_Status st;
	bool close = false;
	int map_id, rank_id;
	while (!close) {
		MPI_Recv(&map_id, 1, MPI_INT, MPI_ANY_SOURCE, 1030, Comm, &st);
		// Task place was changed	
		if (map_id >= 0) {
			int peer = st.MPI_SOURCE;
			MPI_Recv(&rank_id, 1, MPI_INT, peer, 1031, Comm, &st);
			map[map_id] = rank_id;
			fprintf(stderr, "%d:: task %d pass to %d\n", rank, map_id, rank_id);
		}
		// Close mapController
		else if (map_id == -1) close = true;
		// Communicator changing 
		else if (map_id == -10) Comm = newComm;
	}
	return 0;
}