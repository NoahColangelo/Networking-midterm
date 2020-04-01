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

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h> 

///// Networking //////
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#pragma comment(lib, "Ws2_32.lib")
///////////////////////

using std::vector;
using std::cout;
using std::endl;

GLFWwindow* window;

unsigned char* hockeysmacker;
int width, height;
float UPDATE_INTERVAL = 0.1; //seconds

void loadImage() {
	int channels, channelsPuck;
	stbi_set_flip_vertically_on_load(true);
	hockeysmacker = stbi_load("Smacker.jpg",
		&width,
		&height,
		&channels,
		0);
	if (hockeysmacker) {
		std::cout << "Image LOADED" << width << " " << height << std::endl;
	}
	else {
		std::cout << "Failed to load image!" << std::endl;
	}
}

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
}


GLuint shader_program;

bool loadShaders() {
	// Read Shaders from file
	std::string vert_shader_str;
	std::ifstream vs_stream("vertex_shader.glsl", std::ios::in);
	if (vs_stream.is_open()) {
		std::string Line = "";
		while (getline(vs_stream, Line))
			vert_shader_str += "\n" + Line;
		vs_stream.close();
	}
	else {
		printf("Could not open vertex shader!!\n");
		return false;
	}
	const char* vs_str = vert_shader_str.c_str();

	std::string frag_shader_str;
	std::ifstream fs_stream("frag_shader.glsl", std::ios::in);
	if (fs_stream.is_open()) {
		std::string Line = "";
		while (getline(fs_stream, Line))
			frag_shader_str += "\n" + Line;
		fs_stream.close();
	}
	else {
		printf("Could not open fragment shader!!\n");
		return false;
	}
	const char* fs_str = frag_shader_str.c_str();

	GLuint vs = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vs, 1, &vs_str, NULL);
	glCompileShader(vs);
	GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fs, 1, &fs_str, NULL);
	glCompileShader(fs);

	shader_program = glCreateProgram();
	glAttachShader(shader_program, fs);
	glAttachShader(shader_program, vs);
	glLinkProgram(shader_program);

	return true;
}

struct client
{
	client() {}
	client(int8_t _id, glm::vec2 pos, glm::vec2 vel)
	{
		id = _id;
		currentPos = pos;
		velocity = vel;
	}
	int8_t id = 0;
	glm::vec2 currentPos = glm::vec2(0, 0);
	glm::vec2 lastPos = glm::vec2(0,0);
	glm::vec2 velocity = glm::vec2(0, 0);

	glm::vec2 lastSentPos = glm::vec2(0, 0);
	glm::vec2 lastSentVel = glm::vec2(0, 0);
	float lastSentTime = 0.0f;
	glm::vec2 futurePos = glm::vec2(0,0);

	glm::mat4 clientMat4 = glm::mat4(1.0f);
	glm::mat4 MVP;

};

struct packet
{
	packet() {}
	packet(int8_t _id, glm::vec2 _currPos, glm::vec2 _currVel)
	{
		id = _id;
		currentPos = _currPos;
		velocity = _currVel;
	}
	int8_t id;
	glm::vec2 currentPos;
	glm::vec2 velocity;
};

float dotProduct(glm::vec2 a)
{
	float temp;
	temp = (a.x * a.x) + (a.y * a.y);
	return temp;
}

vector <client> clients;

client myClient;

GLuint filter_mode = GL_LINEAR;

void keyboard() {
	if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
		myClient.currentPos.y += 0.01;
	}
	if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
		myClient.currentPos.y -= 0.01;
	}
	if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
		myClient.currentPos.x += 0.01;
	}
	if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
		myClient.currentPos.x -= 0.01;
	}

	//Buttons to increase and decrease interval lag ( lowest it goes is 0.100 but due to how floats work it goes down to 0.9)
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
		UPDATE_INTERVAL += 0.01;
		std::cout<< "lag increased to: " << UPDATE_INTERVAL << std::endl;
	}
	if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS && UPDATE_INTERVAL > 0.1f) {
		UPDATE_INTERVAL -= 0.01;
		std::cout << "lag decreased to: " << UPDATE_INTERVAL <<std::endl;
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
SOCKET client_socket;
struct addrinfo* ptr = NULL;
#define SERVER "127.0.0.1"
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

	//Create a client socket

	//CLIENT TYPES IN IP HERE-------------------------------------
	std::string IP;
	std::cout << "Please type the IP of the server" << std::endl;
	std::getline(std::cin, IP);

	struct addrinfo hints;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;

	if (getaddrinfo(IP.c_str(), PORT, &hints, &ptr) != 0) {
		printf("Getaddrinfo failed!! %d\n", WSAGetLastError());
		WSACleanup();
		return 0;
	}

	
	client_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	if (client_socket == INVALID_SOCKET) {
		printf("Failed creating a socket %d\n", WSAGetLastError());
		WSACleanup();
		return 0;
	}

	u_long mode = 1;// 0 for blocking mode
	ioctlsocket(client_socket, FIONBIO, &mode);

	return 1;
}


int main() {
	//Initialize GLFW
	if (!initGLFW())
		return 1;

	//Initialize GLAD
	if (!initGLAD())
		return 1;

	//Initialize Network
	if (!initNetwork())
		return 1;

	// Cube data
	static const GLfloat points[] = {//front face, 2 triangles
		-0.5f, -0.5f, 0.5f,//0  front face
		0.5f, -0.5f, 0.5f, //3
		-0.5f, 0.5f, 0.5f, //1
		0.5f, -0.5f, 0.5f, //3
		0.5f, 0.5f, 0.5f, //2
		-0.5f, 0.5f, 0.5f, //1
		0.5f, -0.5f, 0.5f, //3 Right face
		0.5f, -0.5f, -0.5f, //7
		0.5f, 0.5f, 0.5f, //2
		0.5f, -0.5f, -0.5f, //7
		0.5f, 0.5f, -0.5f, //6
		0.5f, 0.5f, 0.5f,  //2
		-0.5f, -0.5f, -0.5f, //4 Left face
		-0.5f, -0.5f, 0.5f, //0
		-0.5f, 0.5f, -0.5f, //5
		-0.5f, -0.5f, 0.5f, //0
		-0.5f, 0.5f, 0.5f,  //1
		-0.5f, 0.5f, -0.5f, //5
		-0.5f, 0.5f, 0.5f,  //1 Top face
		0.5f, 0.5f, 0.5f,  //2
		-0.5f, 0.5f, -0.5f,//5
		0.5f, 0.5f, 0.5f,   //2
		0.5f, 0.5f, -0.5f, //6
		-0.5f, 0.5f, -0.5f, //5
		-0.5f, -0.5f, -0.5f, //4 Bottom face
		0.5f, -0.5f, -0.5f, //7
		-0.5f, -0.5f, 0.5f, //0
		0.5f, -0.5f, -0.5f, //7
		0.5f, -0.5f, 0.5f, //3
		-0.5f, -0.5f, 0.5f, //0
		-0.5f, 0.5f, -0.5f, //5 Back face
		0.5f, 0.5f, -0.5f, //6
		-0.5f, -0.5f, -0.5f, //4
		0.5f, 0.5f, -0.5f, //6
		0.5f, -0.5f, -0.5f, //7
		-0.5f, -0.5f, -0.5f, //4
	};
	
	// Color data
	static const GLfloat colors[] = {
		0.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f,
		1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 1.0f, 1.0f,
		0.0f, 1.0f, 1.0f,
		0.0f, 1.0f, 1.0f,
		0.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f,
		1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 1.0f, 1.0f,
		0.0f, 1.0f, 1.0f,
		0.0f, 1.0f, 1.0f,
		0.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f,
		1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 1.0f, 1.0f,
		0.0f, 1.0f, 1.0f,
		0.0f, 1.0f, 1.0f
	};
	
	///////// TEXTURES ///////
	static const GLfloat uv[] = {
		0.0f, 0.0f,
		1.0f, 0.0f,
		0.0f, 1.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,
		0.0f, 1.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,
		0.0f, 1.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,
		0.0f, 1.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,
		0.0f, 1.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,
		0.0f, 1.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
		0.0f, 1.0f
	};

#pragma region openGL_Stuff
	//VBO
	GLuint pos_vbo = 0;
	glGenBuffers(1, &pos_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, pos_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(points), points, GL_STATIC_DRAW);

	GLuint color_vbo = 1;
	glGenBuffers(1, &color_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, color_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(colors), colors, GL_STATIC_DRAW);



	// VAO
	GLuint vao = 0;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, pos_vbo);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);

	glBindBuffer(GL_ARRAY_BUFFER, color_vbo);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, NULL);

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	GLuint uv_vbo = 2;
	glGenBuffers(1, &uv_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, uv_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(uv), uv, GL_STATIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, uv_vbo);

	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, NULL);

	loadImage();

	GLuint textureHandle, textureHandlePuck;

	glGenTextures(1, &textureHandle);

	// "Bind" the newly created texture : all future texture functions will modify this texture
	glBindTexture(GL_TEXTURE_2D, textureHandle);

	// Give the image to OpenGL
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, hockeysmacker);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	// Release the space used for your image once you're done
	glBindTexture(GL_TEXTURE_2D, GL_NONE);
	stbi_image_free(hockeysmacker);

	// Load your shaders
	if (!loadShaders())
		return 1;

	// Projection matrix : 45° Field of View, 4:3 ratio, display range : 0.1 unit <-> 100 units
	int width, height;
	glfwGetWindowSize(window, &width, &height);
	glm::mat4 Projection =
		glm::perspective(glm::radians(45.0f),
			(float)width / (float)height, 0.0001f, 100.0f);

	// Camera matrix
	glm::mat4 View = glm::lookAt(
		glm::vec3(0, 0, 3), // Camera is at (0,0,3), in World Space
		glm::vec3(0, 0, 0), // and looks at the origin
		glm::vec3(0, 1, 0)  // Head is up (set to 0,-1,0 to look upside-down)
	);

	// Model matrix : an identity matrix (model will be at the origin)

	// Our ModelViewProjection : multiplication of our 3 matrices
	myClient.MVP = Projection * View * myClient.clientMat4; // Remember, matrix multiplication is the other way around

	// Get a handle for our "MVP" uniform
	// Only during initialisation
	GLuint MatrixID =
		glGetUniformLocation(shader_program, "MVP");


	// Face culling
	glEnable(GL_CULL_FACE);
	glFrontFace(GL_CCW);
	glEnable(GL_DEPTH_TEST);


	/////// TEXTURE
	glUniform1i(glGetUniformLocation(shader_program, "myTextureSampler"), 0);
#pragma endregion

	// Timer variables for sending network updates
	float sendTime = 0.0;
	float receiveTime = 0.0;
	float previous = glfwGetTime();

	bool thresholdBroken = false;
	float threshold = 0.05;
	
	///// Game loop /////
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		
		/////// Timer for network updates
		float now = glfwGetTime();
		float delta = now - previous;
		previous = now;
		
		receiveTime -= delta;

		//Dead reckoning for players box
		myClient.futurePos = myClient.lastSentPos + myClient.velocity * (now - myClient.lastSentTime);

		//Dead reckoning for other clients
		for (int i = 0; i < clients.size(); i++)
		{
			clients[i].currentPos = clients[i].lastSentPos + clients[i].lastSentVel * (now - clients[i].lastSentTime);
		}

		//checks if the threshold for sending a packet has been breeched
		if (dotProduct(myClient.currentPos - myClient.futurePos) > (threshold * threshold))
		{
			cout << "breach" << endl;
			myClient.lastSentPos = myClient.currentPos;
			myClient.lastSentVel = myClient.velocity;
			myClient.lastSentTime = now;
			thresholdBroken = true;
		}

		if(thresholdBroken)
			sendTime -= delta;

		if (sendTime <= 0.f )
		{
			// Code to send position updates go HERE...
			char message[BUFLEN];
			memset(message, 0, BUFLEN);

			packet _packet = packet(myClient.id, myClient.currentPos, myClient.velocity);
			memcpy(message, &_packet, sizeof(_packet));
			///*std::string msg = std::to_string(myClient.id) + "$" +
			//	std::to_string(myClient.currentPos.x) + "@" + std::to_string(myClient.currentPos.y)
			//*/	+ "%" + std::to_string(myClient.velocity.x) + "#" + std::to_string(myClient.velocity.y); //the message that will be sent

			//strcpy(message, (char*)msg.c_str());
			//sends messages
			if (sendto(client_socket, message, BUFLEN, 0, ptr->ai_addr, ptr->ai_addrlen) == SOCKET_ERROR)
			{
				cout << "Sendto() failed...\n" << endl;
			}

			memset(message, '/0', BUFLEN);

			sendTime = UPDATE_INTERVAL; // reset the timer
			thresholdBroken = false;
		}

		if (receiveTime <= 0.0f)
		{
			char buf[BUFLEN];
			struct sockaddr_in fromAdder;
			int fromLen;
			fromLen = sizeof(fromAdder);

			memset(buf, 0, BUFLEN);

			int bytes_received = -1;
			int sError = -1;

			bytes_received = recvfrom(client_socket, buf, BUFLEN, 0, (struct sockaddr*) & fromAdder, &fromLen);
			sError = WSAGetLastError();

			//float id;
			//float posX;
			//float posY;
			//float velX;
			//float velY;
			if (sError != WSAEWOULDBLOCK && bytes_received > 0 && myClient.id != 0)
			{
				cout << "Received: " << buf <<endl;

				packet _packet = packet();
				memcpy(&_packet, buf, sizeof(_packet));

				//std::string temp = buf;// becomes buf

				//std::size_t idPos = temp.find('$'); // finds position of $
				//id = std::stof(temp.substr(0, idPos - 1));//gets ID

				//std::size_t Xpos = temp.find('@');// finds the position of the @
				//posX = std::stof(temp = temp.substr(0, Xpos - 1));// gets posX

				//std::size_t Ypos = temp.find('%');// finds the position of the %
				//posY = std::stof(temp = temp.substr(0, Ypos - 1));// gets posY

				//std::size_t Xvel = temp.find('#');// finds the position of the #
				//velX = std::stof(temp = temp.substr(0, Xvel - 1));// gets velX

				//temp = buf;
				//temp = temp.substr(Xvel + 1);
				//velY = std::stof(temp);//gets velY

				if (clients.size() == 0)//for when the first client shows up
				{
					clients.emplace_back(_packet.id, _packet.currentPos, _packet.velocity);
				}
				else
				{
					for (int i = 0; i < clients.size(); i++)//when there is more then 1 client in server
					{
						if (clients[i].id != _packet.id)
						{
							clients.emplace_back(_packet.id, _packet.currentPos, _packet.velocity);
							break;
						}
						else if (clients[i].id == _packet.id)//the correction packet is sent here
						{
							clients[i].lastSentPos = clients[i].currentPos;
							clients[i].lastSentVel = clients[i].velocity;
							clients[i].lastSentTime = now;

							clients[i].currentPos = _packet.currentPos;
							clients[i].velocity = _packet.velocity;
							break;
						}
					}
				}
			}
			else if (myClient.id == 0)
			{
				myClient.id = buf[0];
				cout << static_cast<int>(myClient.id) << endl;
			}

			receiveTime = UPDATE_INTERVAL;
		}

		glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glUseProgram(shader_program);

		myClient.clientMat4 = glm::mat4(1.0f);

		for (int i = 0; i < clients.size(); i++)
		{
			clients[i].clientMat4 = glm::mat4(1.0f);
		}

		myClient.lastPos = myClient.currentPos;

		keyboard();

		//calculations for the clients own object
		myClient.velocity = (myClient.currentPos - myClient.lastPos)/ delta;
		myClient.clientMat4 = glm::translate(myClient.clientMat4, glm::vec3(myClient.currentPos.x, myClient.currentPos.y, -2.0f));
		myClient.clientMat4 = glm::scale(myClient.clientMat4, glm::vec3(0.75f, 0.75f, 0.75f));
		myClient.MVP = Projection * View * myClient.clientMat4;

		if (clients.size() != 0)//does calculations for other clients
		{
			for (int i = 0; i < clients.size(); i++)
			{
				clients[i].clientMat4 = glm::translate(clients[i].clientMat4, glm::vec3(clients[i].currentPos.x, clients[i].currentPos.y, -2.0f));
				clients[i].clientMat4 = glm::scale(clients[i].clientMat4, glm::vec3(0.75f, 0.75f, 0.75f));
				clients[i].MVP = Projection * View * clients[i].clientMat4;
			}
		}


		glBindVertexArray(vao);

		if (clients.size() != 0)
		{
			for (int i = 0; i < clients.size(); i++)
			{
				glUniformMatrix4fv(MatrixID, 1,
					GL_FALSE, &clients[i].MVP[0][0]);

				glBindTexture(GL_TEXTURE_2D, textureHandle);

				glDrawArrays(GL_TRIANGLES, 0, 36);
			}
		}

		glUniformMatrix4fv(MatrixID, 1,
			GL_FALSE, &myClient.MVP[0][0]);

		glBindTexture(GL_TEXTURE_2D, textureHandle);

		glDrawArrays(GL_TRIANGLES, 0, 36);

		glfwSwapBuffers(window);
	}
	return 0;

}