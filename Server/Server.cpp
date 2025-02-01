#include <iostream>
#include <WinSock2.h>
#include <fstream>
#include <string>
#include <sstream>
#include <filesystem>
#pragma comment(lib, "ws2_32.lib")
#define CHUNK_SIZE 1024
using namespace std;
using namespace filesystem;
const path database = ".\\database";

class Server {

    const SOCKET client_socket_;

    const string GetFilePermissions(const path& file)const {
        perms p = status(file).permissions();
        string result;
        result += ((p & perms::owner_read) != perms::none ? "r" : "-");
        result += ((p & perms::owner_write) != perms::none ? "w" : "-");
        result += ((p & perms::owner_exec) != perms::none ? "x" : "-");
        return result;
    }

    void Get(const path& file_name) const {
        if (!is_regular_file(file_name)) {
            SendResponse("I am unable to open your file!");
            return;
        }
        ifstream file(file_name, ios::binary);

        SendResponse(to_string(file_size(file_name)).c_str());
        char buffer_for_data[CHUNK_SIZE];
        while (file.read(buffer_for_data, sizeof(buffer_for_data))) {
            send(client_socket_, buffer_for_data, (int)(file.gcount()), 0);
        }
        if (file.gcount() > 0)send(client_socket_, buffer_for_data, (int)(file.gcount()), 0);
        file.close();
    }

    void List(const path& dir_path)const {
        if (is_directory(dir_path)) {
            string list;
            for (const auto& entry : directory_iterator(dir_path)) {
                try {
                    list.append(entry.path().string() + "\n");
                }
                catch (const exception& e) {
                    SendResponse("Not possible to list contents of directory that contains cyryllic file/dir names.");
                    return;
                }
            }
            SendResponse(list.c_str());
        }
        else SendResponse("Looks like this directory doesn't exist!");
    }

    void Put(const path& file_path, int size)  {
        path file_name = database /file_path.filename();
        ofstream file(file_name, ios::binary);
        int i = 0;
        while (i != size) {
            int bytes_received = GetReadBytes();
            file.write(buffer_, bytes_received);
            i += bytes_received;
        }
        file.close();
        SendResponse("File transfer completed!");
    }

    void GetFileInfo(const path& file)const {
        if (!is_regular_file(file)) {
            SendResponse("Request denied.");
            return;
        }
        string size = to_string(file_size(file));
        string time = format("Last modified: {}", last_write_time(file));
        string path = absolute(file).string();
        string info = "Size: " + size + " bytes\n" + time + "\nAbsolute path: " + path + "\nPermissions: "+ GetFilePermissions(file);
        SendResponse(info.c_str());             
    }

    void DeleteFileSpecified(path& file) const {
        if(remove(file))SendResponse("Successful removal!");
        else SendResponse("The file doesn't exist!");
    }

    const path GetPath(const string& command)const {
        string input(buffer_);
        bool is_put_command = (command == "PUT");
        int start_index = command.length() + 1;
        int index_end = is_put_command ? input.rfind(' ') : input.length();
        string file;
        if (input[start_index] == '"' && input[index_end - 1] == '"')file = input.substr(start_index + 1, index_end - start_index - 2);                   
        else file = input.substr(start_index, index_end - start_index);
        replace(file.begin(), file.end(), '\\', '/');
        return file;
    }

    const int GetFileSize()const {
        string input(buffer_);
        return stoi(input.substr(input.rfind(' ') + 1));
    }

    const string GetCommand()const {
        string command;
        stringstream ss(buffer_);
        ss >> command;
        return command;
    }

    bool IsInvalidPath(const path& p, const string& command)const {
        return !exists(p) || (command == "LIST" && !is_directory(p)) || (command != "LIST" && !is_regular_file(p));
    }

public:
    char buffer_[CHUNK_SIZE];

    Server(const SOCKET& socket) :client_socket_(socket) {
        memset(buffer_, 0, CHUNK_SIZE);
    }

    const int GetReadBytes() {
        memset(buffer_, 0, CHUNK_SIZE);
        return recv(client_socket_, buffer_, sizeof(buffer_), 0);
    }

    void SendResponse(const char* response)const {
        send(client_socket_, response, (int)strlen(response), 0);
    }

    void ProcessRequest()  {
        string command = GetCommand();
        path p = GetPath(command);
        if (IsInvalidPath(p, command)) {
            SendResponse("Request denied.");
            return;
        }
        if (command == "GET") Get(p);
        else if (command == "LIST") List(p);
        else if (command == "PUT") Put(p.filename(), GetFileSize());
        else if (command == "INFO") GetFileInfo(p);
        else if (command == "DELETE") DeleteFileSpecified(p);
        else  SendResponse("Request denied.");
    }
};


int main()
{
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        cerr << "WSAStartup failed!" << endl;
        return 1;
    }

    const int port = 12345;
    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET) {
        cerr << "\033[95mError craeting socket: " << WSAGetLastError() << "\033[0m" << endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) == SOCKET_ERROR)
    {
        cerr << "\033[95mBind failed with error: " << WSAGetLastError() << "\033[0m" << endl;
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR) {
        cerr << "\033[95mListen failed with error: " << WSAGetLastError() << "\033[0m" <<endl;
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    cout << "\033[95mServer listening on port " << port << "\033[0m" << endl;

    SOCKET client_socket = accept(server_socket, nullptr, nullptr);
    if (client_socket == INVALID_SOCKET) {
        cerr << "\033[95mAccept failed with error: " << WSAGetLastError() << "\033[0m" << endl;
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    Server server(client_socket);
    
    if (server.GetReadBytes() > 0) {
        cout << "\033[95m" << server.buffer_ <<"\033[0m"<< endl;
        server.SendResponse("Hello, client! This is the server.");
    }
    while (server.GetReadBytes() > 0) {   
        cout << "\033[95m" << server.buffer_ <<"\033[94m" <<endl;
        server.ProcessRequest();     
   }
        
    closesocket(client_socket);
    closesocket(server_socket);
    WSACleanup();
    return 0; 
}
