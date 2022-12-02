// TCP Server.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "winsock2.h"
#include "ws2tcpip.h"
#include "stdio.h"
#include "stdlib.h"
#define SERVER_ADDR "127.0.0.1"
#define BUFF_SIZE 1024
#define ENDING_DELIMITER "\r\n"
#define ERROR_MESSAGE "Failed: String contains non-number character."
#pragma comment (lib, "Ws2_32.lib")

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

char* sumDigits(char* buff) {
	int sum = 0;
	char sumStr[BUFF_SIZE];
	for (int i = 0; i < strlen(buff); i++) {
		sum += buff[i] - 48;
	}
	_itoa_s(sum, sumStr, BUFF_SIZE, 10);
	return sumStr;
}

//Handle stream
void handleStream(char* buff, int &ret, SOCKET connSock, char* clientIP, int clientPort) {
	char* delim = ENDING_DELIMITER,
		*next_message, //Next message
		*message = strtok_s(buff, delim, &next_message); //Current message delimited by delim
	while (message != NULL) {
		//Inittiate prefix 
		char echoMess[BUFF_SIZE] = "+";
		int i = 0;
		while (message[i] != '\0') {
			if (message[i] < 48 || message[i] > 57) {
				//Failed prefix and create failed message
				echoMess[0] = '-';
				strcat_s(echoMess, BUFF_SIZE, ERROR_MESSAGE);
				break;
			}
			i++;
		}

		if (ret == SOCKET_ERROR) {
			printf("Error %d", WSAGetLastError());
			break;
		}
		else {
			if (echoMess[0] == '+') {
				//Create success message
				strcat_s(echoMess, BUFF_SIZE, sumDigits(message));
			}

			printf("Receive from client[%s:%d] %s\n", clientIP, clientPort, message);
			//Echo to client
			ret = send(connSock, echoMess, strlen(echoMess), 0);
			if (ret == SOCKET_ERROR) {
				printf("Error %d", WSAGetLastError());
				break;
			}
		}
		message = strtok_s(NULL, delim, &next_message);
	}
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
	printf("TCP Server started!\n");

	//Communicate with client
	sockaddr_in clientAddr;
	char buff[BUFF_SIZE], clientIP[INET_ADDRSTRLEN];
	int ret, clientAddrLen = sizeof(clientAddr), clientPort;
	SOCKET connSock;
	while (1) {
		//Accept request
		connSock = accept(listenSock, (sockaddr *)& clientAddr, &clientAddrLen);
		inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, sizeof(clientIP));
		clientPort = ntohs(clientAddr.sin_port);

		while (1) {
			//receive message from client
			ret = recv(connSock, buff, BUFF_SIZE, 0);
			//Check if client close
			if (strstr(buff, ENDING_DELIMITER) && strlen(buff) == strlen(ENDING_DELIMITER)) {
				break;
			}
			buff[ret] = '\0';
			handleStream(buff, ret, connSock, clientIP, clientPort);
		}
		//Send mark last message
			ret = send(connSock, ENDING_DELIMITER, strlen(ENDING_DELIMITER), 0);
	}
	//Close socket
	closesocket(connSock);
	closesocket(listenSock);

	//Terminate Winsock
	WSACleanup();

    return 0;
}

