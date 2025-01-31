#include <iostream>
#include <WinSock2.h>
#include <fstream>
#include <string>
#include <sstream>
#define CHUNK_SIZE 1024
using namespace std;
#include <filesystem>
using namespace std::filesystem;
#pragma comment(lib, "ws2_32.lib")
const string serverDatabase = ".\\Server database";


class Server {
    const SOCKET clientSocket;

    const string getFilePermissions(const path& file)const {
        perms p = status(file).permissions();
        string result;
        result += ((p & perms::owner_read) != perms::none ? "r" : "-");
        result += ((p & perms::owner_write) != perms::none ? "w" : "-");
        result += ((p & perms::owner_exec) != perms::none ? "x" : "-");
        return result;
    }

    void get(const path& fileName) const {
        if (!is_regular_file(fileName)) {
            sendMessage("I am unable to open your file!");
            return;
        }
        ifstream file(fileName, ios::binary);

        sendMessage(to_string(file_size(fileName)).c_str());
        char bufferForData[CHUNK_SIZE];
        while (file.read(bufferForData, sizeof(bufferForData))) {
            send(clientSocket, bufferForData, (int)(file.gcount()), 0);
        }
        if (file.gcount() > 0)send(clientSocket, bufferForData, (int)(file.gcount()), 0);
        file.close();
    }


    void list(const path& dir_path)const {
        if (is_directory(dir_path)) {
            string list;
            for (const auto& entry : directory_iterator(dir_path)) {
                list.append(entry.path().string() + "\n");
            }
            sendMessage(list.c_str());
        }
        else sendMessage("Looks like this directory doesn't exist!");
    }

    void put(const path& file_path, int size)  {
        path fileName = serverDatabase /file_path.filename();
        ofstream file(fileName, ios::binary);
        int i = 0;
        while (i != size) {
            int bytesReceived = getData();
            file.write(buffer, bytesReceived);
            i += bytesReceived;
        }
        file.close();
        sendMessage("File transfer completed!");
    }

    void info(const path& file)const {
        if (!is_regular_file(file)) {
            sendMessage("Request denied.");
            return;
        }
        string size = to_string(file_size(file));
        string time = format("Last modified: {}", last_write_time(file));
        string path = absolute(file).string();
        string info = "Size: " + size + " bytes\n" + time + "\nAbsolute path: " + path + "\nPermissions: "+ getFilePermissions(file);
        sendMessage(info.c_str());             
    }

    void deleteFile(path&  file) const {
        if(remove(file))sendMessage("Successful removal!");
        else sendMessage("The file doesn't exist!");
    }


    path getPath(const string& command) {
        string input(buffer);
        bool isPut = (command == "PUT");
        int startIndex = command.length() + 1;
        int indexEnd = isPut ? input.rfind(' ') : input.length();
        string file;
        if (input[startIndex] == '"' && input[indexEnd - 1] == '"')file = input.substr(startIndex + 1, indexEnd - startIndex - 2);                   
        else file = input.substr(startIndex, indexEnd - startIndex);
        return file;
    }

    int getFileSize() {
        string input(buffer);
        return stoi(input.substr(input.rfind(' ') + 1));
    }

    string getCommand() {
        string command;
        stringstream ss(buffer);
        ss >> command;
        return command;
    }

    bool isInvalidPath(const path& p, const string& command)const {
        return !exists(p) || (command == "LIST" && !is_directory(p)) || (command != "LIST" && !is_regular_file(p));
    }

public:
    char buffer[CHUNK_SIZE];

    Server(const SOCKET& socket) :clientSocket(socket) {
        memset(buffer, 0, CHUNK_SIZE);
    }

    const int getData() {
        memset(buffer, 0, CHUNK_SIZE);
        return recv(clientSocket, buffer, sizeof(buffer), 0);
    }

    void sendMessage(const char* response)const {
        send(clientSocket, response, (int)strlen(response), 0);
    }

    void sendResponse()  {
        string command = getCommand();
        path p = getPath(command);
        if (isInvalidPath(p, command)) {
            sendMessage("Request denied.");
            return;
        }
        if (command == "GET") get(p);
        else if (command == "LIST") list(p);
        else if (command == "PUT") put(p.filename(), getFileSize());
        else if (command == "INFO") info(p);
        else if (command == "DELETE") deleteFile(p);
        else  sendMessage("Request denied.");
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
    if (bind(serverSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR)
    {
        std::cerr << "Bind failed with error: " << WSAGetLastError() << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }
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

    Server server(clientSocket);
    
    if (server.getData() > 0) {
        cout << "\033[95m" << server.buffer << endl;
        server.sendMessage("Hello, client! This is the server.");
    }
    while (true) {
        if (server.getData() > 0) {
            cout << "\033[95m" << server.buffer <<"\033[94m" <<endl;
            server.sendResponse();
        }
   }
        
    closesocket(clientSocket);
    closesocket(serverSocket);
    WSACleanup();
    return 0;
    
}
