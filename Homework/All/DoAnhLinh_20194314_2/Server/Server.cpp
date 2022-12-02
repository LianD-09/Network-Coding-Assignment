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
		else{
			int status = users[message];
			if (status == 1) {
				strcpy_s(replyMess, sizeof(replyMess), LOCKED_LOGIN);
			}
			else if (status == -1) {
				strcpy_s(replyMess, sizeof(replyMess), LOGGING_IN);
			}
			else if (status == 0) {
				account = message;
				users[account] = -1;
				isLogin = true;
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
			users[account] = 0;
			isLogin = false;
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
* @funtion echoThread: A single thread to handle request from each client.
*
* @param param: A pointer to the socket.
* @return: 0.
**/
unsigned __stdcall echoThread(void *param) {
	char buff[BUFF_SIZE];
	int ret;
	string account = ""; //Save the account using
	bool isLogin = false; //Save the state of client

	SOCKET connSock = (SOCKET)param;
	while (1) {
		//A string contain all messages
		string appendMess = "";
		vector<char*> recvMess;
		do {
			ret = recv(connSock, buff, BUFF_SIZE, 0);
			if (ret == SOCKET_ERROR) {
				printf("Error %d: Cannot receive data.\n", WSAGetLastError());
				//If client is closed, reset status of account to log out
				users[account] = 0;
				closesocket(connSock);
				return 0;
			}
			else if (strlen(buff) > 0) {
				buff[ret] = 0;
				appendMess.append(buff);
			}
		} while (!strstr(buff + strlen(buff) - strlen(ENDING_DELIMITER), ENDING_DELIMITER) && ret != 0);
		//Break if client disconnects
		if (ret == 0) {
			printf("Client disconnects.\n");
			//If client is closed, reset status of account to log out
			users[account] = 0;
			break;
		}
		//Convert into char * type to handle
		char *storeMess = _strdup(appendMess.c_str());
		if (strlen(storeMess) > 0){
			handleStream(storeMess, recvMess);
			for (auto it : recvMess) {
				printf("Receive from client: %s\n", it);
				//Split header and body
				char *message,
					*header = strtok_s(it, " ", &message);
				char replyMess[BUFF_SIZE] = "";
				//Create and send reply message
				strcpy_s(replyMess, sizeof(replyMess), handleReply(header, message, account, isLogin));
				ret = send(connSock, replyMess, strlen(replyMess), 0);
				if (ret == SOCKET_ERROR)
					printf("Error %d: Cannot send data.\n", WSAGetLastError());
			}
		}
	}
	closesocket(connSock);
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

	//Take and store the data
	users = inputUser(FILE_NAME);

	//Communicate with client
	sockaddr_in clientAddr;
	char clientIP[INET_ADDRSTRLEN];
	int clientAddrLen = sizeof(clientAddr), clientPort;
	SOCKET connSock;
	while (1) {
		//Accept request
		connSock = accept(listenSock, (sockaddr *)&clientAddr, &clientAddrLen);
		if (connSock == SOCKET_ERROR)
			printf("Error %d: Cannot permit incoming connection.\n", WSAGetLastError());
		else {
			inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, sizeof(clientIP));
			clientPort = ntohs(clientAddr.sin_port);
			printf("Accept incoming connection from %s:%d\n", clientIP, clientPort);
			_beginthreadex(0, 0, echoThread, (void *) connSock, 0, 0); //start thread
		}
	}
	//Close socket
	closesocket(connSock);
	closesocket(listenSock);

	//Terminate Winsock
	WSACleanup();

	return 0;
}