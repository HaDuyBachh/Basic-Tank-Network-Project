#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <winsock2.h>
#include <pthread.h>
#include <map>

#pragma comment(lib, "ws2_32.lib")

#define TCP_PORT 8888
#define UDP_PORT 8889
#define BUFFER_SIZE 1024

struct ClientData {
    SOCKET client_socket;
    struct sockaddr_in client_addr;
};

// Thêm struct để quản lý thông tin người chơi
struct PlayerInfo {
    std::string username;
    bool is_authenticated;
    struct sockaddr_in udp_addr;
};

// Map để lưu thông tin người chơi
std::map<std::string, PlayerInfo> players;
pthread_mutex_t players_mutex = PTHREAD_MUTEX_INITIALIZER;

// Xử lý TCP client (đăng ký/đăng nhập)
void* handleTCPClient(void* arg) {
    ClientData* client_data = static_cast<ClientData*>(arg);
    SOCKET client_socket = client_data->client_socket;
    delete client_data;

    char buffer[BUFFER_SIZE] = {0};
    recv(client_socket, buffer, sizeof(buffer), 0);

    std::string request(buffer);
    std::string username = request.substr(request.find("username=") + 9, 
                          request.find("&password=") - (request.find("username=") + 9));
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

        // Thêm người chơi mới vào map
        pthread_mutex_lock(&players_mutex);
        PlayerInfo player;
        player.username = username;
        player.is_authenticated = true;
        players[username] = player;
        pthread_mutex_unlock(&players_mutex);

        send(client_socket, "OK", 2, 0);
    } else {
        send(client_socket, "EXISTS", 6, 0);
    }

    closesocket(client_socket);
    return nullptr;
}

// Xử lý UDP data (game state)
void* handleUDP(void* arg) {
    SOCKET udp_socket = *static_cast<SOCKET*>(arg);
    char buffer[BUFFER_SIZE];
    struct sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);

    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        int recv_len = recvfrom(udp_socket, buffer, BUFFER_SIZE, 0,
                               (struct sockaddr*)&client_addr, &client_addr_len);

        if (recv_len > 0) {
            // Giả sử format: "username:x:y:direction"
            std::string data(buffer);
            size_t first_colon = data.find(':');
            if (first_colon != std::string::npos) {
                std::string username = data.substr(0, first_colon);
                
                // Update player's UDP address
                pthread_mutex_lock(&players_mutex);
                if (players.find(username) != players.end()) {
                    players[username].udp_addr = client_addr;
                    
                    // Broadcast position to all other players
                    for (auto& player : players) {
                        if (player.first != username && player.second.is_authenticated) {
                            sendto(udp_socket, buffer, recv_len, 0,
                                  (struct sockaddr*)&player.second.udp_addr,
                                  sizeof(player.second.udp_addr));
                        }
                    }
                }
                pthread_mutex_unlock(&players_mutex);
            }
        }
    }
    return nullptr;
}

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    // Khởi tạo TCP socket
    SOCKET tcp_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in tcp_addr;
    tcp_addr.sin_family = AF_INET;
    tcp_addr.sin_port = htons(TCP_PORT);
    tcp_addr.sin_addr.s_addr = INADDR_ANY;

    // Bind TCP socket
    if (bind(tcp_socket, (struct sockaddr*)&tcp_addr, sizeof(tcp_addr)) == SOCKET_ERROR) {
        std::cout << "TCP bind failed" << std::endl;
        return 1;
    }
    listen(tcp_socket, 5);

    // Khởi tạo UDP socket
    SOCKET udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in udp_addr;
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_port = htons(UDP_PORT);
    udp_addr.sin_addr.s_addr = INADDR_ANY;

    // Bind UDP socket
    if (bind(udp_socket, (struct sockaddr*)&udp_addr, sizeof(udp_addr)) == SOCKET_ERROR) {
        std::cout << "UDP bind failed" << std::endl;
        closesocket(tcp_socket);
        return 1;
    }

    // Tạo thread xử lý UDP
    pthread_t udp_thread;
    pthread_create(&udp_thread, nullptr, handleUDP, &udp_socket);
    pthread_detach(udp_thread);

    std::cout << "Server running on TCP port " << TCP_PORT 
              << " and UDP port " << UDP_PORT << std::endl;

    // Main loop xử lý TCP connections
    while (true) {
        ClientData* client_data = new ClientData;
        int client_size = sizeof(client_data->client_addr);
        client_data->client_socket = accept(tcp_socket, 
            (struct sockaddr*)&client_data->client_addr, &client_size);

        pthread_t tcp_thread;
        pthread_create(&tcp_thread, nullptr, handleTCPClient, client_data);
        pthread_detach(tcp_thread);
    }

    closesocket(tcp_socket);
    closesocket(udp_socket);
    WSACleanup();
    return 0;
}