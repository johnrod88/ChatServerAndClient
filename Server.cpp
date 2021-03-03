//Author: John Rodriguez

#include <WS2tcpip.h>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#pragma comment (lib, "ws2_32.lib")
#define PORT 54000 //Specifies what port I want to bind to.
#define bufferSize 4096 //Specifies the size of char buffer for client messages.
using namespace std;

//function prototypes
int clientNum(SOCKET, fd_set);
void serverMsg(fd_set&, SOCKET, bool&);

int main()
{
	timeval timeOut;//Used so select() doesn't hang up program.
	timeOut.tv_sec = 2;//Sets timeOut to 2 secs.

	// Initialze winsock
	//Basically a container for Winsock implementation info. WSADATA is a struct type.
	WSADATA wsData;

	//Creates the 16-bit word "00000010 00000010". (i.e. winsock version = 2.2)
	WORD version = MAKEWORD(2, 2);

	//initiates winsock.dll and returns 0 if successful.
	int startWinsock = WSAStartup(version, &wsData);
	if (startWinsock != 0)
	{
		cout << "Can't Initialize winsock! Quitting." << endl;
		return 0;
	}

	//Create the socket/file-descriptor/handle
	//AF_INET sets address family to IPv4. SOCK_STREAM sets socket type to TCP.
	//Third parameter sets protocol. Entry of 0 means I did not specify.
	SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == INVALID_SOCKET)
	{
		cout << "Can't create a socket. Quitting." << endl;
		return 0;
	}

	//Bind the ip address and port to a socket
	sockaddr_in setup;//SOCKADDR_IN structure specifies a transport address and port for the AF_INET address family.
	setup.sin_family = AF_INET;//configures for IPv4
	setup.sin_port = htons(PORT);//configures the port, htons converts PORT to big endian.
	setup.sin_addr.S_un.S_addr = INADDR_ANY; //Bind it to all ip adresses on local machine. 

	//Initiate the bind to 'sock', with config from 'setup', which has name length in bytes of sizeof(setup).
	int bindStatus = ::bind(sock, (sockaddr*)&setup, sizeof(setup));

	if (bindStatus == INVALID_SOCKET)//check for failed bind.
	{
		cout << "Can't bind socket!" << endl;
		return 0;
	}

	//Tell Winsock that socket 'sock' is for listening, and set backlog limit.
	listen(sock, SOMAXCONN);

	cout << "Server is running. Enter \"quit\" at any time to shutdown.\n\n";

	// Create the master socket/file-descriptor set and zero it.
	//The 'fd_set' struct is used by the select function to place sockets into a "set".
	//Max # of ports that an fd_set can hold is 1024.
	fd_set master;
	FD_ZERO(&master);

	//Add 'sock' to the set 'master'
	FD_SET(sock, &master);

	bool running = true;//Used to control loop for recieving messages.	
	bool servThread = TRUE;//Used to determine if  is still running.
	thread thread1{ serverMsg, ref(master), sock, ref(servThread) };//Start thread for send messages.

	while (running)
	{
		//Make a copy of 'master' to pass into select(), because it will remove any non-interacting ports from the set.
		fd_set masterCopy = master;

		//Check for any client messages and count them.
		//The select function handles the sockets in 'master' to perform synchronous I/O.
		int socketCount = select(0, &masterCopy, nullptr, nullptr, &timeOut);

		//Cycle through connections
		for (int i = 0; i < socketCount; i++)
		{
			//Create a holder socket for testing and assign it the i'th socket from 'mastercopy' .
			SOCKET sock2 = masterCopy.fd_array[i];

			//If it is our listening socket 'sock', then it must be a new client
			if (sock2 == sock)
			{
				SOCKET newClient = accept(sock, nullptr, nullptr);//accept client

				//Add new client to 'master' set of sockets. Original 'master' set never gets altered by select().  
				FD_SET(newClient, &master);

				//Inform the new client that they are connected.
				string connected = "Connected to Server. You are Client" 
					   + to_string(clientNum(newClient, master)) + "\r\n\n";
				send(newClient, connected.c_str(), connected.size()+1, 0);

				cout << "Client" << clientNum(newClient, master) << " is now connected.\n";
			}
			else //It must be a message from a client
			{
				char buf[bufferSize];
				ZeroMemory(buf, bufferSize);

				//Receive message
				int bytesIn = recv(sock2, buf, bufferSize, 0);
				if (bytesIn <= 0)//bytesIn = -1 if recv() returned SOCKET_ERROR, bytesIn = 0 if client disconnected.
				{
					//Drop client
					closesocket(sock2);
					FD_CLR(sock2, &master);
				}
				else
				{
					//Print to server's screen
					if ((int)buf[0] != 13)//ASCII #13 is carriage return.
					{
						cout << "Client" << clientNum(sock2, master) << ": " << string(buf) << endl;
					}

					//Send message to other clients
					for (int j = 0; j < master.fd_count; j++)
					{
						SOCKET outSock = master.fd_array[j];
						if (outSock != sock && outSock != sock2)//Don't send to server or client where message originated
						{
							ostringstream who;
							who << "Client" << clientNum(sock2, master) << ": " << buf << "\r\n";
							string strOut = who.str();

							send(outSock, strOut.c_str(), strOut.size() + 1, 0);
						}
					}
				}
			}
		}
		if (servThread == FALSE)
		{
			running = FALSE;
		}
	}

	//Remove 'sock' from 'master' and close it.
	FD_CLR(sock, &master);
	closesocket(sock);

	//Alert clients of server shutdown.
	string shtDown = "The server has shut down. Press Enter key to close window.\r\n";

	while (master.fd_count > 0)
	{
		SOCKET sock3 = master.fd_array[0];//get socket #	
		send(sock3, shtDown.c_str(), shtDown.size() + 1, 0);//Send server shutdown message.
		FD_CLR(sock3, &master);//delete from 'master'
		closesocket(sock3);//close the socket
	}
		
	WSACleanup();//Properly closes Winsock.
	thread1.join();//Wait on thread1 to properly close.
	system("pause");
	return 0;
}

//This function returns the Client# of a client. 
int clientNum(SOCKET x, fd_set y)
{
	for (int k = 0; k < y.fd_count; k++)
	{
		if (x == y.fd_array[k])
		{
			return k;
		}
	}
}

//This function runs in a seperate thread, allowing the server to send messages.
void serverMsg(fd_set &masterSet, SOCKET listener, bool &run)
{
	char servMsg[bufferSize];

	while (TRUE) 
	{		
		string holder;//used to hold message entered by server.
		getline(cin, holder);//getline is used so spaces in message are kept.
		strcpy_s(servMsg, holder.c_str());//copies holder to servMsg, which is used to send output stream.
		
		if (holder == "quit")
		{
			run = FALSE;//stops the loop in main() which recieves messages
			break;
		}

		for (int i = 0; i < masterSet.fd_count; i++)//Send message to all clients
		{
			SOCKET out = masterSet.fd_array[i];
			if (out != listener)
			{
				ostringstream serv;
				serv << "Server: " << servMsg << "\r\n";
				string outString = serv.str();

				send(out, outString.c_str(), outString.size() + 1, 0);
			}

		}
		
		ZeroMemory(servMsg, bufferSize);
	}
	return;
}
