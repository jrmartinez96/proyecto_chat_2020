// Client side C/C++ Chat Program
// Jose Rodrigo Martinez, Antonio Reyes y Dieter de Wit
#include <stdio.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <unistd.h> 
#include <string> 
#include <string.h> 
#include <iostream>
#include <thread>
#include <sstream>
#include "mensaje.pb.h"
//#define PORT 8080
using namespace std;
using namespace chat;

// VARIABLES
bool exitProgram;
bool synAck;
int userId;
thread readThread;
thread inputThread;


//	IS SOCKET CLOSED
bool isSockClosed(int sock);

void handshake(int sock, string nombre);

// THREAD FUNCTIONS
void writeText(int sock);

void readText(int sock);

int main(int argc, char const *argv[]) 
{

	string nombre = argv[1];
	const char * ipServ = argv[2];

	stringstream strValue;
	strValue << argv[3];

	unsigned int puerto;
	strValue >> puerto;

	#define PORT puerto

	GOOGLE_PROTOBUF_VERIFY_VERSION;

	exitProgram = false;
	synAck = false;
    userId = 0;

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
	if(inet_pton(AF_INET, ipServ, &serv_addr.sin_addr)<=0) 
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

	handshake(sock, nombre);

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
    ClientMessage m;
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
			m.set_option(5);
            MyInfoSynchronize* synchronize(new MyInfoSynchronize);
            synchronize->set_username(str);
			m.set_allocated_synchronize(synchronize);

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
	char buffer[8192];
	while(!isSockClosed(sock))
	{
		valread = read( sock , buffer, 8192);
		//printf("%s\r",buffer );

        string ret(buffer, 8192);

        ServerMessage m2;
        m2.ParseFromString(ret);

		cout<<endl<<"Mensaje Recibido!"<< endl;
        cout << "Option: " << m2.option() << endl;
	}
	exitProgram = true;
}

// 3 Way handshake function
void handshake(int sock, string nombre)
{
    cout << "HANDSHAKE INITIATED " << endl;

    // Configuring Synchronize Protobuf Message
    ClientMessage m;
    m.set_option(1);
    MyInfoSynchronize* synchronize(new MyInfoSynchronize);
    // TODO: Replace hardcoded username to a preset arg at runtime
    synchronize->set_username(nombre);
    m.set_allocated_synchronize(synchronize);

    // Message Serialization
    string binary;
    m.SerializeToString(&binary);

    // Conversion to a buffer of char
    char wBuffer[binary.size() + 1];
    strcpy(wBuffer, binary.c_str());

    // Send Synchronize Request to server
    send(sock , wBuffer , strlen(wBuffer) , 0 );

    cout << "Synchronize Request sent to Server" << endl;

    // Read Socket sequence, waits for SYN/ACK
    int valueToRead;
    char buffer[8192];
    while(!synAck)
    {
        // Reading Socket
        valueToRead = read( sock , buffer, 8192);
        string ret(buffer, 8192);

        // Parse Protobuf Message received
        ServerMessage m2;
        m2.ParseFromString(ret);

        cout<<endl<<"SYN/ACK Received from Server"<< endl;

        // Receive and Save the UserId returned by the server
        userId = m2.myinforesponse().userid();
        cout << "UserId Assigned: " << userId << endl;

        if (m2.option() == 4 && userId != 0)
        {
            // Configuring Acknowledge Protobuf Message
            ClientMessage m;
            m.set_option(6);
            MyInfoAcknowledge* acknowledge(new MyInfoAcknowledge);
            acknowledge->set_userid(userId);
            m.set_allocated_acknowledge(acknowledge);

            // Message Serialization
            string binary;
            m.SerializeToString(&binary);

            // Conversion to a buffer of char
            char wBuffer[binary.size() + 1];
            strcpy(wBuffer, binary.c_str());

            // Send Synchronize Request to server
            send(sock , wBuffer , strlen(wBuffer) , 0 );

            cout << "Acknowledge Response sent to Server" << endl;
            cout << "HANDSHAKE COMPLETED" << endl;

            // Finish 3 Way Handshake
            synAck = true;
        }
    }

}