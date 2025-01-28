#include <iostream>
#include <WinSock2.h>
using namespace std;
#include <Ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

int main()
{
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0){
		cerr << "WSAStartup failed" << endl;
		return 1;
	}
	int port = 12345;
	PCWSTR serverIp =  L"127.0.0.1";
	SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (clientSocket == INVALID_SOCKET){
		cerr << "Error creating socket: " << WSAGetLastError() << endl;
		WSACleanup();
		return 1;
	}
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(port);
	InetPton(AF_INET, serverIp, &serverAddr.sin_addr);
	if (connect(clientSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
		cerr << "Connect failed with error: " << WSAGetLastError() << endl;
		closesocket(clientSocket);
		WSACleanup();
		return 1;
	}

	const char* message = "Hello, server! How are you?";
	send(clientSocket, message, (int)strlen(message), 0);

	char buffer[1024];
	memset(buffer, 0, 1024);
	int bytesReceived = recv(clientSocket, buffer, (int)sizeof(buffer), 0);
	if (bytesReceived > 0)
	{
		cout << "Received from server: " << buffer << endl;
	}
	closesocket(clientSocket);
	WSACleanup()

}

