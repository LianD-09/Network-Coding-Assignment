// Server.cpp : Defines the entry point for the console application.
//

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

#define MAX_CLIENT 2048
#define SERVER_ADDR "127.0.0.1"
#define DATA_BUFSIZE 2048
#define RECEIVE 0
#define SEND 1
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

#pragma comment(lib, "Ws2_32.lib")

// Structure definition
typedef struct {
	WSAOVERLAPPED overlapped;
	WSABUF dataBuff;
	CHAR buffer[DATA_BUFSIZE];
	int bufLen;
	int operation;
} PER_IO_OPERATION_DATA, *LPPER_IO_OPERATION_DATA;

typedef struct {
	SOCKET socket;
} PER_HANDLE_DATA, *LPPER_HANDLE_DATA;

struct state {
	string account = ""; //Save the account using
	bool isLogin = false; //Save the state of client
	string recvMess = ""; //A string contain all messages
} status[MAX_CLIENT];

SOCKET socks[MAX_CLIENT];

CRITICAL_SECTION critical;

// Number of connection socket
int numSock = 0;

//Store the value of accounts
map<string, int> users;

/**
* @funtion bindAddress: Bind a address for socket.
*
* @param listenSock: Socket need to be bound.
* @param ipAddress: A pointer to the string containing the address used to bind.
* @param port: The number of port.
* @return: No return value.
**/
void bindAddress(SOCKET listenSock, char* ipAddress, int port) {
	SOCKADDR_IN serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(port);
	inet_pton(AF_INET, ipAddress, &serverAddr.sin_addr);
	if (bind(listenSock, (PSOCKADDR)&serverAddr, sizeof(serverAddr))) {
		printf("Error! Cannot bind this address.\n");
		exit(0);
	}
}

/**
* @function acceptRequest: Accept a connect request
*
* @param listenSock: The listening socket of server
* @param socks: An array of list client socket
* @param events: An array of event assigned with client socket
* @param nEvents: The number of current events
* @return: No value return
**/
int acceptRequest(SOCKET listenSock, HANDLE completionPort, LPPER_HANDLE_DATA perHandleData) {
	SOCKADDR_IN clientAddr;
	int clientAddrLen = sizeof(clientAddr),
		clientPort;
	char clientIP[INET_ADDRSTRLEN];

	SOCKET acceptSock = WSAAccept(listenSock, (SOCKADDR *)&clientAddr, (LPINT)&clientAddrLen, NULL, 0);
	if (acceptSock == SOCKET_ERROR) {
		printf("WSAAccept() failed with error %d\n", WSAGetLastError());
		return -1;
	}
	else {
		inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, sizeof(clientIP));
		clientPort = ntohs(clientAddr.sin_port);

		// Associate the accepted socket with the original completion port
		printf("Socket number %d got connected from [%s:%d]\n", acceptSock, clientIP, clientPort);
		perHandleData->socket = acceptSock;
		if (CreateIoCompletionPort((HANDLE)acceptSock, completionPort, (DWORD)perHandleData, 0) == NULL) {
			printf("CreateIoCompletionPort() failed with error %d\n", GetLastError());
			exit(0);
		}

		int i = ((int)acceptSock / 4) % MAX_CLIENT;
		if (socks[i] == 0) {
			EnterCriticalSection(&critical);
			socks[i] = acceptSock;
			numSock++;
			LeaveCriticalSection(&critical);
			return i;
		}
	}
	return MAX_CLIENT;
}

/**
* @function Receive: Receive message from client.
*
* @param connSock: Socket of client.
* @param buff: A string contains the echo message.
* @param appendMess: A string will contains full of message.
* @return: A integer represents the number of bytes received.
**/
BOOL Receive(SOCKET sock, LPPER_IO_OPERATION_DATA perIoData, DWORD &transferredBytes) {
	BOOL n;
	DWORD flags = 0;

	ZeroMemory(&(perIoData->overlapped), sizeof(OVERLAPPED));
	perIoData->dataBuff.len = DATA_BUFSIZE;
	perIoData->dataBuff.buf = perIoData->buffer;
	perIoData->operation = RECEIVE;

	n = WSARecv(sock, &(perIoData->dataBuff), 1, &transferredBytes, &flags, &(perIoData->overlapped), NULL);

	//printf("%d\n", n);
	if (n == SOCKET_ERROR) {
		if (WSAGetLastError() != ERROR_IO_PENDING) {
			printf("WSARecv() failed with error %d\n", WSAGetLastError());
			return 1;
		}
	}
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
BOOL Send(LPPER_HANDLE_DATA perHandleData, LPPER_IO_OPERATION_DATA perIoData, DWORD &transferredBytes, char* replyMess) {
	BOOL n;
	ZeroMemory(&(perIoData->overlapped), sizeof(OVERLAPPED));
	memset(perIoData->buffer, 0, DATA_BUFSIZE);
	memcpy(perIoData->buffer, replyMess, sizeof(replyMess));
	perIoData->dataBuff.buf = perIoData->buffer;
	perIoData->operation = SEND;

	n = WSASend(perHandleData->socket, &(perIoData->dataBuff), 1, &transferredBytes, 0, &(perIoData->overlapped), NULL);
	if (n == SOCKET_ERROR) {
		if (WSAGetLastError() != ERROR_IO_PENDING) {
			printf("WSASend() failed with error %d\n", WSAGetLastError());
			return 1;
		}
	}
	return n;
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
char* handleReply(char *header, char *message, int offset) {
	char replyMess[DATA_BUFSIZE];
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
				//Enter the critical section
				EnterCriticalSection(&critical);
				status[offset].account = message;
				status[offset].isLogin = true;
				users[status[offset].account] = -1;
				//Leave the critical section
				LeaveCriticalSection(&critical);
				strcpy_s(replyMess, sizeof(replyMess), SUCCESS_LOGIN);
			}
		}
	}
	//Header POST
	else if (!strcmp(header, POST_HEADER)) {
		if (!status[offset].isLogin) {
			strcpy_s(replyMess, sizeof(replyMess), NOT_LOGIN);
		}
		else {
			strcpy_s(replyMess, sizeof(replyMess), SUCCESS_POST);
		}
	}
	//Header BYE
	else if (!strcmp(header, LOGOUT_HEADER)) {
		if (!status[offset].isLogin) {
			strcpy_s(replyMess, sizeof(replyMess), NOT_LOGIN);
		}
		else {
			//Enter the critical section
			EnterCriticalSection(&critical);
			users[status[offset].account] = 0;
			status[offset].account = "";
			status[offset].isLogin = false;
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
* @function handleStream: Handle stream by using delimiter.
*
* @param storeMess: A string contains correct message or a variety of messages.
* @param recvMess: A vector contains all of messages splitted from storeMess param.
* @return: No value return.
**/
void handleStream(char* storeMess, int pos, LPPER_HANDLE_DATA perHandleData, LPPER_IO_OPERATION_DATA perIoData, DWORD &transferredBytes) {
	char	*delim = ENDING_DELIMITER,
		*next_message, //Next message
		*message = strtok_s(storeMess, delim, &next_message); //Current message delimited by delim

	do {
		printf("Receive from client: %s\n", message);
		//Split header and body
		char	*body,
			*header = strtok_s(message, " ", &body);
		char replyMess[DATA_BUFSIZE] = "";
		//Create and send reply message
		memcpy(replyMess, handleReply(header, body, pos), sizeof(replyMess));
		Send(perHandleData, perIoData, transferredBytes, replyMess);
		message = strtok_s(NULL, delim, &next_message);
	} while (message != NULL);
}

/**
* @function: Close a soket of client and update values.
*
* @param i: The offset of client.
* @return: No value return.
**/
BOOL closeClient(int i, LPPER_HANDLE_DATA perHandleData, LPPER_IO_OPERATION_DATA perIoData) {
	//If client is closed or disconnects, reset status of account to log out
	if (closesocket(socks[i]) == SOCKET_ERROR) {
		printf("closesocket() failed with error %d\n", WSAGetLastError());
		return 0;
	}
	GlobalFree(perHandleData);
	GlobalFree(perIoData);
	
	//Enter the critical section
	EnterCriticalSection(&critical);
	users[status[i].account] = 0;
	status[i].isLogin = false;
	status[i].account = "";
	status[i].recvMess = "";
	numSock--;
	socks[i] = 0;
	//Leave the critical section
	LeaveCriticalSection(&critical);

	return 1;
}

unsigned __stdcall serverWorkerThread(LPVOID completionPortID)
{
	int offs;
	HANDLE completionPort = (HANDLE)completionPortID;
	DWORD transferredBytes;
	DWORD flags = 0;
	LPPER_HANDLE_DATA perHandleData;
	LPPER_IO_OPERATION_DATA perIoData;

	while (1) {
		//Sleep(10);
		if (GetQueuedCompletionStatus(completionPort, &transferredBytes,
			(LPDWORD)&perHandleData, (LPOVERLAPPED *)&perIoData, INFINITE) == 0) {
			printf("GetQueuedCompletionStatus() failed with error %d\n", GetLastError());
			return 0;
		}
		offs = ((int)(perHandleData->socket) / 4) % MAX_CLIENT;

		// Check to see if the operation field equals RECEIVE. If this is so, then
		// this means a WSARecv call just completed so update the recvBytes field
		// with the transferredBytes value from the completed WSARecv() call
		/*if (perIoData->operation == RECEIVE) {
			perIoData->recvBytes = transferredBytes;
			perIoData->sentBytes = 0;
			perIoData->operation = SEND;
		}
		else if (perIoData->operation == SEND) {
			perIoData->sentBytes += transferredBytes;
		}*/
		perIoData->bufLen = transferredBytes;

		// Check to see if an error has occurred on the socket and if so
		// then close the socket and cleanup the SOCKET_INFORMATION structure
		// associated with the socket
		if (transferredBytes == 0) {
			printf("Closing socket %d\n", perHandleData->socket);
			closeClient(offs, perHandleData, perIoData);
			continue;
		}

		if (perIoData->operation == RECEIVE) {
			if (perIoData->bufLen > 0) {
				*(perIoData->dataBuff.buf + perIoData->bufLen) = 0;
				status[offs].recvMess.append(perIoData->dataBuff.buf);
			}

			char *message = _strdup(status[offs].recvMess.c_str());
			if (!memcmp(message + strlen(message) - strlen(ENDING_DELIMITER), ENDING_DELIMITER, strlen(ENDING_DELIMITER))) {
				char *storeMess = _strdup(status[offs].recvMess.c_str());
				handleStream(storeMess, offs, perHandleData, perIoData, transferredBytes);
				status[offs].recvMess = "";
			}
			else {
				//printf("%s ---- 1\n", perIoData->dataBuff.buf);
				Receive(perHandleData->socket, perIoData, transferredBytes);
			}
		}
		else if (perIoData->operation == SEND) {
			Receive(perHandleData->socket, perIoData, transferredBytes);
		}

		/*if (perIoData->recvBytes > perIoData->sentBytes) {
			// Post another WSASend() request.
			// Since WSASend() is not guaranteed to send all of the bytes requested,
			// continue posting WSASend() calls until all received bytes are sent.
			ZeroMemory(&(perIoData->overlapped), sizeof(OVERLAPPED));
			perIoData->dataBuff.buf = perIoData->buffer + perIoData->sentBytes;
			perIoData->dataBuff.len = perIoData->recvBytes - perIoData->sentBytes;
			perIoData->operation = SEND;

			if (WSASend(perHandleData->socket,
				&(perIoData->dataBuff),
				1,
				&transferredBytes,
				0,
				&(perIoData->overlapped),
				NULL) == SOCKET_ERROR) {
				if (WSAGetLastError() != ERROR_IO_PENDING) {
					printf("WSASend() failed with error %d\n", WSAGetLastError());
					return 0;
				}
			}
		}
		else {
			// No more bytes to send post another WSARecv() request
			perIoData->recvBytes = 0;
			perIoData->operation = RECEIVE;
			ZeroMemory(&(perIoData->overlapped), sizeof(OVERLAPPED));
			perIoData->dataBuff.len = DATA_BUFSIZE;
			perIoData->dataBuff.buf = perIoData->buffer;
			if (WSARecv(perHandleData->socket,
				&(perIoData->dataBuff),
				1,
				&transferredBytes,
				&flags,
				&(perIoData->overlapped), NULL) == SOCKET_ERROR) {
				if (WSAGetLastError() != ERROR_IO_PENDING) {
					printf("WSARecv() failed with error %d ---- 2\n", WSAGetLastError());
					return 0;
				}
			}
		}*/
	}
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
		char line[DATA_BUFSIZE] = "";
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

int _tmain(int argc, _TCHAR* argv[])
{
	//SOCKADDR_IN serverAddr;
	int i;
	SOCKET listenSock;
	HANDLE completionPort;
	SYSTEM_INFO systemInfo;
	LPPER_HANDLE_DATA perHandleData;
	LPPER_IO_OPERATION_DATA perIoData;
	DWORD transferredBytes = 0;
	DWORD flags;
	WSADATA wsaData;

	if (WSAStartup((2, 2), &wsaData) != 0) {
		printf("WSAStartup() failed with error %d\n", GetLastError());
		return 1;
	}

	// Step 1: Setup an I/O completion port
	if ((completionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0)) == NULL) {
		printf("CreateIoCompletionPort() failed with error %d\n", GetLastError());
		return 1;
	}

	// Step 2: Determine how many processors are on the system
	GetSystemInfo(&systemInfo);

	// Step 3: Create worker threads based on the number of processors available on the
	// system. Create two worker threads for each processor	
	for (int i = 0; i < (int)systemInfo.dwNumberOfProcessors * 2; i++) {
		// Create a server worker thread and pass the completion port to the thread
		if (_beginthreadex(0, 0, serverWorkerThread, (void*)completionPort, 0, 0) == 0) {
			printf("Create thread failed with error %d\n", GetLastError());
			return 1;
		}
	}

	// Step 4: Create a listening socket
	if ((listenSock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED)) == INVALID_SOCKET) {
		printf("WSASocket() failed with error %d\n", WSAGetLastError());
		return 1;
	}

	bindAddress(listenSock, SERVER_ADDR, _tstoi(argv[1]));

	// Prepare socket for listening
	if (listen(listenSock, SOMAXCONN) == SOCKET_ERROR) {
		printf("listen() failed with error %d\n", WSAGetLastError());
		return 1;
	}
	printf("Server started!\n");

	//Initiate critical section
	InitializeCriticalSection(&critical);

	//Take and store the data
	users = inputUser(FILE_NAME);

	while (1) {
		// Create a socket information structure to associate with the socket
		if ((perHandleData = (LPPER_HANDLE_DATA)GlobalAlloc(GPTR, sizeof(PER_HANDLE_DATA))) == NULL) {
			printf("GlobalAlloc() failed with error %d\n", GetLastError());
			exit(0);
		}

		// Step 5: Accept connections
		if (numSock < MAX_CLIENT) {
			i = acceptRequest(listenSock, completionPort, perHandleData);

			// Step 8: Create per I/O socket information structure to associate with the WSARecv call
			if ((perIoData = (LPPER_IO_OPERATION_DATA)GlobalAlloc(GPTR, sizeof(PER_IO_OPERATION_DATA))) == NULL) {
				printf("GlobalAlloc() failed with error %d\n", GetLastError());
				return 1;
			}

			if (i != -1) {
				Receive(socks[i], perIoData, transferredBytes);
				/*if (WSARecv(socks[i], &(perIoData->dataBuff), 1, &transferredBytes, &flags, &(perIoData->overlapped), NULL) == SOCKET_ERROR) {
					if (WSAGetLastError() != ERROR_IO_PENDING) {
						printf("WSARecv() failed with error %d ---- 1\n", WSAGetLastError());
						return 1;
					}
				}*/
			}
		}
		else {
			printf("Limited connection! Can not accept more connection.\n");
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
