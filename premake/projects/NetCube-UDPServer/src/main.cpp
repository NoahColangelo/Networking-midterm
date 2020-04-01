//Noah Colangelo 100659538
//Giulia Santin 100657351
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <GLM/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using std::vector;
using std::cout;
using std::endl;
using std::cin;

///// Networking //////
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#pragma comment(lib, "Ws2_32.lib")
///////////////////////

GLFWwindow* window;

float UPDATE_INTERVAL = 0.100; //seconds

int serverSize;
int clientID = 1;

bool initGLFW() {
	if (glfwInit() == GLFW_FALSE) {
		std::cout << "Failed to Initialize GLFW" << std::endl;
		return false;
	}

	//Create a new GLFW window
	window = glfwCreateWindow(800, 800, "Window", nullptr, nullptr);
	glfwMakeContextCurrent(window);

	return true;
}

bool initGLAD() {
	if (gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) == 0) {
		std::cout << "Failed to initialize Glad" << std::endl;
		return false;
	}
	return true;
}

struct Client
{
	Client() {}
	Client(int8_t id,sockaddr_in addrIN, int sockLen)
	{
		_id = id;
		_udpSockAddr = addrIN;
		_udpSockAddrLen = sockLen;
	}
	int8_t _id = -1;
	sockaddr_in _udpSockAddr;
	int _udpSockAddrLen;
};

vector <Client> _clients;

GLuint filter_mode = GL_LINEAR;

void keyboard(){	

	//Buttons to increase and decrease interval lag ( lowest it goes is 0.100 but due to how floats work it goes down to 0.9)
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
		UPDATE_INTERVAL += 0.01;
		std::cout << "lag increased to: " << UPDATE_INTERVAL << std::endl;
	}
	if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS && UPDATE_INTERVAL > 0.1f) {
		UPDATE_INTERVAL -= 0.01;
		std::cout << "lag decreased to: " << UPDATE_INTERVAL << std::endl;
	}

	if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
		if (filter_mode == GL_LINEAR) {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			filter_mode = GL_NEAREST;
		}
		else {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			filter_mode = GL_LINEAR;
		}
	}



}
// Networking
SOCKET server_socket;
struct addrinfo* ptr = NULL;
#define PORT "8888"
#define BUFLEN 512

bool initNetwork() {
	//Initialize winsock
	WSADATA wsa;

	int error;
	error = WSAStartup(MAKEWORD(2, 2), &wsa);

	if (error != 0) {
		printf("Failed to initialize %d\n", error);
		return 0;
	}

	//Create a server socket

	struct addrinfo hints;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;
	hints.ai_flags = AI_PASSIVE;

	if (getaddrinfo(NULL, PORT, &hints, &ptr) != 0) {
		printf("Getaddrinfo failed!! %d\n", WSAGetLastError());
		WSACleanup();
		return 0;
	}

	
	server_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	if (server_socket == INVALID_SOCKET) {
		printf("Failed creating a socket %d\n", WSAGetLastError());
		WSACleanup();
		return 0;
	}

	// Bind socket

	if (bind(server_socket, ptr->ai_addr, (int)ptr->ai_addrlen) == SOCKET_ERROR) {
		printf("Bind failed: %d\n", WSAGetLastError());
		closesocket(server_socket);
		freeaddrinfo(ptr);
		WSACleanup();
		return 1;
	}

	/// Change to non-blocking mode
	u_long mode = 1;// 0 for blocking mode
	ioctlsocket(server_socket, FIONBIO, &mode);

	printf("Server is ready!\n");

	return 1;
}


int main() {

	cout << "How many client do you want in the server: " << endl;
	cin >> serverSize;

	//Initialize GLFW
	if (!initGLFW())
		return 1;

	//Initialize GLAD
	if (!initGLAD())
		return 1;

	//Initialize Network
	if (!initNetwork())
		return 1;
	
	// Timer variables for sending network updates
	float time = 0.0;
	float previous = glfwGetTime();

	float lastServerPosX;
	float lastServerPosY;

	struct sockaddr_in fromAddr;
	int fromlen;
	fromlen = sizeof(fromAddr);
	///// Game loop /////
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		/////// Timer for network updates
		float now = glfwGetTime();
		float delta = now - previous;
		previous = now;


		// When timer goes off, send an update
		time -= delta;
		if (time <= 0.f)
		{
			char buf[BUFLEN];
			struct sockaddr_in fromAdder;
			int fromLen;
			fromLen = sizeof(fromAdder);

			memset(buf, -1, BUFLEN);

			int bytes_received = -1;
			int sError = -1;

			bytes_received = recvfrom(server_socket, buf, BUFLEN, 0, (struct sockaddr*) & fromAdder, &fromLen);//receives from client

			sError = WSAGetLastError();

			if (sError != WSAEWOULDBLOCK && bytes_received > 0)
			{

				if (buf[0] == '0' && _clients.size() < serverSize)//checks for new clients
				{
					char message[BUFLEN];
					std::string msg = std::to_string(clientID);
					strcpy(message, (char*)msg.c_str());

					sendto(server_socket, message, BUFLEN, 0, (struct sockaddr*) & fromAdder, fromLen);
					_clients.emplace_back(clientID, fromAdder, fromlen);
					cout << "client " << clientID << " added!!" << endl;
					clientID++;
				}
				else
				{
					cout << "received " << buf << " from client " << buf[0] << endl;
				}

				if (buf[0] != '0')//checks to make sure it doesnt send message from new client to other clients
				{
					for (int i = 0; i < _clients.size(); i++)
					{
						if (buf[0] != _clients[i]._id)
						{
							if (sendto(server_socket, buf, BUFLEN, 0, (struct sockaddr*) & _clients[i]._udpSockAddr, _clients[i]._udpSockAddrLen)
								== SOCKET_ERROR)
							{
								cout << "send failed...\n" << std::endl;
								sError = WSAGetLastError();
							}
							else
							{
								cout << "sent: " << buf << " to " << _clients[i]._id<<endl;
							}
						}
					}
				}
			}

			time = UPDATE_INTERVAL; // reset the timer
		}

		keyboard();

	}
		return 0;
}