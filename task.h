#include "library.h"
struct Point {
	double x, y, z;
	int globalNumber;
	void set(double x1, double y1, double z1, int glN) {
		x = x1;
		y = y1;
		z = z1;
		globalNumber = glN;
	};
};
class Task : public ITask {
public:
	double *oldData, *newData;
	int localNumber;
	int tasks_x, tasks_y;
	int flag = 1;
	//	LeftX, RightX, LowY, UpY, LowZ, UpZ;
	std::array<std::vector<double>, 6> bordersSend, bordersRecv;
	std::array <MPI_Request, 6>  sendReq, recvReq;
	std::array<int, 6> neighbors;
	std::array<std::vector <int>, 6> shadowBorders;
	std::vector <double> oldU, newU, F;
	std::vector <Point> points;
	std::vector <int> numbersOfKU;
	bool BelongToShadowBorders(int node);
	bool BelongToKU(int node);
	void Calculate1Node(int i);
	void ReceiveFromNeighbors(MPI_Comm Comm);
	void SendToNeighbors(MPI_Comm Comm);
	void WaitBorders();
	void Run();
	void Clear();
	void GenerateRecv(int sender, MPI_Comm Comm);
	void GenerateSend(int reciever, MPI_Comm Comm);
	~Task();
};