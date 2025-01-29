#include <iostream>
#include <WinSock2.h>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#define CHUNK_SIZE 1024
using namespace std;
#include <filesystem>
using namespace std::filesystem;
#pragma comment(lib, "ws2_32.lib")

class Request {

    char buffer[CHUNK_SIZE];
    const SOCKET clientSocket;

public:
    Request(const SOCKET& socket) :clientSocket(socket) {
        memset(buffer, 0, CHUNK_SIZE);
    }

    const int getRequest() {
        memset(buffer, 0, CHUNK_SIZE);
        return recv(clientSocket, buffer, sizeof(buffer), 0);
    }
    const char* readBuffer() const {
        return buffer;
    }
};


class Response {
    const SOCKET clientSocket;

    
    bool fileIsNotOkay(const string& file)const {
        return !exists(file) || !is_regular_file(file);
    }

    const string getFilePermissions(const string& file)const {
        perms p = status(file).permissions();
        string result;
        result += ((p & perms::owner_read) != perms::none ? "r" : "-");
        result += ((p & perms::owner_write) != perms::none ? "w" : "-");
        result += ((p & perms::owner_exec) != perms::none ? "x" : "-");
        return result;
    }


    void get(const string& fileName) const{
        if (fileIsNotOkay(fileName)) {
            sendMessage("I am unable to open your file!");
            return;
        }
        ifstream file(fileName, ios::binary);
        sendMessage(to_string(file_size(fileName)).c_str());
        char buffer[CHUNK_SIZE];
        while (file.read(buffer, sizeof(buffer))) send(clientSocket, buffer, (int)(file.gcount()), 0);       
        if (file.gcount() > 0)send(clientSocket, buffer, (int)(file.gcount()), 0);
        file.close();
    }

    void list(const string& path)const {
        if (exists(path) && is_directory(path)) {
            string list;
            for (const auto& entry : directory_iterator(path)) {
                list.append(entry.path().string() + "\n");
            }
            sendMessage(list.c_str());
        }
        else sendMessage("Looks like this directory doesn't exist!");
    }

    void put(const string& fileName) const {
        ofstream file(fileName);


    }

    void info(const string& file)const {
        if (fileIsNotOkay(file)) {
            sendMessage("The file doesn't exist!");
            return;
        }
        string size = to_string(file_size(file));
        string time = format("Last modified: {}", last_write_time(file));
        string path = absolute(file).string();
        string info = "Size: " + size + " bytes\n" + time + "\nAbsolute path: " + path + "\nPermissions: "+ getFilePermissions(file);
        sendMessage(info.c_str());             
    }

    void deleteFile(const string&  file) const {
        if(remove(file))sendMessage("Successful removal!");
        else sendMessage("The file doesn't exist!");
    }

public:
    Response(const SOCKET& socket) :clientSocket(socket) {}

    void sendMessage(const char* response)const {
        send(clientSocket, response, (int)strlen(response), 0);
    }

    void sendResponseToRequest(const char* message) const {
        stringstream stream(message);
        string command, path;
        stream >> command >> path;
        if (command == "GET") get(path);
        else if (command == "LIST") list(path);
        else if (command == "PUT") put(path);
        else if (command == "INFO") info(path);
        else if (command == "DELETE_FILE") deleteFile(path);
        else sendMessage("Request denied.");
    }
};


int main()
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "WSAStartup failed!" << endl;
        return 1;
    }

    int port = 12345;
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        cerr << "Error craeting socket: " << WSAGetLastError() << endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        cerr << "Listen failed with error: " << WSAGetLastError() <<endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }
    cout << "Server listening on port " << port << endl;
    SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);
    if (clientSocket == INVALID_SOCKET) {
        cerr << "Accept failed with error: " << WSAGetLastError() << endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }
    Request request(clientSocket);
    Response response(clientSocket);
    
    if (request.getRequest() > 0) {
        cout << "Received data: " << request.readBuffer() << endl;
        response.sendMessage("Hello, client! This is the server.");
    }

    while (true) {
        if (request.getRequest() > 0) {
            const char* buffer = request.readBuffer();
            cout << "Received request: " << buffer << endl;
            response.sendResponseToRequest(buffer);
        }
        else break;
    }

    closesocket(clientSocket);
    closesocket(serverSocket);
    WSACleanup();
    return 0;
    
}
