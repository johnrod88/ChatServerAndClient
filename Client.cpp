//Author: John Rodriguez

#include <iostream>
#include <string>
#include <WS2tcpip.h>
#include <thread>
#pragma comment(lib, "ws2_32.lib")

using namespace std;

void sendMsg(SOCKET &s, int &stillConnected);

int main()
{
	string ipAddr;//For IP storing Address of the server.
	int port;//For storing listening port of server.

	//Ask for server's ip.
	cout << "Please enter the ip address of the server: ";
	cin >> ipAddr;

	//Ask for server's port.
	cout << "Please enter the server's port #: ";
	cin >> port;
	cin.ignore();
	
	//Start WinSock
	WSAData data;
	WORD version = MAKEWORD(2, 2);//version = "0000 0010 0000 0010".
	int wsResult = WSAStartup(version, &data);//WSAStartup returns 0 if successful.
	if (wsResult != 0)
	{
		cout << "Can't start Winsock. Goodbye.\n";
		return 0;
	}

	// Create socket
	SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);//AF_INET for ipv4, SOCK_STREAM for TCP.
	if (sock == INVALID_SOCKET)//INVALID_SOCKET is equivalent to int value -1.
	{
		cerr << "Can't create socket. Goodbye.\n";
		WSACleanup();//Terminates use of the Winsock 2 DLL (Ws2_32.dll).
		return 0;
	}

	sockaddr_in hint;//Decided to use 'hint' name because it seems to be standard practice.
	hint.sin_family = AF_INET;//ipv4
	hint.sin_port = htons(port);//htons() converts passed value from host byte order to network byte order. (i.e. little-endian -> big-endian).
	inet_pton(AF_INET, ipAddr.c_str(), &hint.sin_addr);//converts an ip address from text format into binary form, then asigns it to hint.sin_addr.

	//Connect to server.
	int connResult = connect(sock, (sockaddr*)&hint, sizeof(hint));
	if (connResult == SOCKET_ERROR)
	{
		cerr << "Can't connect to server. Please ensure that the server is running\n"
			 << "and that you entered the correct IP address and port#. Goodbye.\n";
		closesocket(sock);
		WSACleanup();
		return 0;
	}

	char buffer[4096];//For storing received messages.
	int bytesReceived = 1;

	//Start new thread for sending messages while also receiving.
	thread sending{ sendMsg, ref(sock), ref(bytesReceived) };

	do
	{
		//Check for messages from server
		ZeroMemory(buffer, 4096);//clear buffer for message from server.
		bytesReceived = recv(sock, buffer, 4096, 0);
		if (bytesReceived > 0)//if a message is received.
		{
			//Print received message to screen.
			cout << string(buffer, 0, bytesReceived);
		}

	} while (bytesReceived > 0);//If bytes received = 0, server has shutdown.

	sending.join();
	
	closesocket(sock);//Closes the socket.
	WSACleanup();//Terminates Winsock.
	return 0;
}

//This function runs in its own thread so client can send 
//and receive at same time 
void sendMsg(SOCKET &s, int &stillConnected)
{
	string clientMsg;
	while (stillConnected != 0)
	{
		getline(cin, clientMsg);
		if (clientMsg.size() > 0)
		{
			// Send the text
			int sendResult = send(s, clientMsg.c_str(), clientMsg.size() + 1, 0);
			if (sendResult == SOCKET_ERROR)
			{
				cout << "There was an error sending your message.\n";
				return;
			}
		}
	}
}