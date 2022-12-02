// Client.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "conio.h"
#include "stdlib.h"
#include "iostream"
#include "string"
#include "stdio.h"
#include "winsock2.h"
#include "ws2tcpip.h"
#define BUFF_SIZE 2048
#define ENDING_DELIMITER "\r\n"
#define LOGIN_HEADER "USER "
#define POST_HEADER "POST "
#define LOGOUT_HEADER "BYE"
#define SUCCESS_LOGIN 10
#define SUCCESS_POST 20
#define SUCCESS_LOGOUT 30
#define LOCKED_LOGIN 11
#define NOT_LOGIN 21
#define NOT_EXISTED 12
#define LOGGING_IN 13
#define INVALID_REQUEST 99
using namespace std;

#pragma comment (lib, "Ws2_32.lib")

/**
* @function constructAddress: Inittiate and construct server address.
*
* @param ipAdress: A string contains the IPv4 address of server.
* @param port: A number represents the number of port.
* @return: A sockaddr_in value has been constructed.
**/
sockaddr_in constructAddress(char* ipAddress, int port) {
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(port);
	inet_pton(AF_INET, ipAddress, &serverAddr.sin_addr);
	return serverAddr;
}

/**
* @function sendMessage: Create message and send.
*
* @param client: A SOCKET represents current client.
* @param header: A string contains header of message.
* @return: A string will contains content entered.
**/
char* sendMessage(SOCKET client, char *header) {
	char sBuff[BUFF_SIZE] = "";
	int ret;
	char message[BUFF_SIZE] = "";
	//Check if header is BYE
	if (!strcmp(header, LOGOUT_HEADER)) {
		strcat_s(sBuff, BUFF_SIZE, header);
		strcat_s(sBuff, BUFF_SIZE, ENDING_DELIMITER);
	}
	//Other header
	else {
		gets_s(message, BUFF_SIZE);
		strcat_s(sBuff, BUFF_SIZE, header);
		strcat_s(sBuff, BUFF_SIZE, message);
		strcat_s(sBuff, BUFF_SIZE, ENDING_DELIMITER);
	}
	//Send message
	ret = send(client, sBuff, strlen(sBuff), 0);
	if (ret == SOCKET_ERROR)
		printf("Error %d", WSAGetLastError());
	return message;
}

/**
* @function handleMessage: Determine which kind of message server replies
*
* @param rBuff: A integer represents the reply message.
* @param account: A string contains current account used.
* @param isLogin: Check if client is logging in.
* @param message: A string represents the message sent to server.
* @return: No value return.
**/
void handleMessage(int rBuff, string &account, bool &isLogin, char *message) {
	switch (rBuff) {
		case SUCCESS_LOGIN: {
			//Store account value and login status.
			printf("You have logged in!\n");
			account = message;
			isLogin = true;
			_getch();
			break;
		}
		case SUCCESS_POST: {
			printf("You have posted a article!\n");
			_getch();
			break;
		}
		case SUCCESS_LOGOUT: {
			//Delete account value and login status.
			account = "";
			isLogin = false;
			printf("You have been logged out!\n");
			_getch();
			break;
		}
		case LOCKED_LOGIN: {
			printf("This account have been banned or locked!\n");
			_getch();
			break;
		}
		case NOT_LOGIN: {
			printf("You are not logged in!\n");
			_getch();
			break;
		}
		case NOT_EXISTED: {
			printf("This account is not exited!\n");
			_getch();
			break;
		}
		case LOGGING_IN: {
			printf("This account have been used by other user!\n");
			_getch();
			break;
		}
		case INVALID_REQUEST: {
			printf("Cannot recognize your request!\n");
			_getch();
			break;
		}
		default: {
			break;
		}
	}
}

/**
* @function receiveMessage: Receive message replied from server.
*
* @param client: A SOCKET represents current client.
* @return: A string contains the reply message.
**/
char* receiveMessage(SOCKET client) {
	char buff[BUFF_SIZE] = "";
	int ret;
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
		cout << ret;
		buff[strlen(buff)] = 0;
	}
	return buff;
}

/**
* @function displayUI: Create and display client interface.
*
* @param client: A SOCKET represents current client.
* @return: No value return.
**/
void displayUI(SOCKET client) {
	string account = ""; //Store the account
	bool isLogin = false; //Store the status
	int inputNum = 0;
	while (1) {
		system("CLS"); //Clear the console screen
		if (!isLogin) {
			printf("You are not LOGIN!\n");
			printf("--------------------\n");
			printf("Choose a option:\n");
			printf("1. Login a account\n2. Post a article\n3. Logout\n4. Exit\n");
			printf("Enter your option: ");
			scanf_s("%d", &inputNum);
			cin.ignore();
			// Check if user input invalid number
			while (inputNum > 4 || inputNum <= 0) {
				printf("Invalid option! Please re-enter your option: ");
				scanf_s("%d", &inputNum);
				cin.ignore();
			}
			switch (inputNum) {
			case 1: {
				printf("Enter your account: ");
				char message[BUFF_SIZE] = "";
				//Enter and send message
				strcpy_s(message, sizeof(message), sendMessage(client, LOGIN_HEADER));
				//Handle reply message
				int rBuff;
				rBuff = atoi(receiveMessage(client));
				handleMessage(rBuff, account, isLogin, message);
				break;
			}
			case 2: {
				printf("Enter the content for the post: \n");
				char message[BUFF_SIZE] = "";
				//Enter and send message
				strcpy_s(message, sizeof(message), sendMessage(client, POST_HEADER));
				//Handle reply message
				int rBuff;
				rBuff = atoi(receiveMessage(client));
				handleMessage(rBuff, account, isLogin, message);
				break;
			}
			case 3: {
				char message[BUFF_SIZE] = "";
				//Send message
				strcpy_s(message, sizeof(message), sendMessage(client, LOGOUT_HEADER));
				//Handle reply message
				int rBuff;
				rBuff = atoi(receiveMessage(client));
				handleMessage(rBuff, account, isLogin, message);
				break;
			}
			case 4: {
				return; //Break if exit if chosen
			}
			default: {
				break;
			}
			}
		}
		else {
			printf("Welcome %s!\n", account.c_str());
			printf("--------------------\n");
			printf("Choose a option:\n");
			printf("1. Post a article\n2. Logout\n3. Exit\n");
			printf("Enter your option: ");
			scanf_s("%d", &inputNum);
			cin.ignore();
			// Check if user input invalid number
			while (inputNum > 3 || inputNum <= 0) {
				printf("Invalid option! Please re-enter your option: ");
				scanf_s("%d", &inputNum);
				cin.ignore();
			}
			switch (inputNum) {
			case 1: {
				printf("Enter the content for the post: \n");
				char message[BUFF_SIZE] = "";
				//Enter and send message
				strcpy_s(message, sizeof(message), sendMessage(client, POST_HEADER));
				//Handle reply message
				int rBuff;
				rBuff = atoi(receiveMessage(client));
				handleMessage(rBuff, account, isLogin, message);
				break;
			}
			case 2: {
				char message[BUFF_SIZE] = "";
				//Send message
				strcpy_s(message, sizeof(message), sendMessage(client, LOGOUT_HEADER));
				//Handle reply message
				int rBuff;
				rBuff = atoi(receiveMessage(client));
				handleMessage(rBuff, account, isLogin, message);
				break;
			}
			case 3: {
				return; //Break if exit if chosen
			}
			default: {
				break;
			}
			}
		}
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
		_getch();
		return 0;
	}

	//Display client interface
	displayUI(client);

	//Close socket
	closesocket(client);

	//Terminate Winsock
	WSACleanup();

	return 0;
}
