// UDP Client.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "string"
#include "stdio.h"
#include "winsock2.h"
#include "ws2tcpip.h"
#define BUFF_SIZE 2048
#pragma comment (lib, "Ws2_32.lib")

sockaddr_in constructAddress(char* ipAddress, int port) {
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(port);
	inet_pton(AF_INET, ipAddress, &serverAddr.sin_addr);
	return serverAddr;
}

void communicateServer(SOCKET client, sockaddr_in &serverAddr) {
	char buff[BUFF_SIZE];
	int ret, serverAddrLen = sizeof(serverAddr);
	while (1) {
		//Send message
		printf("Send to server: ");
		gets_s(buff, BUFF_SIZE);
		//Break if input empty message
		if (buff[0] == '\0') {
			break;
		}
		ret = sendto(client, buff, strlen(buff), 0, (sockaddr *)&serverAddr, serverAddrLen);
		if (ret == SOCKET_ERROR) {
			printf("Error! Cannot send mesage.\n");
		}
		//Receive echo message
		do {
			ret = recvfrom(client, buff, BUFF_SIZE, 0,
				(sockaddr *)&serverAddr, &serverAddrLen);
			if (ret == SOCKET_ERROR) {
				if (WSAGetLastError() == WSAETIMEDOUT)
					printf("Time-out!");
				else printf("Error! Cannot receive message.\n");
			}
			else {
				buff[ret] = '\0';
				// Break if server send empty message 
				// representing the last message sent
				if (buff[0] == '\0') {
					break;
				}
				printf("Receive IP Address: %s\n", buff + 1);
			}
		} while (1);
	}
}

int main(int argc, char* argv[])
{
	//Inittiate WinSock
	WSADATA wsaData;
	WORD wVersion = MAKEWORD(2, 2);
	if (WSAStartup(wVersion, &wsaData)) {
		printf("Version is not supported.\n");
	}
	printf("UDP Client started!\n");

	//Construct socket
	SOCKET client;
	client = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	//Set time-out for receiving
	int tv = 5000; //Time-out interval: 5000ms
	setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char*)(&tv), sizeof(int));

	//Specify server address
	sockaddr_in serverAddr = constructAddress(argv[1], atoi(argv[2]));

	//Communicate with server
	communicateServer(client, serverAddr);
	
	//Close socket
	closesocket(client);
	
	//Terminate Winsock
	WSACleanup();
    return 0;
}