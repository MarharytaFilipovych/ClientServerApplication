#include <iostream>
#include <WinSock2.h>
#include <fstream>
#include <string>
#include <sstream>
#include <filesystem>
#include <vector>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <atomic>
#pragma comment(lib, "ws2_32.lib")
#define CHUNK_SIZE 1024
#define MAX_CLIENTS 20
using namespace std;
using namespace filesystem;
const path database = ".\\database";
mutex console;
atomic<int> active_clients(0);


class Stats {
    mutex m;
    unordered_map<string, int> stats;
public:
    bool stop;
    void ChangeMap(const string& command) {
        lock_guard<mutex> lock(m);
        stats[command]++;
    }

    void outPutStats() {
        this_thread::sleep_for(chrono::minutes(1));
        while (!stop) {
            this_thread::sleep_for(chrono::minutes(1));
            for (auto& record : stats) {
                lock_guard<mutex> lock1(console);
                cout << "\033[92mCommand " << "\033[36m" << record.first << "\033[92m was called " << "\033[36m " << record.second << "\033[92m times\033[94m" << endl;
            }
        }
    }
};

Stats statistics;

class RequestProcessing {

    string request;
    string command;

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
  
public:
    RequestProcessing(const char* input): request(input){
        command = ToUpper(GetCommandForConstructor());
    }

     bool ConatinsInvalidPath(const path& p) const  {
         if (!exists(p)) return true;
         if (command == "REMOVE")  return !is_directory(p);
         else return !is_regular_file(p);
     }

    const string GetCommand() const {
        return command;
    }

    const path GetPath() {
        if (command == "LIST")return "";
        int start_index = command.length() + 1;
        int index_end =request.length();
        string file;
        if (request[start_index] == '"' && request[index_end - 1] == '"')file = request.substr(start_index + 1, index_end - start_index - 2);
        else file = request.substr(start_index, index_end - start_index);
        replace(file.begin(), file.end(), '\\', '/');
        return file;
    }
};

struct Client {
    const SOCKET socket;
    char buffer_[CHUNK_SIZE];
    const path folder;
    const string name;
    Client(const SOCKET& socket, const string& client_name):socket(socket), name(client_name), folder(database / client_name){
        memset(buffer_, 0, CHUNK_SIZE);
        create_directory(folder);
    } 
};

class ServedClient {

    Client client_;

    const string GetFilePermissions(const path& file)const {
        perms p = status(file).permissions();
        string result;
        result += ((p & perms::owner_read) != perms::none ? "r" : "-");
        result += ((p & perms::owner_write) != perms::none ? "w" : "-");
        result += ((p & perms::owner_exec) != perms::none ? "x" : "-");
        return result;
    }

    void Get(const path& file_name) const {
        ifstream file(file_name, ios::binary);
        int size = htonl(file_size(file_name));
        send(client_.socket, (char*)(&size), sizeof(size), 0);
        char buffer_for_data[CHUNK_SIZE];
        while (file.read(buffer_for_data, sizeof(buffer_for_data))) {
            send(client_.socket, buffer_for_data, (int)(file.gcount()), 0);
        }
        int remaining_bytes = file.gcount();
        if (remaining_bytes > 0) {
            send(client_.socket, buffer_for_data, (int)remaining_bytes, 0);
        }
        file.close();
        SendResponse("File transfer completed!");
    }

    void List()const {
        string list;
        for (const auto& entry : directory_iterator(client_.folder)) {        
            list.append(relative(entry.path(), client_.folder).string() + "\n");                    
        }
        if (list.empty())SendResponse("The folder is empty!");
        else if (list.back() == '\n') {
            list.pop_back();
            SendResponse(list.c_str());
        }   
    }

    void Put(const path& file_path) {
        int size_of_file;
        recv(client_.socket, (char*)(&size_of_file), sizeof(size_of_file), 0);
        size_of_file = ntohl(size_of_file);

        path file_name = client_.folder / file_path.filename();
        ofstream file(file_name, ios::binary);
        int i = 0;
        while (i != size_of_file) {
            int bytes_received = GetReadBytes();
            file.write(client_.buffer_, bytes_received);
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

    ServedClient(const SOCKET& client_socket, const string& name) :client_(client_socket, name) {}
  
    const int GetReadBytes() {
        memset(client_.buffer_, 0, CHUNK_SIZE);
        return recv(client_.socket, client_.buffer_, sizeof(client_.buffer_), 0);
    }

    void SendResponse(const char* response)const {
        send(client_.socket, response, (int)strlen(response), 0);
        //cout << "\033[94m" << response << "\033[0m" << endl;
    }

    const char* ReadBuffer() const {
        return client_.buffer_;
    }
  
    void ProcessRequest()  {
        RequestProcessing request(client_.buffer_);
        const string command = request.GetCommand();
        statistics.ChangeMap(command);
        if (command == "LIST") List();
        else if (command == "PUT")Put(request.GetPath().filename());
        else {
            const path p = client_.folder / request.GetPath();
            if (request.ConatinsInvalidPath(p)) {
                SendResponse("Request denied. Specified file does not exist.");
                return;
            }
            if (command == "GET") {
                SendResponse("OK");
                Get(p); 
            }
            else if (command == "INFO") GetFileInfo(p);
            else if (command == "DELETE") DeleteFileSpecified(p);
            else RemoveFolder(p);
        }    
    }
};
bool IsOldClient(const string& hello) {
    return hello == "Hello, server! How are you?";
}

string GetClientName(const string& hello) {
    if (IsOldClient(hello))return "old_version";
    return hello.substr(hello.find_last_of(' ') + 1);
}

void CloseClient(const SOCKET& client_socket, const string& name) {
    closesocket(client_socket);
    active_clients--;
    lock_guard<mutex> lock1(console);
    cout << "\033[94mClient " << name << " disconnected.\033[0m" << endl;
}


void HandleClient(const SOCKET& client_socket) {
    char buffer[1024];
    memset(buffer, 0, 1024);
    string name, hello;
    int bytesReceived = recv(client_socket, buffer, sizeof(buffer), 0);
    if (bytesReceived > 0) {
        {
            lock_guard<mutex> lock1(console);
            cout << "\033[95m" << buffer << "\033[0m" << endl;
        }
        string got_message = string(buffer, bytesReceived);
        name = GetClientName(got_message);
        IsOldClient(got_message) ?  hello = "Hello, client! This is the server.": hello = "Hello, " + name + "! This is the server.";
        send(client_socket, hello.c_str(), (int)strlen(hello.c_str()), 0);
    }
    ServedClient client_server(client_socket, name);
    while (client_server.GetReadBytes() > 0) {
        {
            lock_guard<mutex> lock1(console);
            cout << "\033[94m" << name << ": " << client_server.ReadBuffer() << "\033[94m" << endl;
        }
        client_server.ProcessRequest();
    }
    CloseClient(client_socket, name);
}

int main()
{
    vector<thread> threads;
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

    thread statistic_displayer = thread(&Stats::outPutStats, &statistics);
    while (true) {
        if (active_clients.load() <= MAX_CLIENTS) {
            SOCKET client_socket = accept(server_socket, nullptr, nullptr);
            if (client_socket == INVALID_SOCKET) {
                cerr << "\033[95mAccept failed with error: " << WSAGetLastError() << "\033[0m" << endl;
                closesocket(server_socket);
                WSACleanup();
                continue;
            }
            active_clients++;
            threads.emplace_back(HandleClient, client_socket);
            threads.back().detach();
        }
    }
    statistics.stop = true;
    statistic_displayer.join();
    closesocket(server_socket);
    WSACleanup();
    return 0; 
}
