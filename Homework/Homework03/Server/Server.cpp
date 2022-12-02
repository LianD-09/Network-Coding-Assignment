// Server.cpp : Defines the entry point for the console application.
//

#define FD_SETSIZE 1024
#include "stdafx.h"
#include "stdlib.h"
#include "map"
#include "vector"
#include "string"
#include "fstream"
#include "winsock2.h"
#include "ws2tcpip.h"
#include "iostream"
#include "stdlib.h"
#include "process.h"
#define SERVER_ADDR "127.0.0.1"
#define BUFF_SIZE 2048
#define ENDING_DELIMITER "\r\n"
#define FILE_NAME "account.txt"
#define LOGIN_HEADER "USER"
#define POST_HEADER "POST"
#define LOGOUT_HEADER "BYE"
#define SUCCESS_LOGIN "10"
#define SUCCESS_POST "20"
#define SUCCESS_LOGOUT "30"
#define LOCKED_LOGIN "11"
#define NOT_LOGIN "21"
#define NOT_EXISTED "12"
#define LOGGING_IN "13"
#define INVALID_REQUEST "99"
using namespace std;

#pragma comment (lib, "Ws2_32.lib")

CRITICAL_SECTION critical;

//Store the value of accounts
map<string, int> users;

//Store the client sockets 
SOCKET client[4096];

int numClient = 0, //Number of client is connecting
	numThread = 0; //Number of using thread

struct state {
	string account = ""; //Save the account using
	bool isLogin = false; //Save the state of client
} status[4096];

//Save the value before create thread
struct argList {
	SOCKET sock;
	int offset = 0;
};

/**
* @funtion bindAddress: Bind a address for socket.
*
* @param listenSock: Socket need to be bound.
* @param ipAddress: A pointer to the string containing the address used to bind.
* @param port: The number of port.
* @return: No return value.
**/
void bindAddress(SOCKET listenSock, char* ipAddress, int port) {
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(port);
	inet_pton(AF_INET, ipAddress, &serverAddr.sin_addr);
	if (bind(listenSock, (sockaddr *)&serverAddr, sizeof(serverAddr))) {
		printf("Error! Cannot bind this address.\n");
		exit(0);
	}
}

/**
* @function handleStream: Handle stream by using delimiter.
*
* @param storeMess: A string contains correct message or a variety of messages.
* @param recvMess: A vector contains all of messages splitted from storeMess param.
* @return: No value return.
**/
void handleStream(char* storeMess, vector<char*> &recvMess) {
	char* delim = ENDING_DELIMITER,
		*next_message, //Next message
		*message = strtok_s(storeMess, delim, &next_message); //Current message delimited by delim

	do {
		recvMess.push_back(message);
		message = strtok_s(NULL, delim, &next_message);
	} while (message != NULL);
}

/**
* @function handleReply: Handle reply message contains header and body.
*
* @param header: A string represents the header of message.
* @param message: A string contains the value of body.
* @param account: A string will contain the account name.
* @param isLogin: A value marks current client is logging in or not.
* @return: A string represents the reply message.
**/
char* handleReply(char *header, char *message, string &account, bool &isLogin) {
	char replyMess[BUFF_SIZE];
	//Header USER
	if (!strcmp(header, LOGIN_HEADER)) {
		if (users.find(message) == users.end()) {
			strcpy_s(replyMess, sizeof(replyMess), NOT_EXISTED);
		}
		else {
			int stateUser = users[message];
			if (stateUser == 1) {
				strcpy_s(replyMess, sizeof(replyMess), LOCKED_LOGIN);
			}
			else if (stateUser == -1) {
				strcpy_s(replyMess, sizeof(replyMess), LOGGING_IN);
			}
			else if (stateUser == 0) {
				account = message;
				//Enter the critical section
				EnterCriticalSection(&critical);
				users[account] = -1;
				isLogin = true;
				//Leave the critical section
				LeaveCriticalSection(&critical);
				strcpy_s(replyMess, sizeof(replyMess), SUCCESS_LOGIN);
			}
		}
	}
	//Header POST
	else if (!strcmp(header, POST_HEADER)) {
		if (!isLogin) {
			strcpy_s(replyMess, sizeof(replyMess), NOT_LOGIN);
		}
		else {
			strcpy_s(replyMess, sizeof(replyMess), SUCCESS_POST);
		}
	}
	//Header BYE
	else if (!strcmp(header, LOGOUT_HEADER)) {
		if (!isLogin) {
			strcpy_s(replyMess, sizeof(replyMess), NOT_LOGIN);
		}
		else {
			//Enter the critical section
			EnterCriticalSection(&critical);
			users[account] = 0;
			isLogin = false;
			//Leave the critical section
			LeaveCriticalSection(&critical);
			strcpy_s(replyMess, sizeof(replyMess), SUCCESS_LOGOUT);
		}
	}
	//Other
	else {
		strcpy_s(replyMess, sizeof(replyMess), INVALID_REQUEST);
	}
	return replyMess;
}

/**
* @function Receive: Receive message from client.
*
* @param connSock: Socket of client.
* @param buff: A string contains the echo message.
* @param appendMess: A string will contains full of message.
* @return: A integer represents the number of bytes received.
**/
int Receive(SOCKET connSock, char *buff, string &appendMess) {
	int n;

	do {
		n = recv(connSock, buff, BUFF_SIZE, 0);
		if (n == SOCKET_ERROR) {
			printf("Error %d: Cannot receive data.\n", WSAGetLastError());
			return n;
		}
		else if (strlen(buff) > 0) {
			buff[n] = 0;
			appendMess.append(buff);
		}
	} while (!strstr(buff + strlen(buff) - strlen(ENDING_DELIMITER), ENDING_DELIMITER) && n != 0);

	return n;
}

/**
* @function Send: Send message to client.
*
* @param s: Socket of client.
* @param buff: A string contains the echo message.
* @param size: Size of message.
* @param flags: A value represents flags.
* @return: A integer represents the number of bytes sent.
**/
int Send(SOCKET s, char *buff, int size, int flags) {
	int n;

	n = send(s, buff, size, flags);
	if (n == SOCKET_ERROR) {
		printf("Error: %d\n", WSAGetLastError());
	}

	return n;
}

/**
* @function: Close a soket of client and update values.
*
* @param i: The offset of client.
* @return: No value return.
**/
void closeClient(int i, fd_set &initfds) {
	printf("Client disconnects.\n");
	//If client is closed or disconnects, reset status of account to log out
	closesocket(client[i]);
	status[i].isLogin = false;
	//Enter the critical section
	EnterCriticalSection(&critical);
	users[status[i].account] = 0;
	numClient--;
	//Leave the critical section
	LeaveCriticalSection(&critical);
	FD_CLR(client[i], &initfds);
	client[i] = 0;
}

/**
* @funtion childThread: A single thread to handle request from each client.
*
* @param param: A pointer to a struct that contains a socket and a offset value.
* @return: 0.
**/
unsigned __stdcall childThread(void *param) {
	char buff[BUFF_SIZE];
	sockaddr_in clientAddr;
	argList *arg = (argList *)param;
	int ret, nEvents, curClient = 0, //The number of current clients are accepted
		fin = FD_SETSIZE + arg->offset, // Last value of offset
		clientAddrLen = sizeof(clientAddr), clientPort;
	char clientIP[INET_ADDRSTRLEN];
	SOCKET listenSock = arg->sock;
	SOCKET connSock;
	fd_set readfds, initfds; //use initfds to initiate readfds at the begining of every loop step

	FD_ZERO(&initfds);
	FD_SET(listenSock, &initfds);

	while (1) {
		readfds = initfds;		/* structure assignment */
		nEvents = select(0, &readfds, 0, 0, 0);
		if (nEvents < 0) {
			printf("\nError! Cannot poll sockets: %d", WSAGetLastError());
			break;
		}
		//Accept request
		if (FD_ISSET(listenSock, &readfds)) {
			if (curClient < fin) {
				connSock = accept(listenSock, (sockaddr *)&clientAddr, &clientAddrLen);
				if (connSock == SOCKET_ERROR)
					printf("Error %d: Cannot permit incoming connection.\n", WSAGetLastError());
				else {
					inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, sizeof(clientIP));
					clientPort = ntohs(clientAddr.sin_port);
					printf("Accept incoming connection from %s:%d\n", clientIP, clientPort);

					for (int i = arg->offset; i < fin; i++) {
						if (client[i] == 0) {
							client[i] = connSock;
							EnterCriticalSection(&critical);
							numClient++;
							LeaveCriticalSection(&critical);
							FD_SET(client[i], &initfds);
							curClient++;
							break;
						}
					}
					if (--nEvents == 0) {
						continue; //no more event
					}
				}
			}
		}

		for (int i = arg->offset; i < fin; i++) {
			//Break if the amount of clients is 0
			if (client[i] == 0) {
				continue;
			}
			//A string contain all messages
			string appendMess = "";
			vector<char*> recvMess;
			if (FD_ISSET(client[i], &readfds)) {
				ret = Receive(client[i], buff, appendMess);

				//Break if client disconnects
				if (ret <= 0) {
					closeClient(i, initfds);
					//End if the current clients is 0
					if (--curClient <= 0 && numThread > 1) {
						EnterCriticalSection(&critical);
						numThread--;
						LeaveCriticalSection(&critical);
						return 0;
					}
					break;
				}
				//Convert into char * type to handle
				char *storeMess = _strdup(appendMess.c_str());
				if (strlen(storeMess) > 0) {
					handleStream(storeMess, recvMess);
					for (auto it : recvMess) {
						printf("Receive from client: %s\n", it);
						//Split header and body
						char *message,
							*header = strtok_s(it, " ", &message);
						char replyMess[BUFF_SIZE] = "";
						//Create and send reply message
						strcpy_s(replyMess, sizeof(replyMess), handleReply(header, message, status[i].account, status[i].isLogin));
						ret = Send(client[i], replyMess, strlen(replyMess), 0);
					}
				}
			}

			if (--nEvents <= 0) {
				continue; //no more event
			}
		}
	}
	return 0;
}

/**
* @funtion inputUser: Get the account data from account.txt to a map
* @param file: Name of file containing data.
* @return: A map contains the data.
**/
map<string, int> inputUser(string file) {
	map<string, int> users;
	//Read file
	ifstream input(file);
	while (!input.eof()) {
		char line[BUFF_SIZE] = "";
		string readLine;
		//Get each line
		getline(input, readLine);
		strcpy_s(line, sizeof(line), readLine.c_str());
		//Split by a space character
		char* status,
			*user = strtok_s(line, " ", &status);
		int st = atoi(status);
		string userName = user;
		users.insert(make_pair(userName, st));
	}
	input.close();
	return users;
}

int main(int argc, char* argv[])
{
	WSADATA wsaData;
	WORD vVersion = MAKEWORD(2, 2);
	if (WSAStartup(vVersion, &wsaData)) {
		printf("Version is not supported.\n");
	}

	//Construct socket
	SOCKET listenSock;
	listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	//Bind address to socket
	bindAddress(listenSock, SERVER_ADDR, atoi(argv[1]));

	//Listen request from client
	if (listen(listenSock, SOMAXCONN)) {
		printf("Error: %d", WSAGetLastError());
		return 0;
	}
	printf("Server started!\n");

	//Initiate critical section
	InitializeCriticalSection(&critical);

	//Take and store the data
	users = inputUser(FILE_NAME);

	//Communicate with client
	argList param;
	param.sock = listenSock;
	param.offset = 0;

	for (int i = 0; i < 4096; i++) {
		client[i] = 0;	// 0 indicates available entry
	}

	while (1) {
		//Accept request
		if (numClient >= 4096) {
			printf("Can not accept more client!\n");
		}
		else if (numThread * FD_SETSIZE <= numClient) {
			param.offset = numThread * FD_SETSIZE;
			_beginthreadex(0, 0, childThread, (void *)&param, 0, 0); //start thread
			numThread++;
		}
	}
	//Delete critical section
	DeleteCriticalSection(&critical);

	//Close socket
	closesocket(listenSock);

	//Terminate Winsock
	WSACleanup();

	return 0;
}