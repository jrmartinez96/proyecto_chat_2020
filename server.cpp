// Server side C/C++ program to demonstrate Socket programming 
#include <unistd.h>
#include<iostream>
#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <string.h> 
#include "mensaje.pb.h"
#include <thread>
#include <arpa/inet.h>
#include <bits/stdc++.h>
#define PORT 8080
#define MAX_CONNECTED_CLIENTS 10
using namespace std;
using namespace chat;

class Client
{
	public:
		int id;
		int socket;
		char* name;
		char* ip;
};

class ActiveConnection
{
	public:
		int sockId = -1;
		char* ip;
		int indexList = -1;
		pthread_t myReadThread;
};

// FUNCTIONS
bool isSockClosed(int sock);

void waitRunning();

void listenConnections(int serverSock);

int freeConnectedIndex();

void *readThread(void *args);

void responseMessage(int clientSock, int userId, int option);

// GLOBAL VARIABLES
list<Client> clientsList;
ActiveConnection connectedClients[MAX_CONNECTED_CLIENTS]; // El valor de cada posicion es ActiveConnection que contiene la ip y el socket id, y el index de la lista de usuarios que se han conectado
bool keepRunning;

int main(int argc, char const *argv[]) 
{
	keepRunning = true;
	// INITIALIZE SERVER
	GOOGLE_PROTOBUF_VERIFY_VERSION;

	int server_fd; 
	struct sockaddr_in address; 
	int opt = 1;
	
	// Creating socket file descriptor 
	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) 
	{ 
		perror("socket failed"); 
		exit(EXIT_FAILURE); 
	}
    cout<<"---------> SOCKET CREATED"<<endl;
	
	// Forcefully attaching socket to the port 8080 
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, 
												&opt, sizeof(opt))) 
	{ 
		perror("setsockopt"); 
		exit(EXIT_FAILURE); 
	} 
	address.sin_family = AF_INET; 
	address.sin_addr.s_addr = INADDR_ANY; 
	address.sin_port = htons( PORT ); 
	
	// Forcefully attaching socket to the port 8080 
	if (bind(server_fd, (struct sockaddr *)&address, 
								sizeof(address))<0) 
	{ 
		perror("bind failed"); 
		exit(EXIT_FAILURE); 
	}
    cout<<"---------> SOCKET BINDED TO PORT " << PORT <<endl;
	if (listen(server_fd, MAX_CONNECTED_CLIENTS) < 0) 
	{ 
		perror("listen"); 
		exit(EXIT_FAILURE); 
	}

    // cout<<"---------> LISTENING..."<<endl;
	thread listenThread(listenConnections, server_fd);
	thread waitRunningThread(waitRunning);
	
	while(keepRunning)
	{
	}

	// send(new_socket , hello , strlen(hello) , 0 ); 
	//printf("Hello message sent\n");
	listenThread.detach();
	waitRunningThread.detach();
    close(server_fd);

	google::protobuf::ShutdownProtobufLibrary();
	return 0; 
}

// CHECK IF SOCJET IS CLOSED
bool isSockClosed(int sock)
{
	char x;
interrupted:
	ssize_t r = recv(sock, &x, 1, MSG_DONTWAIT|MSG_PEEK);
	if(r<0)
	{
		switch (errno)
		{
			case EINTR:		goto interrupted;
			case EAGAIN:	break;
			case ETIMEDOUT:	break;
			case ENOTCONN:	break;
			default:		throw(errno);
		}
	}

	return r == 0;
}

// LISTEN TO CONNECTIONS AND ACCEPT THEM
void listenConnections(int serverSock)
{
	int new_socket, indexEmptyConnected;
	struct sockaddr_in address;
	int addrlen = sizeof(address);
	
	while(!isSockClosed(serverSock)) 
	{
		cout<<"---------> LISTENING..."<<endl;
		if ((new_socket = accept(serverSock, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0) 
		{ 
			perror("accept"); 
			exit(EXIT_FAILURE); 
		}
		
		cout<<"---------> CONNECTION ACCEPTED..."<<endl;

		struct sockaddr_in* pV4Addr = (struct sockaddr_in*)&address;
		struct in_addr ipAddr = pV4Addr->sin_addr;
		char ipStr[INET_ADDRSTRLEN];
		inet_ntop( AF_INET, &ipAddr, ipStr, INET_ADDRSTRLEN );

		cout << "CONNETION WITH: " << ipStr << endl;

		if ((indexEmptyConnected = freeConnectedIndex())>-1)
		{
			connectedClients[indexEmptyConnected].sockId = new_socket;
			connectedClients[indexEmptyConnected].ip = ipStr;
			pthread_create(&connectedClients[indexEmptyConnected].myReadThread, NULL, readThread, (void *)&indexEmptyConnected);
		} else {
			cout << "ALL CONNECTIONS ARE FULL, ";
			cout << "CLOSING CONNECTION WITH " << ipStr <<endl;
			close(new_socket);
		}
	}
}

// READ THREAD
void *readThread(void *indexCon)
{
	int indexConnection = *((int *)indexCon);
	cout << "INDEX CONNECTION " << indexConnection << endl;
	ActiveConnection clientConnection = connectedClients[indexConnection];
	int valread;
	char buffer[8192];
	while(!isSockClosed(clientConnection.sockId))
	{
		if((valread = read( clientConnection.sockId , buffer, 8192)) < 0)
		{
			perror("read"); 
			exit(EXIT_FAILURE);
		}
		string ret(buffer, 8192);

        ClientMessage m2;
		m2.ParseFromString(ret);

		if (m2.option() == 1){
		    // TODO: Pass attribute to function DIETER
            cout << "New User: " << m2.synchronize().username() << endl;
            thread respondingThread(
                    responseMessage, clientConnection.sockId, indexConnection, m2.option());
            respondingThread.detach();
		}
        else if (m2.option() == 6){
            // TODO: Pass attribute to function DIETER
            cout << "UserID: " << m2.acknowledge().userid() << endl;
            thread respondingThread(
                    responseMessage, clientConnection.sockId, indexConnection, m2.option());
            respondingThread.detach();
        }

		// TODO: Program all other options DIETER
        cout << "Message MOCK: " << m2.synchronize().username() << endl;
	}

	cout << "CLOSING CONNECTION WITH: " << clientConnection.ip << " indecon: " << indexConnection << endl;
	connectedClients[indexConnection].sockId = -1;
	connectedClients[indexConnection].indexList = -1;
	pthread_detach(connectedClients[indexConnection].myReadThread);

	return indexCon;
}
void responseMessage(int clientSock, int userId, int option)
{
    if (option == 1)
    {
        cout << "NEW HANDSHAKE REQUESTED" << endl;

        // Configuring SYN/ACK Protobuf Message
        ServerMessage m;
        m.set_option(4);
        MyInfoResponse* myInfoResponse(new MyInfoResponse);
        myInfoResponse->set_userid(userId);
        m.set_allocated_myinforesponse(myInfoResponse);

        // Message Serialization
        string binary;
        m.SerializeToString(&binary);

        // Conversion to a buffer of char
        char wBuffer[binary.size() + 1];
        strcpy(wBuffer, binary.c_str());

        // Send SYN/ACK to client
        send(clientSock , wBuffer , strlen(wBuffer) , 0 );
        cout << "SYN/ACK sent to Client" << endl;
    }
    else if (option == 6)
    {
        // TODO: Program the UserList with Username and Status Here DIETER
        cout << "Acknowledge received from Client" << endl;
        cout << "HANDSHAKE COMPLETED" << endl;
    }
}

// LOOK FOR AN EMPTY SPACE IN THE CONNECTED CLIENTS ARRAY, IT IS EMPTY IF THE ID IS -1
int freeConnectedIndex()
{
	int indexVacio = -1;
	for(int j = 0; j < MAX_CONNECTED_CLIENTS; j++) {
		if(connectedClients[j].sockId == -1) {
			indexVacio = j;
		}
	}

	return indexVacio;
}

void waitRunning()
{
	string quitInput;

	while(keepRunning)
	{
		getline(cin, quitInput);

		if(quitInput == "q")
		{
			keepRunning = false;
		}
	}
}