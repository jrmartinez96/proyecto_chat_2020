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
#include <sstream>
// #define PORT 8080
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
		string ip;
		int userIds = -1;
		string userNames = "";
		string userStatus = "ACTIVO";
		bool acknowledged = false;
		pthread_t myReadThread;
};

// FUNCTIONS
bool isSockClosed(int sock);

void waitRunning();

void listenConnections(int serverSock);

int freeConnectedIndex();

void *readThread(void *args);

void responseMessage(int indexConn, ClientMessage message, ActiveConnection attributes);

void sendingMessage(int indexConn, ClientMessage message, ActiveConnection attributes);

void sendUserListToAll();

// GLOBAL VARIABLES
list<Client> clientsList;
ActiveConnection connectedClients[MAX_CONNECTED_CLIENTS]; // El valor de cada posicion es ActiveConnection que contiene la ip y el socket id, y el index de la lista de usuarios que se han conectado
bool keepRunning;
bool isSync;

int main(int argc, char const *argv[]) 
{
    stringstream strValue;
	strValue << argv[1];

	unsigned int puerto;
	strValue >> puerto;

	#define PORT puerto

	keepRunning = true;
	isSync = false;
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
            connectedClients[indexEmptyConnected].userIds = indexEmptyConnected;
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

		int opt = m2.option();

		if ((1 <= opt && opt <= 3) || opt == 6){
            thread respondingThread(
                    responseMessage, indexConnection, m2, clientConnection);
            respondingThread.detach();
		}
        else if (opt == 4 || opt == 5){
            thread messageThread(
                    sendingMessage, indexConnection, m2, clientConnection);
            messageThread.detach();
        }
	}

	cout << "CLOSING CONNECTION WITH: " << clientConnection.ip << " indecon: " << indexConnection << endl;
	connectedClients[indexConnection].sockId = -1;
	connectedClients[indexConnection].ip = "";
	connectedClients[indexConnection].userIds = -1;
	connectedClients[indexConnection].userNames = "";
	connectedClients[indexConnection].userStatus = "ACTIVO";
	connectedClients[indexConnection].acknowledged = false;
	pthread_detach(connectedClients[indexConnection].myReadThread);

	return indexCon;
}
void responseMessage(int indexConn, ClientMessage message, ActiveConnection attributes)
{
    if (message.option() == 1)
    {
        cout << "NEW HANDSHAKE REQUESTED" << endl;

        // Check if username already exists
        bool userExists = false;
        for(int i = 0; i < 10; i++)
        {
            if(connectedClients[i].sockId != -1)
            {
                if(connectedClients[i].userNames.compare(message.synchronize().username()) == 0)
                {
                    userExists = true;
                }
            }
        }

        if(userExists)
        {
            cout << "USUARIO EXISTE" << endl;
            ServerMessage sme;
            sme.set_option(3);

            ErrorResponse *err(new ErrorResponse);
            err->set_errormessage("USUARIO YA EXISTE");

            sme.set_allocated_error(err);

            // Message Serialization
            string binary;
            sme.SerializeToString(&binary);

            // Conversion to a buffer of char
            char wBuffer[binary.size() + 1];
            strcpy(wBuffer, binary.c_str());

            // Send SYN/ACK to client
            send(attributes.sockId , wBuffer , strlen(wBuffer) , 0 );
        }
        else
        {    
            int userId = 10 - indexConn;

            // Associate the Username and UserId to the Connection Data
            connectedClients[indexConn].userNames = message.synchronize().username();
            connectedClients[indexConn].userIds = userId;

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
            send(attributes.sockId , wBuffer , strlen(wBuffer) , 0 );
            cout << "SYN/ACK sent to Client" << endl;

            isSync = true;
        }
        

    }
    else if (message.option() == 6 && isSync)
    {
        connectedClients[indexConn].acknowledged = true;
        cout << "Acknowledge received from Client" << endl;
        cout << "HANDSHAKE COMPLETED" << endl;

        sendUserListToAll();
    }
    else if (message.option() == 2 && connectedClients[indexConn].acknowledged)
    {
        cout << "A USER REQUIRED THE USER-LIST" << endl;

        // Configuring SYN/ACK Protobuf Message
        ServerMessage m;
        m.set_option(5);

        ConnectedUserResponse* connectedUserResponse(new ConnectedUserResponse);

        //ConnectedUserResponse connectedUserResponse;

        if (message.connectedusers().userid() == 0 || !message.connectedusers().has_username())
        {
            cout << "Complete list requested" << endl;

            // Filling out the Repeated Message
            for(int j = 0; j < MAX_CONNECTED_CLIENTS; j++) {
                if(connectedClients[j].sockId != -1) {
                    ConnectedUser* ConnectedUser = connectedUserResponse->add_connectedusers();
                    ConnectedUser->set_username(connectedClients[j].userNames);
                    ConnectedUser->set_status(connectedClients[j].userStatus);
                    ConnectedUser->set_userid(connectedClients[j].userIds);
                    ConnectedUser->set_ip(connectedClients[j].ip);
                }
            }

            cout << "Complete UserList sent to Client" << endl;
        }
        else {
            cout << "List Requested for User: " << message.connectedusers().userid() << endl;

            ServerMessage m;
            m.set_option(5);

            if (0 < message.connectedusers().userid() && message.connectedusers().userid() < 10)
            {
                int index = 10 - message.connectedusers().userid();

                ConnectedUserResponse* connectedUserResponse(new ConnectedUserResponse);
                ConnectedUser* ConnectedUser = connectedUserResponse->add_connectedusers();
                ConnectedUser->set_username(connectedClients[index].userNames);
                ConnectedUser->set_status(connectedClients[index].userStatus);
                ConnectedUser->set_userid(connectedClients[index].userIds);
                ConnectedUser->set_ip(connectedClients[index].ip);
            }
            else
            {
                for(int j = 0; j < MAX_CONNECTED_CLIENTS; j++) {
                    if(connectedClients[j].userNames == message.connectedusers().username()) {
                        ConnectedUserResponse* connectedUserResponse(new ConnectedUserResponse);
                        ConnectedUser* ConnectedUser = connectedUserResponse->add_connectedusers();
                        ConnectedUser->set_username(connectedClients[j].userNames);
                        ConnectedUser->set_status(connectedClients[j].userStatus);
                        ConnectedUser->set_userid(connectedClients[j].userIds);
                        ConnectedUser->set_ip(connectedClients[j].ip);
                    }
                }
            }

            cout << "Partial UserList sent to Client" << endl;
        }

        m.set_allocated_connecteduserresponse(connectedUserResponse);

        // Message Serialization
        string binary;
        m.SerializeToString(&binary);

        // Conversion to a buffer of char
        char wBuffer[binary.size() + 1];
        strcpy(wBuffer, binary.c_str());

        // Send Complete Users List to client
        send(attributes.sockId , wBuffer , strlen(wBuffer) , 0 );
    }
    else if (message.option() == 3 && connectedClients[indexConn].acknowledged)
    {
        cout << "STATUS CHANGE REQUESTED FOR USER: " << connectedClients[indexConn].userIds << endl;

        connectedClients[indexConn].userStatus = message.changestatus().status();

        cout << "Status changed to " << message.changestatus().status() << endl;
    }
}

void sendingMessage(int indexConn, ClientMessage message2, ActiveConnection attributes)
{
    if (message2.option() == 4 && connectedClients[indexConn].acknowledged){
        cout << "RECEIVED A BROADCAST REQUEST" << endl;

        cout << "Responding with Broadcast message status" << endl;

        // Configuring Broadcast Response Protobuf Message
        ServerMessage r;
        r.set_option(7);
        BroadcastResponse* broadcastResponse(new BroadcastResponse);
        broadcastResponse->set_messagestatus("Received! Broadcasting...");
        r.set_allocated_broadcastresponse(broadcastResponse);

        // Message Serialization
        string binary2;
        r.SerializeToString(&binary2);

        // Conversion to a buffer of char
        char wBuffer2[binary2.size() + 1];
        strcpy(wBuffer2, binary2.c_str());

        // Send Broadcast Response to client
        send(attributes.sockId , wBuffer2 , strlen(wBuffer2) , 0 );

        cout << "Response sent, Broadcasting..." << endl;
        cout << "Message: " << message2.broadcast().message() << endl;
        // Configuring Broadcast Protobuf Message
        ServerMessage b;
        b.set_option(1);
        BroadcastMessage* broadcast(new BroadcastMessage);
        broadcast->set_message(message2.broadcast().message());
        broadcast->set_userid(connectedClients[indexConn].userIds);
        broadcast->set_username(connectedClients[indexConn].userNames);
        b.set_allocated_broadcast(broadcast);

        // Message Serialization
        string binary;
        b.SerializeToString(&binary);

        // Conversion to a buffer of char
        char wBuffer[binary.size() + 1];
        strcpy(wBuffer, binary.c_str());

        for(int j = 0; j < MAX_CONNECTED_CLIENTS; j++) {
            if(connectedClients[j].sockId != -1) {
                // Send Broadcast to all connected clients
                send(connectedClients[j].sockId , wBuffer , strlen(wBuffer) , 0 );
            }
        }

        cout << "Broadcast Set to all Users" << endl;
    }
    else if (message2.option() == 5 && connectedClients[indexConn].acknowledged)
    {
        cout << "RECEIVED A DIRECT MESSAGE REQUEST" << endl;

        cout << "Responding with Direct Message status" << endl;

        // Configuring DM Response Protobuf Message
        ServerMessage r;
        r.set_option(8);
        DirectMessageResponse* directMessageResponse(new DirectMessageResponse);
        directMessageResponse->set_messagestatus("Received! Sending Message...");
        r.set_allocated_directmessageresponse(directMessageResponse);

        // Message Serialization
        string binary2;
        r.SerializeToString(&binary2);

        // Conversion to a buffer of char
        char wBuffer2[binary2.size() + 1];
        strcpy(wBuffer2, binary2.c_str());

        // Send Broadcast Response to client
        send(attributes.sockId , wBuffer2 , strlen(wBuffer2) , 0 );

        cout << "Response sent, Sending Message..." << endl;

        // Configuring DM Protobuf Message
        ServerMessage b;
        b.set_option(2);
        DirectMessage* message(new DirectMessage);
        message->set_message(message2.directmessage().message());
        message->set_userid(connectedClients[indexConn].userIds);
        b.set_allocated_message(message);

        // Message Serialization
        string binary;
        b.SerializeToString(&binary);

        // Conversion to a buffer of char
        char wBuffer[binary.size() + 1];
        strcpy(wBuffer, binary.c_str());

        // Send DM to connected client
        if (1 <= message2.directmessage().userid() && message2.directmessage().userid() <= 10)
        {
            int userTo = 10 - message2.directmessage().userid();
            int socketTo = connectedClients[userTo].sockId;
            send(socketTo , wBuffer , strlen(wBuffer) , 0 );
        }
        else
        {
            for(int j = 0; j < MAX_CONNECTED_CLIENTS; j++) {
                if(connectedClients[j].userNames == message2.directmessage().username()) {
                    send(connectedClients[j].sockId , wBuffer , strlen(wBuffer) , 0 );
                }
            }
        }

        cout << "Direct Message sent to: " << message2.directmessage().userid() << endl;
    }
}

void sendUserListToAll()
{
    ServerMessage m;
    m.set_option(5);

    ConnectedUserResponse* connectedUserResponse(new ConnectedUserResponse);

    //ConnectedUserResponse connectedUserResponse;

    for(int j = 0; j < MAX_CONNECTED_CLIENTS; j++) {
        if(connectedClients[j].sockId != -1) {
            ConnectedUser* ConnectedUser = connectedUserResponse->add_connectedusers();
            ConnectedUser->set_username(connectedClients[j].userNames);
            ConnectedUser->set_status(connectedClients[j].userStatus);
            ConnectedUser->set_userid(connectedClients[j].userIds);
            ConnectedUser->set_ip(connectedClients[j].ip);
        }
    }

    m.set_allocated_connecteduserresponse(connectedUserResponse);

    // Message Serialization
    string binary;
    m.SerializeToString(&binary);

    // Conversion to a buffer of char
    char wBuffer[binary.size() + 1];
    strcpy(wBuffer, binary.c_str());

    for(int j = 0; j < MAX_CONNECTED_CLIENTS; j++) {
        if(connectedClients[j].sockId != -1) {
            // Send Broadcast to all connected clients
            send(connectedClients[j].sockId , wBuffer , strlen(wBuffer) , 0 );
        }
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