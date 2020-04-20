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
#include <curses.h>
#include <list>
#include <map>
#include <sstream>
#include "mensaje.pb.h"
// #define PORT 8080
using namespace std;
using namespace chat;

// CLASES
class UserConnected
{
	public:
		string username;
		string status;
		int userId;
		string ip;
};

// VARIABLES
bool exitProgram;
bool synAck;
int userId;
string username;
int userIdDM;
int userIdInfo;
thread readThread;
thread inputThread;
string ERROR_STRING = "ERROR - INGRESA UNA DE LAS OPCIONES.";
string ERROR_BACK_STRING = "ADVERTENCIA - Escribe 'back' para regresar.";

int pantalla = 1; // Dice en que pantalla se encuentra el usuario
/*
        0: Pantalla de inicio
        1: Main Menu
        2: Broadcast
        3: Mensaje directo - lista de usuarios para seleccionar
        4: Mensaje directo - chat con persona seleccionado
        5: Cambio de estatus
        6: Lista de usuarios conectados
        7: Informacion de usuarios - Lista de usuarios
        8: Informacion de usuarios - Informacion de usuario seleccionado
        9: Ayuda
    */
list<string> broadcastMessages;
list<string> notificacionesArray;
map<int, list<string>> directMessages;
list<UserConnected> usersConnected;

//	IS SOCKET CLOSED
bool isSockClosed(int sock);

void handshake(int sock);

// THREAD FUNCTIONS
void writeText(int sock, WINDOW *mainWin, WINDOW *inputWin, WINDOW *notificationWin);
void readText(int sock, WINDOW *mainWin, WINDOW *inputWin, WINDOW *notificationWin);

// DIBUJAR
// Input
void getInput(WINDOW *win);

// Notifications
void printNotifications(WINDOW *win);

// Main
void renderMainWindow(WINDOW *win);
void printMainMenu(WINDOW *win);
void printInicio(WINDOW *win);
void printBroadcast(WINDOW *win);
void printMensajeDirectoLista(WINDOW *win);
void printMensajeDirectoChat(WINDOW *win);
void printChangeStatus(WINDOW *win);
void printUsuariosConectados(WINDOW *win);
void printInfoUsuariosLista(WINDOW *win);
void printInfoUsuario(WINDOW *win);
void printAyuda(WINDOW *win);

// Funciones generales
char *getMessage(list<char *> _list, int _i);
void sendToServer(int sock, ClientMessage m);
string getUsernameFromUserid(int userId);

int main(int argc, char const *argv[])
{
	string nombre = argv[1];
	username = nombre;
	const char * ipServ = argv[2];

	stringstream strValue;
	strValue << argv[3];

	unsigned int puerto;
	strValue >> puerto;

	#define PORT puerto

	GOOGLE_PROTOBUF_VERIFY_VERSION;
	username = "Mochi";
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
	if (inet_pton(AF_INET, ipServ, &serv_addr.sin_addr) <= 0)
	{
		printf("\nInvalid address/ Address not supported \n");
		return -1;
	}

	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != 0)
	{
		printf("\nConnection Failed \n");
		exit(2);
		return -1;
	}
	cout << "------> CONNECTED TO SOCKET" << endl << endl;

	handshake(sock);

	// INTERFAZ GRAFICA
	// Crear ventanas
	initscr();
	keypad(stdscr, TRUE);
	WINDOW *winMain = newwin(20, 120, 0, 0);
	WINDOW *inputWindow = newwin(3, 120, 20, 0);
	WINDOW *notificationsWindow = newwin(23, 60, 0, 122);

	renderMainWindow(winMain);
	printNotifications(notificationsWindow);

	readThread = thread(readText, sock, winMain, inputWindow, notificationsWindow);
	inputThread = thread(writeText, sock, winMain, inputWindow, notificationsWindow);

	while (!exitProgram)
	{
	}

	readThread.detach();
	inputThread.detach();

	//cout << "\r\e[A" << flush;
	cout << endl << "------> CLOSING SOCKET" << endl;
	close(sock);

	google::protobuf::ShutdownProtobufLibrary();
	return 0;
}

// CHECK IF SOCJET IS CLOSED
bool isSockClosed(int sock)
{
	char x;
interrupted:
	ssize_t r = recv(sock, &x, 1, MSG_DONTWAIT | MSG_PEEK);
	if (r < 0)
	{
		switch (errno)
		{
		case EINTR:
			goto interrupted;
		case EAGAIN:
			break;
		case ETIMEDOUT:
			break;
		case ENOTCONN:
			break;
		default:
			throw(errno);
		}
	}

	return r == 0;
}

// THREAD FUNCTIONS
void writeText(int sock, WINDOW *mainWin, WINDOW *inputWin, WINDOW *notificationWin)
{
	bool exitLoop = false;
	box(inputWin, 0, 0);
	waddstr(inputWin, "Input");
	wrefresh(inputWin);
	while (!exitLoop && !isSockClosed(sock))
	{

		int ch;
		nodelay(stdscr, TRUE);
		bool isIn = TRUE;
		char str[50];

		ch = mvwgetnstr(inputWin, 1, 3, str, 49);
		wmove(inputWin, 1, 3);
		waddstr(inputWin, "                                                  ");
		wrefresh(inputWin);

		// PANTALLA MENU
		if(pantalla == 1)
		{
			int opcion = 0;
			bool noHayError = true;
			try
			{
				opcion = atoi(str);
			}
			catch (const std::exception& e)
			{
				noHayError = false;
				notificacionesArray.push_back(ERROR_STRING);
				printNotifications(notificationWin);
			}

			if(noHayError)
			{
				if( 0 < opcion && opcion < 8)
				{
					switch (opcion)
					{
					case 1:
						pantalla = 2;
						renderMainWindow(mainWin);
						break;
					case 2:
						{
							pantalla = 3;
							renderMainWindow(mainWin);
							connectedUserRequest *cur(new connectedUserRequest);
							cur->set_userid(0);

							ClientMessage m;
							m.set_option(2);
							m.set_allocated_connectedusers(cur);

							sendToServer(sock, m);
							break;
						}
					case 3:
						{
							pantalla = 5;
							renderMainWindow(mainWin);
							break;
						}
					case 4:
						{
							pantalla = 6;
							renderMainWindow(mainWin);
							connectedUserRequest *cur(new connectedUserRequest);
							cur->set_userid(0);

							ClientMessage m;
							m.set_option(2);
							m.set_allocated_connectedusers(cur);

							sendToServer(sock, m);
							break;
						}
					case 5:
						{
							pantalla = 7;
							renderMainWindow(mainWin);
							connectedUserRequest *cur(new connectedUserRequest);
							cur->set_userid(0);

							ClientMessage m;
							m.set_option(2);
							m.set_allocated_connectedusers(cur);

							sendToServer(sock, m);
							break;
						}
					case 6:
						pantalla = 9;
						renderMainWindow(mainWin);
						break;
					case 7:
						exitLoop = true;
						break;
					
					default:
						notificacionesArray.push_back(ERROR_STRING);
						printNotifications(notificationWin);
						break;
					}
				} else {
					notificacionesArray.push_back(ERROR_STRING);
					printNotifications(notificationWin);
				}
			}

		}

		// PANTALLA BOADCASTING
		else if (pantalla == 2)
		{
			if(strcmp(str, "back") == 0)
			{
				pantalla = 1;
				renderMainWindow(mainWin);
			}
			else
			{	
				BroadcastRequest *br(new BroadcastRequest);
				br->set_message(str);

				ClientMessage m;
				m.set_option(4);
				m.set_allocated_broadcast(br);

				sendToServer(sock, m);

				string message(str);
				broadcastMessages.push_back("Yo: " + message);
				renderMainWindow(mainWin);
			}
			
		}

		// PANTALLA MENSAJE DIRECTO - LISTA USUARIOS
		else if(pantalla == 3)
		{
			if(strcmp(str, "back") == 0)
			{
				pantalla = 1;
				renderMainWindow(mainWin);
			}
			else
			{
				int opcion = 0;
				bool noHayError = true;
				try
				{
					opcion = atoi(str);
				}
				catch (const std::exception& e)
				{
					noHayError = false;
					notificacionesArray.push_back("ERROR - Elija un id de usuario aceptable.");
					printNotifications(notificationWin);
				}

				if(noHayError)
				{
					bool existeUsuario = false;
					for(int i = 0; i < usersConnected.size(); i++)
					{
						list<UserConnected>::iterator it = usersConnected.begin();
						advance(it, i);
						UserConnected user = *it;
						if(user.userId == opcion)
						{
							existeUsuario = true;
						}
					}

					if(existeUsuario)
					{
						userIdDM = opcion;
						pantalla = 4;
						renderMainWindow(mainWin);
					}
					else
					{
						notificacionesArray.push_back("ERROR - Usuario no existe en lista.");
						printNotifications(notificationWin);
					}
					
				}
			}
		}

		// PANTALLA MENSAJE DIRECTO - CHAT CON PERSONA SELECCIONADA
		else if(pantalla == 4)
		{
			if(strcmp(str, "back") == 0)
			{
				pantalla = 3;
				renderMainWindow(mainWin);
			}
			else
			{
				string username = "";
				try
				{
					username = getUsernameFromUserid(userIdDM);
				}
				catch(const std::exception& e)
				{
				}
				
				DirectMessageRequest *dmr(new DirectMessageRequest);
				dmr->set_message(str);
				dmr->set_userid(userIdDM);

				char cstr[username.size() + 1];
				strcpy(cstr, username.c_str());

				if(strcmp(cstr, "") != 0)
				{
					dmr->set_username(username);
				}

				ClientMessage m;
				m.set_option(5);
				m.set_allocated_directmessage(dmr);

				sendToServer(sock, m);

				string message(str);
				directMessages[userIdDM].push_back("Yo: " + message);
				renderMainWindow(mainWin);
			}
		}

		// PANTALLA CAMBIO DE ESTADO
		else if(pantalla == 5)
		{
			if(strcmp(str, "back") == 0)
			{
				pantalla = 1;
				renderMainWindow(mainWin);
			}
			else
			{
				int opcion = 0;
				bool noHayError = true;
				try
				{
					opcion = atoi(str);
				}
				catch (const std::exception& e)
				{
					noHayError = false;
					notificacionesArray.push_back(ERROR_STRING);
					printNotifications(notificationWin);
				}

				if(noHayError)
				{
					if( 0 < opcion && opcion < 4)
					{
						ClientMessage m;
						m.set_option(3);

						ChangeStatusRequest *csr(new ChangeStatusRequest);
						switch (opcion)
						{
						case 1:
							csr->set_status("Conectado");
							break;
						case 2:
							csr->set_status("Away");
							break;
						case 3:
							csr->set_status("Ocupado");
							break;
						
						default:
							break;
						}

						m.set_allocated_changestatus(csr);

						sendToServer(sock, m);

						pantalla = 1;
						renderMainWindow(mainWin);
					} else {
						notificacionesArray.push_back(ERROR_STRING);
						printNotifications(notificationWin);
					}
				}
			}
		}

		// PANTALLA DE LISTA DE USUARIOS CONECTADOS
		else if(pantalla == 6)
		{
			if(strcmp(str, "back") == 0)
			{
				pantalla = 1;
				renderMainWindow(mainWin);
			}
			else
			{
				notificacionesArray.push_back(ERROR_BACK_STRING);
				printNotifications(notificationWin);
			}
		}

		// PANTALLA INFORMACION DE USUARIOS - LISTA DE USUARIOS
		else if(pantalla == 7)
		{
			if(strcmp(str, "back") == 0)
			{
				pantalla = 1;
				renderMainWindow(mainWin);
			}
			else
			{
				int opcion = 0;
				bool noHayError = true;
				try
				{
					opcion = atoi(str);
				}
				catch (const std::exception& e)
				{
					noHayError = false;
					notificacionesArray.push_back("ERROR - Elija un id de usuario aceptable.");
					printNotifications(notificationWin);
				}

				if(noHayError)
				{
					bool existeUsuario = false;
					for(int i = 0; i < usersConnected.size(); i++)
					{
						list<UserConnected>::iterator it = usersConnected.begin();
						advance(it, i);
						UserConnected user = *it;
						if(user.userId == opcion)
						{
							existeUsuario = true;
						}
					}

					if(existeUsuario)
					{
						userIdInfo = opcion;
						pantalla = 8;
						renderMainWindow(mainWin);
					}
					else
					{
						notificacionesArray.push_back("ERROR - Usuario no existe en lista.");
						printNotifications(notificationWin);
					}
					
				}
			}
		}

		// PANTALLA INFORMACION DE USUARIOS - INFORMACION DE USUARIOS CONECTADOS
		else if(pantalla == 8)
		{
			if(strcmp(str, "back") == 0)
			{
				pantalla = 7;
				renderMainWindow(mainWin);
				connectedUserRequest *cur(new connectedUserRequest);
				cur->set_userid(0);

				ClientMessage m;
				m.set_option(2);
				m.set_allocated_connectedusers(cur);

				sendToServer(sock, m);
			}
			else
			{
				notificacionesArray.push_back(ERROR_BACK_STRING);
				printNotifications(notificationWin);
			}
		}

		// PANTALLA DE AYUDA
		else if(pantalla == 9)
		{
			if(strcmp(str, "back") == 0)
			{
				pantalla = 1;
				renderMainWindow(mainWin);
			}
			else
			{
				notificacionesArray.push_back(ERROR_BACK_STRING);
				printNotifications(notificationWin);
			}
		}
		
	}
	exitProgram = true;
}

void readText(int sock, WINDOW *mainWin, WINDOW *inputWin, WINDOW *notificationWin)
{
	int valread;
	char buffer[8192];
	while (!isSockClosed(sock))
	{
		valread = read(sock, buffer, 8192);
		//printf("%s\r",buffer );

		string ret(buffer, 8192);

		ServerMessage m;
		m.ParseFromString(ret);

		int opcion = m.option();

		switch (opcion)
		{
			case 1: // BROADCAST RESPONSE
				{
					string user = "";
					try
					{
						user = getUsernameFromUserid(m.broadcast().userid());
					}
					catch(const exception& e)
					{
						if(m.broadcast().has_username())
						{
							user = m.broadcast().username();
						}
						else
						{
							user = to_string(m.broadcast().userid());
						}
						
					}
					
					
					broadcastMessages.push_back(user + ": " +m.broadcast().message());

					if(pantalla == 2)
					{
						renderMainWindow(mainWin);
					}
					else
					{
						if(m.broadcast().has_username())
						{
							notificacionesArray.push_back("BROADCAST - Nuevo mensaje.");
						}
						else
						{
							string str1 = "BROADCAST - Nuevo mensaje de ";
							string username = m.broadcast().username();
							notificacionesArray.push_back(str1 + username);
						}
						printNotifications(notificationWin);
					}
					
					break;
				}
			
			case 2: // DIRECT MESSAGE RESPONSE
				{
					int userId = m.message().userid();
					string message = m.message().message();
					string username = "";
					if(m.message().has_username())
					{
						username = m.message().username();
					}

					directMessages[userId].push_back(username + ": " + message);

					if(pantalla == 4 && userId == userIdDM)
					{
						renderMainWindow(mainWin);
					}
					else
					{
						if(m.message().has_username())
						{
							notificacionesArray.push_back("DM - Nuevo mensaje de " + username);
						}
						else
						{
							notificacionesArray.push_back("DM - Nuevo mensaje de usuario " + to_string(userId));
						}

						printNotifications(notificationWin);
						
					}

					break;
					
				}

			case 3: // ERROR RESPONSE
				{
					string mensaje = m.error().errormessage();
					notificacionesArray.push_back("SERVER ERROR - " + mensaje);
					printNotifications(notificationWin);
					break;
				}
			
			case 4: // MY INFO RESPONSE
				{
					int userId = m.myinforesponse().userid();
					notificacionesArray.push_back("SERVER RESPONSE - MyInfo userId: " + to_string(userId));
					printNotifications(notificationWin);
					break;
				}

			case 5: // CONNECTED USER RESPONSE
				{
					usersConnected.clear();
					for(int i = 0; i < m.connecteduserresponse().connectedusers_size(); i++)
					{
						string username = m.connecteduserresponse().connectedusers(i).username();
						string ip = "";
						string status = "";
						int userId = -1;

						if(m.connecteduserresponse().connectedusers(i).has_ip())
						{
							ip = m.connecteduserresponse().connectedusers(i).ip();
						}

						if(m.connecteduserresponse().connectedusers(i).has_status())
						{
							status = m.connecteduserresponse().connectedusers(i).status();
						}

						if(m.connecteduserresponse().connectedusers(i).has_userid())
						{
							userId = m.connecteduserresponse().connectedusers(i).userid();
						}

						UserConnected user;
						user.username = username;
						user.ip = ip;
						user.status = status;
						user.userId = userId;

						usersConnected.push_back(user);						
					}

					if(pantalla == 3 || pantalla == 6 || pantalla == 7)
					{
						renderMainWindow(mainWin);
					}
					else
					{
						notificacionesArray.push_back("USERS CONNECTED - Lista actualizada.");
						printNotifications(notificationWin);
					}

					break;
				}

			case 6: // CHANGE STATUS RESPOSE
				{
					int userId = m.changestatusresponse().userid();
					string status = m.changestatusresponse().status();

					notificacionesArray.push_back("Status cambia a " + status + " para usuario con id" + to_string(userId));
					printNotifications(notificationWin);
					break;
				}

			case 7: // BROADCAST RESPONSE
				{
					string status = m.broadcastresponse().messagestatus();

					notificacionesArray.push_back("BROADCAST RESPONSE - " + status);
					printNotifications(notificationWin);

					break;
				}

			case 8: // DIRECT MESSAGE RESPONSE
				{
					string status = m.directmessageresponse().messagestatus();

					notificacionesArray.push_back("DM RESPONSE - " + status);
					printNotifications(notificationWin);

					break;
				}
			
			default:
				notificacionesArray.push_back("MENSAJE SIN OPCION");
				printNotifications(notificationWin);
				break;
		}
	}
	exitProgram = true;
}

// 3 Way handshake function
void handshake(int sock)
{
	cout << "HANDSHAKE INITIATED " << endl;

	// Configuring Synchronize Protobuf Message
	ClientMessage m;
	m.set_option(1);
	MyInfoSynchronize *synchronize(new MyInfoSynchronize);
	// TODO: Replace hardcoded username to a preset arg at runtime
	synchronize->set_username(username);
	m.set_allocated_synchronize(synchronize);

	// Message Serialization
	string binary;
	m.SerializeToString(&binary);

	// Conversion to a buffer of char
	char wBuffer[binary.size() + 1];
	strcpy(wBuffer, binary.c_str());

	// Send Synchronize Request to server
	send(sock, wBuffer, strlen(wBuffer), 0);

	cout << "Synchronize Request sent to Server" << endl;

	// Read Socket sequence, waits for SYN/ACK
	int valueToRead;
	char buffer[8192];
	while (!synAck)
	{
		// Reading Socket
		valueToRead = read(sock, buffer, 8192);
		string ret(buffer, 8192);

		// Parse Protobuf Message received
		ServerMessage m2;
		m2.ParseFromString(ret);

		cout << endl
			 << "SYN/ACK Received from Server" << endl;

		// Receive and Save the UserId returned by the server
		userId = m2.myinforesponse().userid();
		cout << "UserId Assigned: " << userId << endl;

		if (m2.option() == 4 && userId != 0)
		{
			// Configuring Acknowledge Protobuf Message
			ClientMessage m;
			m.set_option(6);
			MyInfoAcknowledge *acknowledge(new MyInfoAcknowledge);
			acknowledge->set_userid(userId);
			m.set_allocated_acknowledge(acknowledge);

			// Message Serialization
			string binary;
			m.SerializeToString(&binary);

			// Conversion to a buffer of char
			char wBuffer[binary.size() + 1];
			strcpy(wBuffer, binary.c_str());

			// Send Synchronize Request to server
			send(sock, wBuffer, strlen(wBuffer), 0);

			cout << "Acknowledge Response sent to Server" << endl;
			cout << "HANDSHAKE COMPLETED" << endl;

			// Finish 3 Way Handshake
			synAck = true;
		}
	}
}

// --------------------------------------- MAIN WINDOW --------------------------------------- //
void renderMainWindow(WINDOW *win)
{
    switch (pantalla)
    {
    case 0:
        printInicio(win);
        break;

    case 1:
        printMainMenu(win);
        break;

    case 2:
        printBroadcast(win);
        break;

	case 3:
		printMensajeDirectoLista(win);
		break;

	case 4:
		printMensajeDirectoChat(win);
		break;

    case 5:
        printChangeStatus(win);
        break;
	
	case 6:
        printUsuariosConectados(win);
        break;

	case 7:
        printInfoUsuariosLista(win);
        break;

	case 8:
        printInfoUsuario(win);
        break;

    case 9:
        printAyuda(win);
        break;
    
    default:
        printMainMenu(win);
        break;
    }
}

void printMainMenu(WINDOW *win)
{
    wclear(win);
    box(win, 0, 0);
    waddstr(win, "Menu Principal");
    wmove(win, 1, 2);
    waddstr(win, "Seleccione una opcion: ");

    wmove(win, 3, 2);
    waddstr(win, "1) Broadcasting");

    wmove(win, 4, 2);
    waddstr(win, "2) Mensaje directo (DM)");

    wmove(win, 5, 2);
    waddstr(win, "3) Cambiar status");

    wmove(win, 6, 2);
    waddstr(win, "4) Listar usuarios conectados");

    wmove(win, 7, 2);
    waddstr(win, "5) Informacion de usuario");

    wmove(win, 8, 2);
    waddstr(win, "6) Ayuda");

    wmove(win, 9, 2);
    waddstr(win, "7) Salir");
    wrefresh(win);
}

void printInicio(WINDOW *win)
{
    wclear(win);
    box(win, 0, 0);
    waddstr(win, "Bienvenido!");
    wmove(win, 1, 2);
    waddstr(win, "Escribe tu nombre:");
    wrefresh(win);
}

void printBroadcast(WINDOW *win)
{
    wclear(win);
    box(win, 0, 0);
    waddstr(win, "Broadcast");

    int listBroadSize = broadcastMessages.size();
    int listIndex = 0;
    if(listBroadSize > 18)
    {
        listIndex = listBroadSize - 18;
    }

    int cursorPositionY = 1;

    for(int i = listIndex; i < listBroadSize; i++)
    {
        wmove(win, cursorPositionY, 2);

		// GET NOTIFICATION FROM LIST
		list<string>::iterator it = broadcastMessages.begin();
    	advance(it, i);
		string mensaje = *it;

        char cstr[mensaje.size() + 1];
		strcpy(cstr, mensaje.c_str());
        waddstr(win, cstr);
        cursorPositionY++;
    }

    wrefresh(win);
}

void printMensajeDirectoLista(WINDOW *win)
{
	wclear(win);
    box(win, 0, 0);
    waddstr(win, "Mensaje directo - Lista");

	int listUsersConnSize = usersConnected.size();
	int listIndex = 0;
	if(listUsersConnSize > 18)
	{
		listIndex = listUsersConnSize - 18;
	}

	int cursorPositionY = 1;

    for(int i = listIndex; i < listUsersConnSize; i++)
    {
        wmove(win, cursorPositionY, 2);

		// GET NOTIFICATION FROM LIST
		list<UserConnected>::iterator it = usersConnected.begin();
    	advance(it, i);
		UserConnected user = *it;

		string strShow = to_string(user.userId) + ") " + user.username + " - " + user.ip + " - " + user.status;

        char cstr[strShow.size() + 1];
		strcpy(cstr, strShow.c_str());
        waddstr(win, cstr);
        cursorPositionY++;
    }

    wrefresh(win);
}

void printMensajeDirectoChat(WINDOW *win)
{
	string username = "";
	try
	{
		username = getUsernameFromUserid(userIdDM);
	}
	catch(const std::exception& e)
	{
	}
	
	string titulo = "Mensaje directo - " + username;

	char cstr[titulo.size() + 1];
	strcpy(cstr, titulo.c_str());

	wclear(win);
    box(win, 0, 0);
    waddstr(win, cstr);

	list<string> dms = directMessages[userIdDM];

	int listDmsSize = dms.size();
    int listIndex = 0;
    if(listDmsSize > 18)
    {
        listIndex = listDmsSize - 18;
    }

    int cursorPositionY = 1;

    for(int i = listIndex; i < listDmsSize; i++)
    {
        wmove(win, cursorPositionY, 2);
		// GET NOTIFICATION FROM LIST
		list<string>::iterator it = dms.begin();
    	advance(it, i);
		string mensaje = *it;


		char cstr[mensaje.size() + 1];
		strcpy(cstr, mensaje.c_str());
        waddstr(win, cstr);
        cursorPositionY++;
    }

    wrefresh(win);
}

void printChangeStatus(WINDOW *win)
{
    wclear(win);
    box(win, 0, 0);
    waddstr(win, "Cambiar status");

    wmove(win, 1, 2);
    waddstr(win, "Seleccione una de las opciones:");

    wmove(win, 3, 2);
    waddstr(win, "1) Conectado");

    wmove(win, 4, 2);
    waddstr(win, "2) Lejos");

    wmove(win, 5, 2);
    waddstr(win, "3) Ocupado");

    wrefresh(win);
}

void printUsuariosConectados(WINDOW *win)
{
	wclear(win);
    box(win, 0, 0);
    waddstr(win, "Usarios conectados");

	int listUsersConnSize = usersConnected.size();
	int listIndex = 0;
	if(listUsersConnSize > 18)
	{
		listIndex = listUsersConnSize - 18;
	}

	int cursorPositionY = 1;

    for(int i = listIndex; i < listUsersConnSize; i++)
    {
        wmove(win, cursorPositionY, 2);

		// GET NOTIFICATION FROM LIST
		list<UserConnected>::iterator it = usersConnected.begin();
    	advance(it, i);
		UserConnected user = *it;

		string strShow = to_string(user.userId) + ") " + user.username + " - " + user.ip + " - " + user.status;

        char cstr[strShow.size() + 1];
		strcpy(cstr, strShow.c_str());
        waddstr(win, cstr);
        cursorPositionY++;
    }

    wrefresh(win);
}

void printInfoUsuariosLista(WINDOW *win)
{
	wclear(win);
    box(win, 0, 0);
    waddstr(win, "Usarios conectados");

	int listUsersConnSize = usersConnected.size();
	int listIndex = 0;
	if(listUsersConnSize > 18)
	{
		listIndex = listUsersConnSize - 18;
	}

	int cursorPositionY = 1;

    for(int i = listIndex; i < listUsersConnSize; i++)
    {
        wmove(win, cursorPositionY, 2);

		// GET NOTIFICATION FROM LIST
		list<UserConnected>::iterator it = usersConnected.begin();
    	advance(it, i);
		UserConnected user = *it;

		string strShow = to_string(user.userId) + ") " + user.username;

        char cstr[strShow.size() + 1];
		strcpy(cstr, strShow.c_str());
        waddstr(win, cstr);
        cursorPositionY++;
    }

    wrefresh(win);
}

void printInfoUsuario(WINDOW *win)
{
	UserConnected user;
	for(int i = 0; i < usersConnected.size(); i++)
	{
		list<UserConnected>::iterator it = usersConnected.begin();
    	advance(it, i);
		UserConnected possibleUser = *it;

		if(possibleUser.userId == userIdInfo)
		{
			user = possibleUser;
		}
	}

	wclear(win);
    box(win, 0, 0);
	waddstr(win, "Informacion de usuario");

	string username = "Username: " + user.username;
	char usernamec[username.size() + 1];
	strcpy(usernamec, username.c_str());
	wmove(win, 2, 2);
	waddstr(win, usernamec);

	string ip = "IP: " + user.ip;
	char ipc[ip.size() + 1];
	strcpy(ipc, ip.c_str());
	wmove(win, 4, 2);
	waddstr(win, ipc);

	string status = "Status: " + user.status;
	char statusc[status.size() + 1];
	strcpy(statusc, status.c_str());
	wmove(win, 6, 2);
	waddstr(win, statusc);

	string uid = "Status: " + to_string(user.userId);
	char uidc[uid.size() + 1];
	strcpy(uidc, uid.c_str());
	wmove(win, 8, 2);
	waddstr(win, uidc);

	wrefresh(win);
}

void printAyuda(WINDOW *win)
{
    wclear(win);
    box(win, 0, 0);
    waddstr(win, "Ayuda");

    wmove(win, 2, 2);
    waddstr(win, "Bienvenido al programa del chat, aqui te daremos unos puntos importantes para poder utilizar este programa:");

    wmove(win, 4, 2);
    waddstr(win, "- Si ves numeros como '1)', '2)'... significa que tienes que seleccionar alguna opcion escribiendo el numero.");

    wmove(win, 5, 2);
    waddstr(win, "- En caso de que no aparezcan numeros entre las opciones puedes escribir lo que tu quieras.");

    wmove(win, 6, 2);
    waddstr(win, "- Si quieres regresar a una pantalla solo escribe 'back' en la caja de Input.");

    wrefresh(win);
}

// --------------------------------------- INPUT WINDOW --------------------------------------- //
void getInput(WINDOW *win)
{
    box(win, 0, 0);
    waddstr(win, "Input");
    wrefresh(win);

    int ch;
    nodelay(stdscr, TRUE);
    bool isIn = TRUE;
    char str[50];

    while(TRUE) {
        ch = mvwgetnstr(win, 1, 3, str, 49);
        wmove(win, 1, 3);
        waddstr(win, "                                                  ");
        wrefresh(win);
    }
}

// --------------------------------------- NOTIFICATIONS WINDOW --------------------------------------- //
void printNotifications(WINDOW *win)
{
    wclear(win);
    box(win, 0, 0);
    waddstr(win, "Notificaciones");

    int listNotificacionesSize = notificacionesArray.size();
    int listIndex = 0;
    if(listNotificacionesSize > 21)
    {
        listIndex = listNotificacionesSize - 21;
    }

    int cursorPositionY = 1;

    for(int i = listIndex; i < listNotificacionesSize; i++)
    {
        wmove(win, cursorPositionY, 2);
		// GET NOTIFICATION FROM LIST
		list<string>::iterator it = notificacionesArray.begin();
    	advance(it, i);
		string mensaje = *it;


		char cstr[mensaje.size() + 1];
		strcpy(cstr, mensaje.c_str());
        waddstr(win, cstr);
        cursorPositionY++;
    }

    wrefresh(win);
}

// --------------------------------------- FUNCIONES GENERALES --------------------------------------- //
char* getMessage(list<char*> objectlist, int _i){
    list<char*>::iterator it = objectlist.begin();
    advance(it, _i);
    return *it;
}

void sendToServer(int sock, ClientMessage m)
{
	string binary;
	m.SerializeToString(&binary);
	char cstr[binary.size() + 1];
	strcpy(cstr, binary.c_str());
	send(sock , cstr , strlen(cstr) , 0 );
}

string getUsernameFromUserid(int userId)
{
	string username = "";

	list<UserConnected>::iterator it = usersConnected.begin();
	for(int i = 0; i < usersConnected.size(); i++)
	{
		UserConnected user = *it;
		if(user.userId == userId)
		{
			username = user.username;
		}
		advance(it, 1);
	}

	return username;
}