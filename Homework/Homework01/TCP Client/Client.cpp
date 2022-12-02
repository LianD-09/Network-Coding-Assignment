// TCP Client.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "stdio.h"
#include "winsock2.h"
#include "ws2tcpip.h"
#define BUFF_SIZE 1024
#define ENDING_DELIMITER "\r\n"
#pragma comment (lib, "Ws2_32.lib")

sockaddr_in constructAddress(char* ipAddress, int port) {
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(port);
	inet_pton(AF_INET, ipAddress, &serverAddr.sin_addr);
	return serverAddr;
}

void receiveMessage(SOCKET client, char *buff, int &ret) {
	ret = recv(client, buff, BUFF_SIZE, 0);
	if (ret == SOCKET_ERROR) {
		if (WSAGetLastError() == WSAETIMEDOUT) {
			printf("Time-out!\n");
		}
		else {
			printf("Error %d", WSAGetLastError());
		}
	}
	else if (strlen(buff) > 0) {
		buff[ret] = 0;
		printf("Receive from server: %s\n", buff + 1);
	}
}

int main(int argc, char* argv[])
{
	//Inittiate Winsock
	WSADATA wsaData;
	WORD wVersion = MAKEWORD(2, 2);
	if (WSAStartup(wVersion, &wsaData)) {
		printf("Version is not supported\n");
	}

	//Construct socket
	SOCKET client;
	client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	//Set time-out for receiving
	int tv = 5000; //Time-out interval: 5000ms
	setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char*)(&tv), sizeof(int));

	//Specify server address
	sockaddr_in serverAddr = constructAddress(argv[1], atoi(argv[2]));

	//Request to connect server
	if (connect(client, (sockaddr *)&serverAddr, sizeof(serverAddr))) {
		printf("Error! Cannot connect server.");
		return 0;
	}

	//Communicate with server
	char buff[1024];
	int ret, messageLen;

	//Send message
	while (1) {
		//Send message
		printf("Send to server: ");
		gets_s(buff, BUFF_SIZE);
		messageLen = strlen(buff);
		//Break if input empty message
		if (messageLen == 0) {
			//Send message close 
			ret = send(client, ENDING_DELIMITER, strlen(ENDING_DELIMITER), 0);
			break;
		}

		//Add delimiter and send message
		strcat_s(buff, BUFF_SIZE, ENDING_DELIMITER);
		ret = send(client, buff, messageLen + strlen(ENDING_DELIMITER), 0);
		if (ret == SOCKET_ERROR)
			printf("Error %d", WSAGetLastError());

		//Receive echo message
		receiveMessage(client, buff, ret);
	}

	//Close socket
	closesocket(client);

	//Terminate Winsock
	WSACleanup();

    return 0;
}

