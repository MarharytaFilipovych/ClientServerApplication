#include <iostream>
#include <WinSock2.h>
using namespace std;
#include <Ws2tcpip.h>
#include <fstream>
#include <sstream>

#pragma comment(lib, "ws2_32.lib")

class Commands {

	SOCKET clientSocket;

	
public:
	Commands(const SOCKET& socket) :clientSocket(socket) {}

	void get(const string& filename) {
		const char* message = ("GET "+filename).c_str();
		send(clientSocket, message, (int)strlen(message), 0);
		char buffer[1024];
		memset(buffer, 0, 1024);
		int bytesReceived = recv(clientSocket, buffer, (int)sizeof(buffer), 0);
		if (bytesReceived < 0)return;
		if (string(buffer) == "I am unable to open your file!") {
			cout << "Received from server: " << buffer << endl;
			return;
		}
		ofstream file(filename, ios::binary);
		int i = 0;
		while (i != atoi(buffer)) {
			memset(buffer, 0, 1024);
			bytesReceived = recv(clientSocket, buffer, (int)sizeof(buffer), 0);
			file.write(buffer, bytesReceived);
			i += bytesReceived;
		}
		file.close();
		cout << "File transfer completed!" << endl;

	}
};

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
	WSACleanup();

}

