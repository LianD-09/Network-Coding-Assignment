// UDP Server.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "stdio.h"
#include "string.h"
#include "winsock2.h"
#include "ws2tcpip.h"
#define SERVER_ADDR "127.0.0.1"
#define BUFF_SIZE 2048
#pragma comment (lib, "Ws2_32.lib")

sockaddr_in bindAddress(SOCKET server, char* ipAddress, int port) {
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(port);
	inet_pton(AF_INET, ipAddress, &serverAddr.sin_addr);
	if (bind(server, (sockaddr *)&serverAddr, sizeof(serverAddr))) {
		printf("Error! Cannot bind this address.\n");
		exit(0);
	}
	return serverAddr;
}

void receiveMessage(SOCKET server, char* buff, sockaddr_in &clientAddr, int &ret) {
	char clientIP[INET_ADDRSTRLEN];
	int clientAddrLen = sizeof(clientAddr), clientPort;
	ret = recvfrom(server, buff, BUFF_SIZE, 0,
		(sockaddr *)&clientAddr, &clientAddrLen);
	if (ret == SOCKET_ERROR)
		printf("Error : %s", WSAGetLastError());
	else {
		buff[ret] = 0;
		inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP,
			sizeof(clientIP));
		clientPort = ntohs(clientAddr.sin_port);
		printf("Receive from client %s:%d %s\n",
			clientIP, clientPort, buff);
	}
}

void resolveHost(SOCKET server, char* buff, sockaddr_in &clientAddr, int &ret) {
	addrinfo *res;
	int rc;
	sockaddr_in *address;
	addrinfo hints = { 0 };
	hints.ai_family = AF_INET; //only focus on IPv4 address
	rc = getaddrinfo(buff, NULL, &hints, &res);
	char ipStr[INET_ADDRSTRLEN];
	char errorMessage[BUFF_SIZE] = "Not found information";
	if (rc == 0) {
		for (addrinfo *addr = res; addr != nullptr; addr = addr->ai_next) {
			address = (struct sockaddr_in *) addr->ai_addr;
			inet_ntop(AF_INET, &address->sin_addr, ipStr, sizeof(ipStr));

			//Echo to client
			char echoMes[BUFF_SIZE] = "";
			echoMes[0] = '+';
			ret = strlen(ipStr);
			ipStr[ret] = '\0';
			strcat_s(echoMes, BUFF_SIZE, ipStr);
			ret = sendto(server, echoMes, ret, 0, (SOCKADDR *)&clientAddr, sizeof(clientAddr));
			if (ret == SOCKET_ERROR)
				printf("Error: %", WSAGetLastError());

			//printf("%s\n", ipStr);
		}
	}
	else {
		char echoMes[BUFF_SIZE] = "";
		echoMes[0] = '-';
		strcat_s(echoMes, BUFF_SIZE, errorMessage);
		ret = sendto(server, echoMes, strlen(echoMes), 0, (SOCKADDR *)&clientAddr, sizeof(clientAddr));
		if (ret == SOCKET_ERROR)
			printf("Error: %", WSAGetLastError());
	}
}

int main(int argc, char* argv[]) {
	//Inittiate WinSock
	WSADATA wsaData;
	WORD wVersion = MAKEWORD(2, 2);
	if (WSAStartup(wVersion, &wsaData))
		printf("Version is not supported\n");

	//Construct socket
	SOCKET server;
	server = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	//Bind address to socket
	sockaddr_in serverAddr = bindAddress(server, SERVER_ADDR, atoi(argv[1]));
	printf("UDP Server started!\n");

	//Communicate with client
	sockaddr_in clientAddr;
	char buff[BUFF_SIZE];
	int ret;

	while (1) {
		//Receive message
		receiveMessage(server, buff, clientAddr, ret);

		//Resolve hostname
		resolveHost(server, buff, clientAddr, ret);

		//Send flag message after the last message 
		ret = sendto(server, "", ret, 0, (SOCKADDR *)&clientAddr, sizeof(clientAddr));
		}

	//Close socket
	closesocket(server);

	//Terminate Winsock
	WSACleanup();
	return 0;
}

