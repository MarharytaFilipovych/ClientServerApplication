#include <iostream>
#include <WinSock2.h>
#define CHUNK_SIZE 1024
using namespace std;
#include <filesystem>
using namespace std::filesystem;
#include <Ws2tcpip.h>
#include <fstream>
#include <sstream>
#include <unordered_set>
#pragma comment(lib, "ws2_32.lib")
#include <algorithm>
const path database = ".\\database";

class Client {
	char buffer[CHUNK_SIZE];
	const SOCKET clientSocket;

	const int getResponse() {
		memset(buffer, 0, CHUNK_SIZE);
		return recv(clientSocket, buffer, sizeof(buffer), 0);
	}

	void get(const path& file_path) {
		if (getResponse() < 0)return;
		if (string(buffer) == "Request denied.") {
			cout << "Server: " << buffer << endl;
			return;
		}
		path filename = database / file_path.filename();
		ofstream file(filename, ios::binary);
		int i = 0;
		int fileSize = atoi(buffer);
		while (i != fileSize) {
			int bytesReceived = getResponse();
			file.write(buffer, bytesReceived);
			i += bytesReceived;
		}
		file.close();
		cout << "File received." << endl;		
	}

	void put(const path& filename) {
		ifstream file(filename, ios::binary);	
		char bufferForContent[CHUNK_SIZE];
		while (file.read(bufferForContent, sizeof(bufferForContent))) {
			send(clientSocket, bufferForContent, (int)(file.gcount()), 0);
		} 
		if (file.gcount() > 0)send(clientSocket, bufferForContent, (int)(file.gcount()), 0);
		file.close();
		outputServerResponse();
	}

	path getFileFromInput(string& command, string& input, int index) {
		string file;
		int indexEnd =  input.length();
		if (input[index] == '"' && input[indexEnd - 1] == '"') file = input.substr(index + 1, indexEnd - index - 2);
		else file = input.substr(index, indexEnd - index);
		replace(file.begin(), file.end(), '\\', '/');
		return file;
	}

	string getCommand(string& input) {
		string command;
		stringstream ss(input);
		ss >> command;
		return command;
	}

public:

	Client(const SOCKET& socket) : clientSocket(socket) {
		memset(buffer, 0, CHUNK_SIZE);
	}

	void outputServerResponse() {
		if (getResponse() > 0)cout << "Server: " << buffer << endl;
	}

	void sendMessage(const char* message)const {
		send(clientSocket, message, (int)strlen(message), 0);
	}
	

	void getResponse(string& userInput) {
		string command = getCommand(userInput);
		if (command == "PUT") {
			path file = getFileFromInput(command, userInput, command.length() + 1);
			if (!is_regular_file(file) || !exists(file) || file_size(file) == 0) {
				cout << "Something wrong with the file!" << endl;
				return;
			}
			sendMessage((userInput + " " + to_string(file_size(file))).c_str());
			put(file);
		}	
		else if (command == "LIST" || command == "INFO" || command == "DELETE") {
			sendMessage(userInput.c_str());
			outputServerResponse();
		}
		else {
			sendMessage(userInput.c_str());
			get(getFileFromInput(command, userInput, command.length() + 1));
		} 
	}
};

struct Validation {
	static const unordered_set<string> commands;

	static bool isIncorrectRequest(string& input) {
		if (input.empty())return false;
		string command;
		stringstream ss(input);
		ss >> command;
		return commands.find(command) == commands.end() || input.length()<=command.length();
	}
};
const unordered_set<string> Validation::commands = { "GET", "LIST", "PUT", "INFO", "DELETE" };

int main()
{
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		cerr << "WSAStartup failed" << endl;
		return 1;
	}
	int port = 12345;
	PCWSTR serverIp = L"127.0.0.1";
	SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (clientSocket == INVALID_SOCKET) {
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
	Client request(clientSocket);
	request.sendMessage("Hello, server! How are you?");
	request.outputServerResponse();
	string user_input;
	getline(cin, user_input);
	while (true) {
		if (Validation::isIncorrectRequest(user_input)) {
			cout << "Undefined request" << endl;
			getline(cin, user_input);
		}
		else {
			request.getResponse(user_input);
			getline(cin, user_input);
		} 
	}
	closesocket(clientSocket);
	WSACleanup();

}

