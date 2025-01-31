#include <iostream>
#include <WinSock2.h>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#define CHUNK_SIZE 1024
using namespace std;
#include <filesystem>
#include <thread>
using namespace std::filesystem;
#pragma comment(lib, "ws2_32.lib")
const string serverDatabase = ".\\Server database";
class Server {
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

    void put(const string& file_path, int size)  {
        string fileName = path(file_path).filename().string();
        string fileNameInDatabase = (path(serverDatabase) / fileName).string();
        ofstream file(fileNameInDatabase, ios::binary | ios::out);
        int i = 0;
        while (i != size) {
            int bytesReceived = getData();
            file.write(buffer, bytesReceived);
            i += bytesReceived;
        }
        file.close();
        sendMessage("File transfer completed!");

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


    string getPath(const string& command, const char* buffer) {
        string input(buffer);
        if (input.length() <= command.length() + 1) return "";
        bool isPut = (command == "PUT");
        int startIndex = command.length() + 1;
        int indexEnd = isPut ? input.rfind(' ') : input.length();
        if (indexEnd <= startIndex) return "";  
        string file;
        if (input[startIndex] == '"' && input[indexEnd - 1] == '"') {
            file = input.substr(startIndex + 1, indexEnd - startIndex - 2);
        }
        else {
            file = input.substr(startIndex, indexEnd - startIndex);
        }
        return file;
    }

    int getFileSize() {
        string input(buffer);
        return stoi(input.substr(input.rfind(' ') + 1));
    }

    string getCommand(const char* input) {
        string command;
        stringstream ss(input);
        ss >> command;
        return command;
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
        if (strlen(buffer)==0 ) {
            sendMessage("Request denied.");
            return;
        }
        string command = getCommand(buffer);
        string path = getPath(command, buffer);
        if (path == "") {
            sendMessage("Request denied.");
            return;
        }
        if (command == "GET") get(path);
        else if (command == "LIST") list(path);
        else if (command == "PUT") put(path, getFileSize());
        else if (command == "INFO") info(path);
        else if (command == "DELETE_FILE") deleteFile(path);
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
        cout << "Client: " << server.buffer << endl;
        server.sendMessage("Hello, client! This is the server.");
    }
    while (true) {

        if (server.getData() > 0) {
            cout << "Client: " << server.buffer << endl;
            server.sendResponse();
        }

   }
        
    

    closesocket(clientSocket);
    closesocket(serverSocket);
    WSACleanup();
    return 0;
    
}
