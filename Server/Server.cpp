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

class RequestProcessing {

    string request;
    string command;
    path path_to;
    bool invalid;

    string ToUpper(string word)const {
        transform(word.begin(), word.end(), word.begin(), ::toupper);
        return word;
    }

    string GetCommandForConstructor() {
        string command;
        stringstream ss(request);
        ss >> command;
        return command;
    }

    const path GetPathForConstructor() {
        bool is_put_command = (command == "PUT");
        int start_index = command.length() + 1;
        int index_end = is_put_command ? request.rfind(' ') : request.length();
        string file;
        if (request[start_index] == '"' && request[index_end - 1] == '"')file = request.substr(start_index + 1, index_end - start_index - 2);
        else file = request.substr(start_index, index_end - start_index);
        replace(file.begin(), file.end(), '\\', '/');
        return file;
    }

    bool IsInvalidPath() {
        if (!exists(path_to)) return true;
        if (command == "LIST" || command == "REMOVE")  return !is_directory(path_to);
        else return !is_regular_file(path_to);
    }

public:
    RequestProcessing(const char* input): request(input){
        command = ToUpper(GetCommandForConstructor());
        path_to = GetPathForConstructor();
        invalid = IsInvalidPath();   
    }

     int GetFileSize()const {
        int size = 0;
        if (command == "PUT" && !invalid)size = stoi(request.substr(request.rfind(' ') + 1));
        return size;
     }  

    bool isInValid()const {
        return invalid;
    }

    const string GetCommand() const {
        return command;
    }

    const path GetPath() const {
        return path_to;
    }
};

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
            SendResponse("The path specified is not a regular file!");
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
        SendResponse("File transfer completed!");

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
            if (list.empty())SendResponse("The folder is empty!");
            else if (list.back() == '\n') {
                list.pop_back();
                SendResponse(list.c_str());
            }         
        }
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

    void RemoveFolder(const path& folder)const {
         int count = remove_all(folder);
         count > 0 ? SendResponse("Successful removal!") : SendResponse("The directory could not be deleted!");
    }

    void DeleteFileSpecified(const path& path_to) const {
        remove(path_to)? SendResponse("Successful removal!"): SendResponse("The file could not be deleted!");
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
        //cout << "\033[94m" << response << "\033[0m" << endl;
    }

    void ProcessRequest()  {
        RequestProcessing request(buffer_);
        if (request.isInValid()) {
            SendResponse("Request denied.");
            return;
        }
        string command = request.GetCommand();
        path p = request.GetPath();
       
        if (command == "GET") Get(p);
        else if (command == "LIST") List(p);
        else if (command == "PUT") Put(p.filename(), request.GetFileSize());
        else if (command == "INFO") GetFileInfo(p);
        else if (command == "DELETE") DeleteFileSpecified(p);
        else if (command == "REMOVE")RemoveFolder(p);
        else  SendResponse("Request denied.");
    }
};

int main()
{
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        cerr << "\033[94mWSAStartup failed!\033[0m" << endl;
        return 1;
    }

    const int port = 12345;
    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET) {
        cerr << "\033[94mError craeting socket: " << WSAGetLastError() << "\033[0m" << endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) == SOCKET_ERROR)
    {
        cerr << "\033[94mBind failed with error: " << WSAGetLastError() << "\033[0m" << endl;
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR) {
        cerr << "\033[94mListen failed with error: " << WSAGetLastError() << "\033[0m" <<endl;
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    cout << "\033[94mServer listening on port " << port << "\033[0m" << endl;

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
