#include <iostream>
#include <WinSock2.h>
#include <filesystem>
#include <Ws2tcpip.h>
#include <fstream>
#include <sstream>
#include <unordered_set>
#pragma comment(lib, "ws2_32.lib")
#define CHUNK_SIZE 1024
using namespace std;
using namespace filesystem;
const path database = ".\\database";

class Client {
	char buffer_[CHUNK_SIZE];
	const SOCKET client_socket_;

	const int GetReadBytes() {
		memset(buffer_, 0, CHUNK_SIZE);
		return recv(client_socket_, buffer_, sizeof(buffer_), 0);
	}

	void Get(const path& file_path) {
		if (GetReadBytes() < 0)return;
		if (string(buffer_) == "Request denied.") {
			cout << "\033[94m" << buffer_ << endl;
			return;
		}
		path file_name = database / file_path.filename();
		ofstream file(file_name, ios::binary);
		int i = 0;
		int size = atoi(buffer_);
		while (i != size) {
			int bytes_received = GetReadBytes();
			file.write(buffer_, bytes_received);
			i += bytes_received;
		}
		file.close();
		OutputServerResponse();
	}

	void Put(const path& file_name) {
		ifstream file(file_name, ios::binary);	
		char buffer_for_data[CHUNK_SIZE];
		while (file.read(buffer_for_data, sizeof(buffer_for_data))) {
			send(client_socket_, buffer_for_data, (int)(file.gcount()), 0);
		} 
		if (file.gcount() > 0)send(client_socket_, buffer_for_data, (int)(file.gcount()), 0);
		file.close();
		OutputServerResponse();
	}

	const path GetFileFromInput(string& input, int index) const{
		string file;
		int index_end =  input.length();
		if (input[index] == '"' && input[index_end - 1] == '"') file = input.substr(index + 1, index_end - index - 2);
		else file = input.substr(index, index_end - index);
		replace(file.begin(), file.end(), '\\', '/');
		return file;
	}

	const string GetCommand(string& input) const{
		string command;
		stringstream ss(input);
		ss >> command;
		return command;
	}

public:

	Client(const SOCKET& socket) : client_socket_(socket) {
		memset(buffer_, 0, CHUNK_SIZE);
	}

	void OutputServerResponse() {
		if (GetReadBytes() > 0)cout << "\033[94m" << buffer_ << "\033[95m"<<endl;
	}

	void SendMessageToServer(const char* message)const {
		send(client_socket_, message, (int)strlen(message), 0);
	}

	void ProcessRequest(string& user_input) {
		string command = GetCommand(user_input);
		if (command == "PUT") {
			path file = GetFileFromInput(user_input, command.length() + 1);
			if (!is_regular_file(file) || !exists(file) || file_size(file) == 0) {
				cout << "\033[95mSomething wrong with the file!" << endl;
				return;
			}
			SendMessageToServer((user_input + " " + to_string(file_size(file))).c_str());
			Put(file);
		}	
		else if (command == "GET") {
			SendMessageToServer(user_input.c_str());
			Get(GetFileFromInput(user_input, command.length() + 1));
		}
		else {
			SendMessageToServer(user_input.c_str());
			OutputServerResponse();
		} 
	}
};

struct Validation {
	static const unordered_set<string> commands;

	static string ToUpper(string input) {
		 transform(input.begin(), input.end(), input.begin(), ::toupper);
		 return input;
	}

	static bool IsIncorrectRequest(string& input) {
		if (input.empty())return false;
		string command;
		stringstream ss(input);
		ss >> command;
		return commands.find(ToUpper(command)) == commands.end() || input.length()<=command.length()+1;
	}
};
const unordered_set<string> Validation::commands = { "GET", "LIST", "PUT", "INFO", "DELETE", "REMOVE"};

int main()
{
	WSADATA wsa_data;
	if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
		cerr << "WSAStartup failed" << endl;
		return 1;
	}

	const int port = 12345;
	const PCWSTR server_ip = L"127.0.0.1";
	SOCKET client_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (client_socket == INVALID_SOCKET) {
		cerr << "Error creating socket: " << WSAGetLastError() << endl;
		WSACleanup();
		return 1;
	}

	sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	InetPton(AF_INET, server_ip, &server_addr.sin_addr);
	if (connect(client_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) == SOCKET_ERROR) {
		cerr << "Connect failed with error: " << WSAGetLastError() << endl;
		closesocket(client_socket);
		WSACleanup();
		return 1;
	}

	Client client(client_socket);
	client.SendMessageToServer("Hello, server! How are you?");
	client.OutputServerResponse();

	string user_input;
	getline(cin, user_input);
	while (true) {
		if (Validation::ToUpper(user_input) == "EXIT") {
			client.SendMessageToServer("\033[91mClient decided to terminate the connection.\033[0m");
			break;
		}
		if (Validation::IsIncorrectRequest(user_input))cout << "\033[91mUndefined request.\033[95m" << endl; 
		else if (user_input.length()> CHUNK_SIZE)cout << "\033[95The message length exceeds 1024 bytes, which is the maximum." << endl;
		else client.ProcessRequest(user_input);
		getline(cin, user_input);
	}

	closesocket(client_socket);
	WSACleanup();
}

