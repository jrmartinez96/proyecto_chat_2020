// Client side C/C++ program to demonstrate Socket programming 
#include <stdio.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <unistd.h> 
#include <string> 
#include <string.h> 
#include <iostream>
#include <thread>
#include "mensaje.pb.h"
#define PORT 8080 
using namespace std;
using namespace chat;

// VARIABLES
bool exitProgram;
thread readThread;
thread inputThread;

//	IS SOCKET CLOSED
bool isSockClosed(int sock);


// THREAD FUNCTIONS
void writeText(int sock);

void readText(int sock);

int main(int argc, char const *argv[]) 
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;

	exitProgram = false;
	int sock = 0, valread; 
	struct sockaddr_in serv_addr;
	
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
	{ 
		printf("\n Socket creation error \n"); 
		return -1; 
	} 

	serv_addr.sin_family = AF_INET; 
	serv_addr.sin_port = htons(PORT); 
	
	// Convert IPv4 and IPv6 addresses from text to binary form 
	if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0) 
	{ 
		printf("\nInvalid address/ Address not supported \n"); 
		return -1; 
	} 

	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) 
	{ 
		printf("\nConnection Failed \n"); 
		return -1; 
	}
	cout<<"------> CONNECTED TO SOCKET"<<endl<<endl;

	readThread = thread(readText, sock);
	inputThread = thread(writeText, sock);

	while(!exitProgram){
	}

	readThread.detach();
	inputThread.detach();

	//cout << "\r\e[A" << flush;
	cout<<endl<<"------> CLOSING SOCKET"<<endl;
    close(sock);

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


// THREAD FUNCTIONS
void writeText(int sock)
{
	Mensaje m;
	bool exitLoop = false;
	while(!exitLoop && !isSockClosed(sock))
	{
		cout<<"Ingresa tu mensaje: ";
		string str;
		getline(cin, str);
		if(str == "quit") {
			exitLoop = true;
		} else {
			// Configurar Protobuf
			m.set_option("0");
			MiInfoRequest* miinfo(new MiInfoRequest);
			miinfo->set_username(str);
			miinfo->set_ip("123456789");
			m.set_allocated_miinforeq(miinfo);

			// Serializacion
			string binary;
    		m.SerializeToString(&binary);

			// Conversion a buffer de char
			char cstr[binary.size() + 1];
			strcpy(cstr, binary.c_str());

			// Envio de mensaje
			send(sock , cstr , strlen(cstr) , 0 );
		}
	}
	exitProgram = true;
}

void readText(int sock)
{
	int valread;
	char buffer[1024] = {0}; 
	while(!isSockClosed(sock))
	{
		valread = read( sock , buffer, 1024); 
		printf("%s\r",buffer );
		cout<<endl<<"Ingresa tu mensaje: ";
	}
	exitProgram = true;
}