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
#define MAX_CLIENTS 1024
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

int numClient = 0, //Number of client is connecting
	numThread = 0; //Number of using thread

struct state {
	string account = ""; //Save the account using
	bool isLogin = false; //Save the state of client
} status[1024];

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
* @function acceptRequest: Accept a connect request
* 
* @param listenSock: The listening socket of server
* @param socks: An array of list client socket
* @param events: An array of event assigned with client socket
* @param nEvents: The number of current events
* @return: No value return
**/
void acceptRequest(SOCKET listenSock, SOCKET *socks, WSAEVENT *events, DWORD &nEvents) {
	sockaddr_in clientAddr;
	int clientAddrLen = sizeof(clientAddr), 
		clientPort;
	char clientIP[INET_ADDRSTRLEN];

	SOCKET connSock = accept(listenSock, (sockaddr *)&clientAddr, &clientAddrLen);
	if (connSock == SOCKET_ERROR) {
		printf("Error %d: Cannot permit incoming connection.\n", WSAGetLastError());
	}
	else {
		inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, sizeof(clientIP));
		clientPort = ntohs(clientAddr.sin_port);
		printf("Accept incoming connection from %s:%d\n", clientIP, clientPort);

		for (int i = 1; i < WSA_MAXIMUM_WAIT_EVENTS; i++) {
			if (socks[i] == 0) {
				socks[i] = connSock;
				WSAEventSelect(socks[i], events[i], FD_READ | FD_CLOSE);
				EnterCriticalSection(&critical);
				numClient++;
				LeaveCriticalSection(&critical);
				nEvents++;
				break;
			}
		}
	}
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
			int err = WSAGetLastError();
			if (err != WSAEWOULDBLOCK) {
				printf("Error %d: Cannot receive data.\n", err);
				return n;
			}
		}
		else if (strlen(buff) > 0) {
			buff[n] = 0;
			appendMess.append(buff);
		}
	} while (!!strcmp(buff + strlen(buff) - strlen(ENDING_DELIMITER), ENDING_DELIMITER) && n != 0);

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
* @function handleReply: Handle reply message contains header and body.
*
* @param header: A string represents the header of message.
* @param message: A string contains the value of body.
* @param account: A string will contain the account name.
* @param isLogin: A value marks current client is logging in or not.
* @return: A string represents the reply message.
**/
char* handleReply(char *header, char *message, int offset) {
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
void handleStream(char* storeMess, int pos, SOCKET &sock) {
	char	*delim = ENDING_DELIMITER,
			*next_message, //Next message
			*message = strtok_s(storeMess, delim, &next_message); //Current message delimited by delim

	do {
		printf("Receive from client: %s\n", message);
		//Split header and body
		char	*body,
				*header = strtok_s(message, " ", &body);
		char replyMess[BUFF_SIZE] = "";
		//Create and send reply message
		strcpy_s(replyMess, sizeof(replyMess), handleReply(header, body, pos));
		Send(sock, replyMess, strlen(replyMess), 0);
		message = strtok_s(NULL, delim, &next_message);
	} while (message != NULL);
}

/**
* @function: Close a soket of client and update values.
*
* @param i: The offset of client.
* @return: No value return.
**/
void closeClient(int i, SOCKET &sock, WSAEVENT &event) {
	printf("Client disconnects.\n");
	//If client is closed or disconnects, reset status of account to log out
	closesocket(sock);
	//Enter the critical section
	EnterCriticalSection(&critical);
	users[status[i].account] = 0;
	status[i].isLogin = false;
	status[i].account = "";
	numClient--;
	//Leave the critical section
	LeaveCriticalSection(&critical);
	sock = 0;
	WSACloseEvent(event);
	event = WSACreateEvent();
}

/**
* @funtion finishThread: Close events and handle before close thread
* @param events: An array of event assigned with client socket.
* @return: No value return.
**/
void finishThread(WSAEVENT *events) {
	EnterCriticalSection(&critical);
	numThread--;
	LeaveCriticalSection(&critical);

	for (int i = 0; i < WSA_MAXIMUM_WAIT_EVENTS; i++) {
		WSACloseEvent(events[i]);
	}
}

/**
* @funtion childThread: A single thread to handle request from each client.
*
* @param param: A pointer to a struct that contains a socket and a offset value.
* @return: 0.
**/
unsigned __stdcall childThread(void *param) {
	char buff[BUFF_SIZE];
	argList *arg = (argList *)param;
	int	ret;

	WSANETWORKEVENTS sockEvent;
	DWORD index;
	DWORD	nEvents = 1;
	WSAEVENT events[WSA_MAXIMUM_WAIT_EVENTS];
	SOCKET	listenSock = arg->sock,
			socks[WSA_MAXIMUM_WAIT_EVENTS]; // Store connected soket to client

	socks[0] = listenSock;
	events[0] = WSACreateEvent(); // Create new events
	WSAEventSelect(socks[0], events[0], FD_ACCEPT | FD_CLOSE);

	//Initiate elements of socks array and events array
	for (int i = 1; i < WSA_MAXIMUM_WAIT_EVENTS; i++) {
		socks[i] = 0;
		events[i] = WSACreateEvent();
	}

	while (1) {
		index = WSAWaitForMultipleEvents(nEvents, events, FALSE, WSA_INFINITE, FALSE);
		if (index == WSA_WAIT_FAILED) {
			printf("Error %d: WSAWaitForMultipleEvents() failed\n", WSAGetLastError());
			continue;
		}

		index = index - WSA_WAIT_EVENT_0;
		WSAEnumNetworkEvents(socks[index], events[index], &sockEvent);
		
		//Accept request
		if (sockEvent.lNetworkEvents & FD_ACCEPT) {
			if (nEvents < WSA_MAXIMUM_WAIT_EVENTS) {
				if (sockEvent.iErrorCode[FD_ACCEPT_BIT] != 0) {
					printf("FD_ACCEPT failed with error %d\n", sockEvent.iErrorCode[FD_READ_BIT]);
					continue;
				}
				acceptRequest(listenSock, socks, events, nEvents);
				//Reset event
				WSAResetEvent(events[index]);
			}
		}

		else if (sockEvent.lNetworkEvents & FD_READ) {
			//Receive message from client
			if (sockEvent.iErrorCode[FD_READ_BIT] != 0) {
				printf("FD_READ failed with error %d\n", sockEvent.iErrorCode[FD_READ_BIT]);
				closeClient(arg->offset + index - 1, socks[index], events[index]);
				//End if the current clients is 0
				if (--nEvents <= 1 && numThread > 1) {
					finishThread(events);
					return 0;
				}
				continue;
			}
			//A string contain all messages
			string appendMess = "";

			//Position of socket in socks array
			int pos = arg->offset + index - 1;
			ret = Receive(socks[index], buff, appendMess);

			//Break if client disconnects
			if (ret <= 0) {
				closeClient(arg->offset + index - 1, socks[index], events[index]);
				//End if the current clients is 0
				if (--nEvents <= 1 && numThread > 1) {
					finishThread(events);
					return 0;
				}
				continue;
			}
			else {
				//Convert into char * type to handle
				char *storeMess = _strdup(appendMess.c_str());
				handleStream(storeMess, pos, socks[index]);
			}
			//reset event
			WSAResetEvent(events[index]);
		}

		else if (sockEvent.lNetworkEvents & FD_CLOSE) {
			if (sockEvent.iErrorCode[FD_CLOSE_BIT] != 0) {
				printf("FD_CLOSE failed with error %d\n", sockEvent.iErrorCode[FD_CLOSE_BIT]);
				for (int i = 0; i < WSA_MAXIMUM_WAIT_EVENTS; i++) {
					WSAResetEvent(events[i]);
				}
			}
			//Release socket
			closeClient(arg->offset + index - 1, socks[index], events[index]);
			//End if the current clients is 0
			if (--nEvents <= 1 && numThread > 1) {
				//Release event and finish current thread
				finishThread(events);
				return 0;
			}
		}
	}
	//Release event and finish current thread
	finishThread(events);
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
	//Initiate Winsock
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

	while (1) {
		//Accept request
		if (numClient >= 1024) {
			printf("Can not accept more client!\n");
		}
		else if (numThread * (WSA_MAXIMUM_WAIT_EVENTS - 1) <= numClient) {
			param.offset = numThread * WSA_MAXIMUM_WAIT_EVENTS;
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