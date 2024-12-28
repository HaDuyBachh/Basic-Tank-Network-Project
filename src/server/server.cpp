#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <winsock2.h>
#include <pthread.h>

#pragma comment(lib, "ws2_32.lib")

struct ClientData {
    SOCKET client_socket;
    struct sockaddr_in client_addr;
};

void* handleClient(void* arg) {
    ClientData* client_data = static_cast<ClientData*>(arg);
    SOCKET client_socket = client_data->client_socket;
    delete client_data;

    char buffer[1024] = {0};
    recv(client_socket, buffer, sizeof(buffer), 0);

    std::string request(buffer);
    std::string username = request.substr(request.find("username=") + 9, request.find("&password=") - (request.find("username=") + 9));
    std::string password = request.substr(request.find("&password=") + 10);

    std::ifstream user_log("user.log");
    std::string line;
    bool user_exists = false;

    while (std::getline(user_log, line)) {
        if (line.find(username) != std::string::npos) {
            user_exists = true;
            break;
        }
    }
    user_log.close();

    if (!user_exists) {
        std::ofstream user_log("user.log", std::ios::app);
        user_log << username << ":" << password << std::endl;
        user_log.close();
        send(client_socket, "OK", 2, 0);
    } else {
        send(client_socket, "EXISTS", 6, 0);
    }

    closesocket(client_socket);
    return nullptr;
}

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8888);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    bind(server_socket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    listen(server_socket, 5);

    std::cout << "Server running..." << std::endl;

    while (true) {
        ClientData* client_data = new ClientData;
        int client_size = sizeof(client_data->client_addr);
        client_data->client_socket = accept(server_socket, (struct sockaddr*)&client_data->client_addr, &client_size);

        pthread_t thread_id;
        pthread_create(&thread_id, nullptr, handleClient, client_data);
        pthread_detach(thread_id); // Tách thread để tự động thu hồi tài nguyên sau khi kết thúc
    }

    closesocket(server_socket);
    WSACleanup();
    return 0;
}