//#include <windows.h>

#define WIN32_LEAN_AND_MEAN
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <process.h>
#include <SDL.h>
#include <GL\GL.h>
#include <GL\GLU.h>
#include <math.h>
#include <mutex>

#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_PORT "27015"
#define DEFAULT_BUFFER_LENGTH 1024
#define DEFAULT_R 17.0f
#define M_PI           3.14159265358979323846

//GLfloat	rtri = 0.1;
//float rotate[3];
//int rflag[3];
//float currentOrientationQuaternion[4] = { 0.044037, -0.006511, 0.3906, -0.919457 };
//float currentOrientationQuaternion[4] = { -0.048929, 0.005697, -0.011549, -0.998712 };
float currentOrientationQuaternion[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
float gravity[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
//float rotate[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
float pos[3] = { 0.0f, 0.0f, -DEFAULT_R };
std::mutex lock;
std::mutex lock2;
//float max = 0.0f;
//float x_pos = 0.0f;
//float y_pos = 0.0f;
//float z_pos = 0.0f;
//float x_pos_guess = 0.0f;
//int flag = 0;

long long milliseconds_now() {
	static LARGE_INTEGER s_frequency;
	static BOOL s_use_qpc = QueryPerformanceFrequency(&s_frequency);
	if (s_use_qpc) {
		LARGE_INTEGER now;
		QueryPerformanceCounter(&now);
		return (1000LL * now.QuadPart) / s_frequency.QuadPart;
	}
	else {
		return GetTickCount();
	}
}

void multiplyByQuat(float* q1, float* q2, float* out) {
	out[3] = (q1[3] * q2[3] - q1[0] * q2[0] - q1[1] * q2[1] - q1[2]
		* q2[2]); //w = w1w2 - x1x2 - y1y2 - z1z2
	out[0] = (q1[3] * q2[0] + q1[0] * q2[3] + q1[1] * q2[2] - q1[2]
		* q2[1]); //x = w1x2 + x1w2 + y1z2 - z1y2
	out[1] = (q1[3] * q2[1] + q1[1] * q2[3] + q1[2] * q2[0] - q1[0]
		* q2[2]); //y = w1y2 + y1w2 + z1x2 - x1z2
	out[2] = (q1[3] * q2[2] + q1[2] * q2[3] + q1[0] * q2[1] - q1[1]
		* q2[0]); //z = w1z2 + z1w2 + x1y2 - y1x2
}

void rotateVec3(float* points, float* vec, float* temp){
	temp[0] = vec[0] * (1 - 2 * points[1] * points[1] - 2 * points[2] * points[2]) + vec[1] * 2 * (points[0] * points[1] + points[3] * points[2]) + vec[2] * 2 * (points[0] * points[2] - points[3] * points[1]);
	temp[1] = vec[1] * (1 - 2 * points[0] * points[0] - 2 * points[2] * points[2]) + vec[0] * 2 * (points[0] * points[1] - points[3] * points[2]) + vec[2] * 2 * (points[1] * points[2] + points[3] * points[0]);
	temp[2] = vec[2] * (1 - 2 * points[0] * points[0] - 2 * points[1] * points[1]) + vec[0] * 2 * (points[0] * points[2] + points[3] * points[1]) + vec[1] * 2 * (points[1] * points[2] - points[3] * points[0]);
}

float angle2vec(float* vec1, float* vec2){
	float dotp = vec1[0] * vec2[0] + vec1[1] * vec2[1] + vec1[2] * vec2[2];
	float abs1 = sqrt(vec1[0] * vec1[0] + vec1[1] * vec1[1] + vec1[2] * vec1[2]);
	float abs2 = sqrt(vec2[0] * vec2[0] + vec2[1] * vec2[1] + vec2[2] * vec2[2]);
	//printf("%f %f %f ", dotp, abs1, abs2);
	if (dotp >= abs1 * abs2){
		return acos(1.0f);
	}
	if (dotp <= 0.0f - abs1 * abs2){
		return acos(-1.0f);
	}
	return acos(dotp / (abs1 * abs2));
}

void inverse(float* points) {
	float d = points[0] * points[0] + points[1] * points[1] + points[2] * points[2] + points[3] * points[3];
	points[0] /= d;
	points[0] = 0.0f - points[0];
	points[1] /= d;
	points[1] = 0.0f - points[1];
	points[2] /= d;
	points[2] = 0.0f - points[2];
	points[3] /= d;
}

void socket(void *param) {

	WSADATA wsaData;

	// Initialize Winsock
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0)
	{
		printf("WSAStartup failed: %d\n", iResult);
		return;
	}

	struct addrinfo	*result = NULL,
		hints;

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;		// Internet address family is unspecified so that either an IPv6 or IPv4 address can be returned
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;
	hints.ai_flags = AI_PASSIVE;

	// Resolve the local address and port to be used by the server
	iResult = getaddrinfo("0.0.0.0", DEFAULT_PORT, &hints, &result);
	if (iResult != 0)
	{
		printf("getaddrinfo failed: %d\n", iResult);
		WSACleanup();
		return;
	}

	SOCKET ListenSocket = INVALID_SOCKET;

	// Create a SOCKET for the server to listen for client connections
	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	int i = TRUE;
	if (ListenSocket == INVALID_SOCKET)
	{
		printf("Error at socket(): %d\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return;
	}

	iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);

	char ipAddress[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &(result->ai_addr), ipAddress, INET_ADDRSTRLEN);
	printf("The IP address is: %s\n", ipAddress);

	if (iResult == SOCKET_ERROR)
	{
		printf("bind failed: %d", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(ListenSocket);
		WSACleanup();
		return;
	}

	freeaddrinfo(result);


	SOCKET ClientSocket;

	ClientSocket = INVALID_SOCKET;

	// Accept a client socket
	int client_length = (int)sizeof(struct sockaddr_in); struct sockaddr_in client;
	ClientSocket = accept(ListenSocket, (struct sockaddr *)&client, &client_length);

	long long negative_okay_time_x = 0;
	long long negative_okay_time_y = 0;
	long long last_time = 0;
	long long last_angle_time = 0;
	long long last_last_angle_time = 0;
	int flag_x = 0;
	int flag_y = 0;
	int init_flag_x = 0;
	int init_flag_y = 0;
	int next_flag = 1;
	int counter_x = 0;
	int counter_y = 0;
	int counter_x2 = 0;
	int counter_y2 = 0;
	int counter1 = 1;
	int counter2 = 1;
	//float tail_x = 0.0f;
	//float tail_y = 0.0f;
	//float acc_max = 0.0f;
	//float angle_max = 0.0f;
	float speed_x = 0.0f;
	float dis_x = 0.0f;
	float dis_y = 0.0f;
	float angle_dis_x = 0.0f;
	float angle_dis_y = 0.0f;
	float last_angle_dis_x = 0.0f;
	float last_angle_dis_y = 0.0f;
	float radius = DEFAULT_R;
	float ratio_x = 0.0f;
	float ratio_x_max = 0.0f;
	float ratio_y_max = 0.0f;
	float ratio_y = 0.0f;
	float acc_x_sum = 0.0f;
	float acc_y_sum = 0.0f;
	float angle_x_sum = 0.0f;
	float angle_y_sum = 0.0f;
	float last_ori[4];
	float init_ori[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	float init_ori_x[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	float init_ori_y[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	float mov[3] = { 0.0f, 0.0f, 0.0f };
	float temp_pos[3] = { 0.0f, 0.0f, -DEFAULT_R };
	float temp_pos_x[3] = { 0.0f, 0.0f, -DEFAULT_R };
	float temp_pos_y[3] = { 0.0f, 0.0f, -DEFAULT_R };
	float last_mov[3] = { 0.0f, 0.0f, 0.0f };
	float center[3] = { 0.0f, 0.0f, 0.0f };
	//float des[3] = { 0.0f, 0.0f, -DEFAULT_R };
	//int flag_x = 0;
	//int division = 0;
	//int counter = 0;
	//int action_flag_x = 0;
	//float last_x = 0.0f;
	char recvbuf[DEFAULT_BUFFER_LENGTH];
	char scan_buf[DEFAULT_BUFFER_LENGTH];
	int end = 0;
	long long last = 0;
	float NS2S = 1.0f / 1000000000.0f;
	double gyroscopeRotationVelocity = 0;
	double EPSILON = 0.1f;
	float deltaQuaternion[16];
	FILE* op;
	fopen_s(&op, "op.txt", "w");
	do {
		iResult = recvfrom(ListenSocket, recvbuf, DEFAULT_BUFFER_LENGTH - 1, 0, (struct sockaddr *)&client, &client_length);
		if (iResult > 0)
		{
			recvbuf[iResult] = '\0';
			int i = 0;
			while (i < iResult){
				if (end < 999){
					scan_buf[end] = recvbuf[i];
					scan_buf[end + 1] = '\0';
				}
				if (scan_buf[end] == '\n'){
					end = -1;
					int a;
					int type;
					float x, y, z, w;
					long long time = 0;
					sscanf_s(scan_buf, "%d ", &type);
					if (type == 0){
						sscanf_s(scan_buf, "%*d %lld %f %f %f %f %f %f %f %f\n", &time, &x, &y, &z, &w, &gravity[0], &gravity[1], &gravity[2], &gravity[3]);
						float temp_y = gravity[1];
						gravity[1] = gravity[2];
						gravity[2] = 0.0f - temp_y;
						//printf_s("%d %lld %f %f %f %f %f %f %f %f\n", type, time, x, y, z, w, gravity[0], gravity[1], gravity[2], gravity[3]);
						last_ori[0] = currentOrientationQuaternion[0];
						last_ori[1] = currentOrientationQuaternion[1];
						last_ori[2] = currentOrientationQuaternion[2];
						last_ori[3] = currentOrientationQuaternion[3];
						lock.lock();
						currentOrientationQuaternion[0] = x;
						currentOrientationQuaternion[1] = z;
						currentOrientationQuaternion[2] = 0.0f - y;
						currentOrientationQuaternion[3] = w;
						lock.unlock();
						float temp[3];
						float last_pos[3];
						float curr_pos[3];
						if (last_angle_time && last_time != 0){
							float r = 7.0f;
							temp[0] = 0.0f;
							temp[1] = 0.0f;
							temp[2] = 0.0f - r;
							rotateVec3(last_ori, temp, last_pos);
							rotateVec3(currentOrientationQuaternion, temp, curr_pos);
							float angle_dis_x_temp = sqrt((curr_pos[0] - last_pos[0]) * (curr_pos[0] - last_pos[0]) + (curr_pos[2] - last_pos[2]) * (curr_pos[2] - last_pos[2]));
							angle_dis_x_temp /= (time - last_angle_time) / 1000000000.0f;
							float angle_dis_y_temp = curr_pos[1] - last_pos[1];
							angle_dis_y_temp /= (time - last_angle_time) / 1000000000.0f;
							float curr_pos_temp[3];
							float last_pos_temp[3];
							curr_pos_temp[0] = (r / sqrt(curr_pos[0] * curr_pos[0] + curr_pos[2] * curr_pos[2])) * curr_pos[0];
							curr_pos_temp[2] = (r / sqrt(curr_pos[0] * curr_pos[0] + curr_pos[2] * curr_pos[2])) * curr_pos[2];
							last_pos_temp[0] = (r / sqrt(last_pos[0] * last_pos[0] + last_pos[2] * last_pos[2])) * last_pos[0];
							last_pos_temp[2] = (r / sqrt(last_pos[0] * last_pos[0] + last_pos[2] * last_pos[2])) * last_pos[2];
							if ((curr_pos_temp[0] >= last_pos_temp[0] && last_pos_temp[2] >= center[2] && curr_pos_temp[2] >= center[2]) || (curr_pos_temp[0] < last_pos_temp[0] && last_pos_temp[2] < center[2] && curr_pos_temp[2] < center[2]) || (curr_pos_temp[2] < last_pos_temp[2] && last_pos_temp[0] >= center[0] && curr_pos_temp[0] >= center[0]) || (curr_pos_temp[2] >= last_pos_temp[2] && last_pos_temp[0] < center[0] && curr_pos_temp[0] < center[0])){
								angle_dis_x = angle_dis_x * 0.3 - 0.7 * angle_dis_x_temp;
							}
							else{
								angle_dis_x = angle_dis_x * 0.3 + 0.7 * angle_dis_x_temp;
							}
							angle_dis_y = angle_dis_y * 0.3 + 0.7 * angle_dis_y_temp;
							//printf("%f %f\n", curr_pos[1], angle_dis_y);
						}
						last_angle_time = time;
					}
					if (type == 1){
						last_mov[0] = mov[0];
						last_mov[1] = mov[1];
						last_mov[2] = mov[2];
						sscanf_s(scan_buf, "%*d %lld %f %f %f\n", &time, &x, &y, &z);
						//fprintf_s(op,"%d %lld %f %f %f\n", type, time, x, y, z);
						float diff = (time - last_time) / 1000000000.0f;
						mov[0] = x;
						mov[1] = z;
						mov[2] = -y;
						float temp_mov[3];
						temp_mov[0] = mov[0];
						temp_mov[1] = mov[1];
						temp_mov[2] = mov[2];
						rotateVec3(gravity, temp_mov, mov);
						/*
						float temp_mov[3];
						temp_mov[0] = 0.0f;
						temp_mov[1] = 0.0f;
						temp_mov[2] = 0.0f;
						float temp1[3];
						float temp2[3];
						float temp3[3];
						float temp4[3];
						temp3[0] = 10.0f;
						temp3[1] = 0.0f;
						temp3[2] = 0.0f;
						rotateVec3(currentOrientationQuaternion, temp3, temp4);
						temp4[0] = sqrt(temp2[0] * temp2[0] + temp2[2] * temp2[2]);
						temp4[1] = 0.0f;
						temp1[0] = mov[0];
						temp1[1] = 0.0f;
						temp1[2] = 0.0f;
						rotateVec3(currentOrientationQuaternion, temp1, temp2);
						if (temp2[0] >= 0.0f){
							temp_mov[0] += sqrt(temp2[0] * temp2[0] + temp2[2] * temp2[2]);
						}
						else{
							temp_mov[0] -= sqrt(temp2[0] * temp2[0] + temp2[2] * temp2[2]);
						}
						temp_mov[1] += temp2[1];
						temp1[0] = 0.0f;
						temp1[1] = mov[1];
						temp1[2] = 0.0f;
						rotateVec3(currentOrientationQuaternion, temp1, temp2);
						if (mov[1] < 0.0f){
							temp_mov[0] += sqrt(temp2[0] * temp2[0] + temp2[2] * temp2[2]) * (1.0f - temp4[0] / temp3[0]);
						}
						else{
							temp_mov[0] -= sqrt(temp2[0] * temp2[0] + temp2[2] * temp2[2]) * (1.0f - temp4[0] / temp3[0]);
						}
						temp_mov[1] += temp2[1];
						if (mov[1] >= 0.0f){
							temp_mov[2] += sqrt(temp2[0] * temp2[0] + temp2[2] * temp2[2]) * (temp4[0] / temp3[0]);
						}
						else{
							temp_mov[2] -= sqrt(temp2[0] * temp2[0] + temp2[2] * temp2[2]) * (temp4[0] / temp3[0]);
						}
						//temp_mov[2] += temp2[2];
						temp1[0] = 0.0f;
						temp1[1] = 0.0f;
						temp1[2] = mov[2];
						rotateVec3(currentOrientationQuaternion, temp1, temp2);
						if (temp2[2] >= 0.0f){
							temp_mov[2] += sqrt(temp2[0] * temp2[0] + temp2[2] * temp2[2]);
						}
						else{
							temp_mov[2] -= sqrt(temp2[0] * temp2[0] + temp2[2] * temp2[2]);
						}
						temp_mov[1] += temp2[1];
						mov[0] = temp_mov[0];
						mov[1] = temp_mov[1];
						mov[2] = temp_mov[2];
						*/
						if (last_angle_time == last_last_angle_time){
							next_flag = 0;
						}
						else{
							next_flag = 1;
						}
						last_last_angle_time = last_angle_time;
						if (last_time != 0 && next_flag == 1){
							if (angle_dis_x < 0.0f && init_flag_x != 0 && init_flag_x != 2 && init_flag_x != 4){
								counter_x2++;
								/*
								if (counter_x2 > 200){
									init_flag_x = 2;
									counter_x = 0;
									counter_x2 = 0;
								}
								*/
							}
							if (angle_dis_x >= 0.0f && init_flag_x != 0 && init_flag_x != 1 && init_flag_x != 3){
								counter_x2++;
								/*
								if (counter_x2 > 200){
									init_flag_x = 1;
									counter_x = 0;
									counter_x2 = 0;
								}
								*/
							}
							if (init_flag_x == 1 && angle_dis_x < last_angle_dis_x){
								init_flag_x = 3;
								counter_x = 0;
							}
							if (init_flag_x == 3 && angle_dis_x >= last_angle_dis_x){
								counter_x++;
								if (counter_x > 2){
									init_flag_x = 1;
								}
							}
							if (init_flag_x == 2 && angle_dis_x >= last_angle_dis_x){
								init_flag_x = 4;
								counter_x = 0;
							}
							if (init_flag_x == 4 && angle_dis_x < last_angle_dis_x){
								counter_x++;
								if (counter_x > 2){
									init_flag_x = 2;
								}
							}

							if (angle_dis_y < 0.0f && init_flag_y != 0 && init_flag_y != 2 && init_flag_y != 4){
								counter_y2++;
								if (counter_y2 > 4){
									init_flag_y = 2;
									counter_y = 0;
									counter_y2 = 0;
								}
							}
							if (angle_dis_y >= 0.0f && init_flag_y != 0 && init_flag_y != 1 && init_flag_y != 3){
								counter_y2++;
								if (counter_y2 > 4){
									init_flag_y = 1;
									counter_y = 0;
									counter_y2 = 0;
								}
							}
							if (init_flag_y == 1 && angle_dis_y < last_angle_dis_y){
								init_flag_y = 3;
								counter_y = 0;
							}
							if (init_flag_y == 3 && angle_dis_y >= last_angle_dis_y){
								counter_y++;
								if (counter_y > 2){
									init_flag_y = 1;
								}
							}
							if (init_flag_y == 2 && angle_dis_y >= last_angle_dis_y){
								init_flag_y = 4;
								counter_y = 0;
							}
							if (init_flag_y == 4 && angle_dis_y < last_angle_dis_y){
								counter_y++;
								if (counter_y > 2){
									init_flag_y = 2;
								}
							}

							/*
							if ((dis_x >= 0.0f && dis_x + mov[0] < 0.0f) || (dis_x < 0.0f && dis_x + mov[0] >= 0.0f)){
								tail_x = mov[0] * ((time - last_time) / 5000000);
							}
							*/

							if (flag_x == 1 && ((dis_x >= 0.0f && dis_x + mov[0] >= 0.0f) || (dis_x < 0.0f && dis_x + mov[0] < 0.0f) || init_flag_x == 1 || init_flag_x == 2)){
								dis_x += mov[0] * ((time - last_time) / 5000000);
							}
							if (flag_x == 0){
								dis_x += mov[0] * ((time - last_time) / 5000000);
								flag_x = 1;
								if (angle_dis_x >= last_angle_dis_x){
									init_flag_x = 1;
								}
								else{
									init_flag_x = 2;
								}
								counter_x = 0;
								counter_x2 = 0;
							}

							if (flag_y == 1 && ((dis_y >= 0.0f && dis_y + mov[1] >= 0.0f) || (dis_y < 0.0f && dis_y + mov[1] < 0.0f) || init_flag_y == 1 || init_flag_y == 2)){
								dis_y += mov[1] * ((time - last_time) / 5000000);
							}
							if (flag_y == 0){
								dis_y += mov[1] * ((time - last_time) / 5000000);
								flag_y = 1;
								if (angle_dis_y >= last_angle_dis_y){
									init_flag_y = 1;
								}
								else{
									init_flag_y = 2;
								}
								counter_y = 0;
								counter_y2 = 0;
							}

							if ((angle_dis_x <= 1.0f && angle_dis_x >= -1.0f) || (angle_dis_x >= 0.0f && last_angle_dis_x < 0.0f) || (angle_dis_x < 0.0f && last_angle_dis_x >= 0.0f)){
								dis_x = mov[0] * ((time - last_time) / 5000000);
								flag_x = 0;
								//tail_x = 0;
								init_flag_x = 0;
								counter_x = 0;
								counter_x2 = 0;
								ratio_x = 0.0f;
								acc_x_sum = 0.0f;
								angle_x_sum = 0.0f;
								//acc_max = 0.0f;
								//angle_max = 0.0f;
								ratio_x_max = 0.0f;
								counter1 = 1;

								/*
								float temp[3];
								temp[0] = 0.0f;
								temp[1] = 0.0f;
								temp[2] = 0.0f - radius;
								float last_pos[3];
								float curr_pos[3];
								rotateVec3(init_ori_x, temp, last_pos);
								rotateVec3(currentOrientationQuaternion, temp, curr_pos);
								lock2.lock();
								pos[0] = temp_pos_x[0] + curr_pos[0] - last_pos[0];
								pos[1] = temp_pos_x[1] + curr_pos[1] - last_pos[1];
								pos[2] = temp_pos_x[2] + curr_pos[2] - last_pos[2];
								lock2.unlock();
								*/
								temp_pos_x[0] = pos[0];
								temp_pos_x[1] = pos[1];
								temp_pos_x[2] = pos[2];
								init_ori_x[0] = currentOrientationQuaternion[0];
								init_ori_x[1] = currentOrientationQuaternion[1];
								init_ori_x[2] = currentOrientationQuaternion[2];
								init_ori_x[3] = currentOrientationQuaternion[3];

							}
							if ((angle_dis_y <= 1.0f && angle_dis_y >= -1.0f) || (angle_dis_y >= 0.0f && last_angle_dis_y < 0.0f) || (angle_dis_y < 0.0f && last_angle_dis_y >= 0.0f)){
								dis_y = mov[1] * ((time - last_time) / 5000000);
								flag_y = 0;
								//tail_y = 0;
								init_flag_y = 0;
								counter_y = 0;
								counter_y2 = 0;
								ratio_y = 0.0f;
								acc_y_sum = 0.0f;
								angle_y_sum = 0.0f;
								counter2 = 1;
								ratio_y_max = 0.0f;
								//acc_max = 0.0f;
								//angle_max = 0.0f;
								
								temp_pos_y[0] = pos[0];
								temp_pos_y[1] = pos[1];
								temp_pos_y[2] = pos[2];
								init_ori_y[0] = currentOrientationQuaternion[0];
								init_ori_y[1] = currentOrientationQuaternion[1];
								init_ori_y[2] = currentOrientationQuaternion[2];
								init_ori_y[3] = currentOrientationQuaternion[3];
								
							}
							if (((angle_dis_y <= 1.0f && angle_dis_y >= -1.0f) || (angle_dis_y >= 0.0f && last_angle_dis_y < 0.0f) || (angle_dis_y < 0.0f && last_angle_dis_y >= 0.0f)) || ((angle_dis_x <= 1.0f && angle_dis_x >= -1.0f) || (angle_dis_x >= 0.0f && last_angle_dis_x < 0.0f) || (angle_dis_x < 0.0f && last_angle_dis_x >= 0.0f))){
								temp_pos[0] = pos[0];
								temp_pos[1] = pos[1];
								temp_pos[2] = pos[2];
								init_ori[0] = currentOrientationQuaternion[0];
								init_ori[1] = currentOrientationQuaternion[1];
								init_ori[2] = currentOrientationQuaternion[2];
								init_ori[3] = currentOrientationQuaternion[3];
							}

							acc_x_sum += dis_x / counter1;// -(1 + counter1 / 2) * tail_x;
							angle_x_sum += angle_dis_x / counter1;
							if (init_flag_x == 1 || init_flag_x == 2){
								if (angle_x_sum != 0){
									ratio_x = acc_x_sum / angle_x_sum;
								}
							}
							acc_y_sum += dis_y / counter2;// - (1 + counter2 / 2) * tail_y;
							angle_y_sum += angle_dis_y / counter2;
							if (init_flag_y == 1 || init_flag_y == 2){
								if (angle_y_sum != 0){
									ratio_y = acc_y_sum / angle_y_sum;
								}
							}

							float ratio = 0.0f;
							float angle_sum = sqrt(angle_x_sum*angle_x_sum + angle_y_sum*angle_y_sum);
							float acc_sum = sqrt(acc_x_sum*acc_x_sum + acc_y_sum*acc_y_sum);
							if (angle_sum != 0.0f){
								ratio = acc_sum / angle_sum;
							}

							counter1++;
							counter2++;
							/*
							if (abs(dis_x) > acc_max){
								acc_max = abs(dis_x);
							}
							if (abs(angle_dis_x) > angle_max){
								angle_max = abs(angle_dis_x);
							}
							ratio_x = acc_max / angle_max;
							*/
							float radius_temp_x;
							if (mov[0] >= 20.0f || mov[0] <= -20.0f){
								ratio_x *= 5.0f;
								negative_okay_time_x = time;
							}
							if (abs(ratio_x_max) < abs(ratio_x)){
								ratio_x_max = ratio_x;
							}
							if (ratio_x > 0){
								radius_temp_x = DEFAULT_R / (1 + exp(-1.0f*(ratio_x - 4.0f)));
							}
							else{
								if (time - negative_okay_time_x > 2000000000){
									radius_temp_x = -DEFAULT_R / (1 + exp(-1.0f*(0.0 - ratio_x - 4.0f)));
								}
								else{
									radius_temp_x = DEFAULT_R / (1 + exp(-1.0f*(ratio_x - 4.0f)));
								}
							}

							float radius_temp_y;
							if (mov[1] >= 20.0f || mov[1] <= -20.0f){
								ratio_y *= 5.0f;
								//ratio_y += 5.0f;
								negative_okay_time_y = time;
							}
							if (abs(ratio_y_max) < abs(ratio_y)){
								ratio_y_max = ratio_y;
							}
							if (ratio_y > 0){
								radius_temp_y = DEFAULT_R / (1 + exp(-1.0f*(ratio_y - 4.0f)));
							}
							else{
								if (time - negative_okay_time_y > 2000000000){
									radius_temp_y = -DEFAULT_R / (1 + exp(-1.0f*(0.0 - ratio_y - 4.0f)));
								}
								else{
									radius_temp_y = DEFAULT_R / (1 + exp(-1.0f*(ratio_y - 4.0f)));
								}
							}
							
							if (radius_temp_x > radius_temp_y){
								radius = radius_temp_x;
							}
							else{
								radius = radius_temp_y;
							}
							float temp[3];
							temp[0] = 0.0f;
							temp[1] = 0.0f;
							temp[2] = 0.0f - radius;
							float last_pos_x[3];
							float curr_pos_x[3];
							rotateVec3(init_ori_x, temp, last_pos_x);
							rotateVec3(currentOrientationQuaternion, temp, curr_pos_x);
							lock2.lock();
							pos[0] = temp_pos_x[0] + curr_pos_x[0] - last_pos_x[0];
							//pos[1] = temp_pos_x[1] + curr_pos[1] - last_pos[1];
							pos[2] = temp_pos_x[2] + curr_pos_x[2] - last_pos_x[2];
							lock2.unlock();
							/*
							if (radius_temp_x > radius_temp_y){
								radius = radius_temp_x;
							}
							else{
								radius = radius_temp_y;
							}
							temp[0] = 0.0f;
							temp[1] = 0.0f;
							temp[2] = 0.0f - radius;
							*/
							float last_pos_y[3];
							float curr_pos_y[3];
							rotateVec3(init_ori_y, temp, last_pos_y);
							rotateVec3(currentOrientationQuaternion, temp, curr_pos_y);
							lock2.lock();
							pos[1] = temp_pos_y[1] + curr_pos_y[1] - last_pos_y[1];
							lock2.unlock();
							
							//fprintf(op, "%f    %f %f %f   %f %f %f   %f %f %f   %f %f %f\n", pos[0]*pos[0]+pos[1]*pos[1]+pos[2]*pos[2], pos[0], pos[1], pos[2], temp_pos_x[0], temp_pos_x[1], temp_pos_x[2], curr_pos[0], curr_pos[1], curr_pos[2], last_pos[0], last_pos[1], last_pos[2]);
							//fprintf(op, "%lld %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f\n", last_time, pos[0], pos[1], pos[2], angle_dis_x, dis_x, mov[0], angle_dis_y, dis_y, mov[1], mov[2], radius_temp_x, radius_temp_y, curr_pos_x[0] - last_pos_x[0], curr_pos_y[1] - last_pos_y[1], curr_pos_x[2] - last_pos_x[2]);
							fprintf(op, "%lld %lld %f %f %f %f %f %f %f %f %f %f %d %d %f %f %f %f %f %f %f %f\n", last_time, last_angle_time, pos[0], pos[1], pos[2], angle_dis_x, dis_x, mov[0], angle_dis_y, dis_y, mov[1], mov[2], counter1, counter2, radius_temp_x, radius_temp_y, acc_sum, angle_sum, radius, curr_pos_x[0] - last_pos_x[0], curr_pos_y[1] - last_pos_y[1], curr_pos_x[2] - last_pos_x[2]);
							last_angle_dis_x = angle_dis_x;
							angle_dis_x = 0.0f;
							last_angle_dis_y = angle_dis_y;
							angle_dis_y = 0.0f;
						}
						last_time = time;
					}
				}
				end++;
				i++;
			}
			/*
			iSendResult = send(ClientSocket, recvbuf, iResult, 0);

			if (iSendResult == SOCKET_ERROR)
			{
			printf("send failed: %d\n", WSAGetLastError());
			closesocket(ClientSocket);
			WSACleanup();
			return 1;
			}
			printf("Bytes sent: %ld\n", iSendResult);
			*/
		}
		else if (iResult == 0){
			ClientSocket = accept(ListenSocket, NULL, NULL);
			fclose(op);
		}
		else
		{
			printf("recv failed: %d\n", WSAGetLastError());
			closesocket(ClientSocket);
			WSACleanup();
		}
	} while (true);

	// Free the resouces
	closesocket(ListenSocket);
	WSACleanup();

	return;
}

void init()
{
	glClearColor(1.0, 0.0, 0.0, 1.0);  //background color and alpha
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(45, 640.0 / 480.0, 1.0, 500.0);
	glMatrixMode(GL_MODELVIEW);
	//      glShadeModel(GL_FLAT);          // no color interpolation
	//      glShadeModel(GL_SMOOTH);        // color interpolation (default)
	//      glColor3f(0.0,1.0,0.0);
	//rflag[0] = rflag[1] = rflag[2] = 0;
}

void display()
{

	glClear(GL_COLOR_BUFFER_BIT);
	//glLoadIdentity();
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	//rtri += 0.2f;
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	gluPerspective(45, 1024.0f / 680.0f, 0.5, 100);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glColor3f(0.5f, 0.5f, 1.0f);

	glTranslatef(0.0f, -0.5f, -45.0f);
	lock2.lock();
	glTranslatef(pos[0], pos[1], pos[2]);
	lock2.unlock();
	/*
	float pos1[3];
	float pos2[3];
	pos1[0] = 0.0f;
	pos1[1] = 0.0f;
	pos1[2] = -DEFAULT_R;
	rotateVec3(currentOrientationQuaternion, pos1, pos2);
	glTranslatef(0.0f-pos1[0], 0.0f-pos1[1], 0.0f-pos1[2]);
	glTranslatef(pos2[0], pos2[1], pos2[2]);
	*/
	//glTranslatef(-x_pos, -y_pos, -z_pos);

	lock.lock();
	glRotatef((float)(2.0f * acos(0.0f - currentOrientationQuaternion[3]) * 180.0f / M_PI), currentOrientationQuaternion[0], currentOrientationQuaternion[1], currentOrientationQuaternion[2]);
	lock.unlock();

	//glTranslatef(pos1[0], pos1[1], pos1[2]);
	//glTranslatef(x_pos, y_pos, z_pos);
	glBegin(GL_QUADS);
	glVertex3f(-1.0f, 0.0f, -7.0f);
	glVertex3f(-1.2f, 0.0f, 0.75f);
	glVertex3f(1.2f, 0.0f, 0.75f);
	glVertex3f(1.0f, 0.0f, -7.0f);
	glEnd();
	glBegin(GL_QUADS);
	glVertex3f(-1.2f, 0.0f, 0.75f);
	glVertex3f(1.2f, 0.0f, 0.75f);
	glVertex3f(2.0f, 0.0f, 0.8f);
	glVertex3f(-2.0f, 0.0f, 0.8f);
	glEnd();
	glBegin(GL_QUADS);
	glVertex3f(-2.0f, 0.0f, 0.8f);
	glVertex3f(2.0f, 0.0f, 0.8f);
	glVertex3f(1.6f, 0.0f, 1.15f);
	glVertex3f(-1.6f, 0.0f, 1.15f);
	glEnd();
	glBegin(GL_QUADS);
	glVertex3f(-1.6f, 0.0f, 1.15f);
	glVertex3f(1.6f, 0.0f, 1.15f);
	glVertex3f(0.5f, 0.0f, 1.2f);
	glVertex3f(-0.5f, 0.0f, 1.2f);
	glEnd();
	glBegin(GL_QUADS);
	glVertex3f(-0.5f, 0.0f, 1.2f);
	glVertex3f(0.5f, 0.0f, 1.2f);
	glVertex3f(0.4f, 0.0f, 1.45f);
	glVertex3f(-0.4f, 0.0f, 1.45f);
	glEnd();
	glBegin(GL_QUADS);
	glVertex3f(-0.4f, 0.0f, 1.45f);
	glVertex3f(0.4f, 0.0f, 1.45f);
	glVertex3f(0.3f, 0.0f, 1.5f);
	glVertex3f(-0.3f, 0.0f, 1.5f);
	glEnd();
	glBegin(GL_QUADS);
	glVertex3f(-0.3f, 0.0f, 1.5f);
	glVertex3f(0.3f, 0.0f, 1.5f);
	glVertex3f(0.3f, 0.0f, 3.5f);
	glVertex3f(-0.3f, 0.0f, 3.5f);
	glEnd();
	glBegin(GL_QUADS);
	glVertex3f(-0.3f, 0.0f, 3.5f);
	glVertex3f(0.3f, 0.0f, 3.5f);
	glVertex3f(0.5f, 0.0f, 3.7f);
	glVertex3f(-0.5f, 0.0f, 3.7f);
	glEnd();
	glBegin(GL_TRIANGLES);
	glVertex3f(0.0f, 0.0f, -8.0f);
	glVertex3f(-1.0f, 0.0f, -7.0f);
	glVertex3f(1.0f, 0.0f, -7.0f);
	glEnd();
}

int main(int argc, char** argv)
{
	_beginthread(socket, 0, NULL);
	SDL_Init(SDL_INIT_EVERYTHING);
	SDL_Surface *screen;
	screen = SDL_SetVideoMode(1024, 680, 8, SDL_SWSURFACE | SDL_OPENGL);
	//      screen = SDL_SetVideoMode(640, 480, 8, SDL_SWSURFACE|SDL_FULLSCREEN);
	bool running = true;
	const int FPS = 60;
	Uint32 start;
	SDL_Event event;
	init();
	while (running) {
		start = SDL_GetTicks();
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
			case SDL_QUIT:
				running = false;
				break;
			}
		}

		display();
		SDL_GL_SwapBuffers();
		if (1000 / FPS > SDL_GetTicks() - start)
			SDL_Delay(1000 / FPS - (SDL_GetTicks() - start));
	}
	SDL_Quit();
	return 0;
}
