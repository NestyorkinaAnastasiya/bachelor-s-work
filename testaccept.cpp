#include <iostream>
#include <mpi.h>
#include <fstream>

int main(int argc, char** argv)
{
	MPI_Init(&argc, &argv);
	int rank, size;
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm client;
	MPI_Status status;
	char port_name[MPI_MAX_PORT_NAME];
	
	if (rank == 0)
	{
		MPI_Open_port(MPI_INFO_NULL, port_name);
		std::ofstream fPort("port_name.txt");
		for (int i = 0; i < MPI_MAX_PORT_NAME; i++)
			fPort << port_name[i];
		fPort.close();
                printf("server available at %s\n", port_name);
	}
	std::cerr << "Rank " << rank << "\n";
	MPI_Comm_accept((rank)?NULL:port_name, MPI_INFO_NULL, 0, MPI_COMM_WORLD, &client);

	MPI_Finalize();
	return 0;
}

