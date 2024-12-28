#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <vector>
#include <winsock2.h>
#include <pthread.h>
#include <map>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")

#define TCP_PORT 8888
#define UDP_PORT 8889
#define BUFFER_SIZE 1024

struct ClientData {
    SOCKET client_socket;
    struct sockaddr_in client_addr;
};

// Thông tin phòng chơi
struct RoomInfo {
    std::string room_code;
    std::vector<std::string> players;
    std::string host;     // Người tạo phòng
    bool is_playing;

    // Constructor để khởi tạo các thành viên
    RoomInfo() : is_playing(false) {}
};

// Thông tin người chơi

struct PlayerInfo {
    std::string username;
    bool is_authenticated;
    struct sockaddr_in udp_addr;
    std::string current_room;
    bool is_host;         // Có phải host không

    // Constructor
    PlayerInfo() : is_authenticated(false), is_host(false) {}
};

// Maps toàn cục
std::map<std::string, PlayerInfo> players;
std::map<std::string, RoomInfo> rooms;
pthread_mutex_t players_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t rooms_mutex = PTHREAD_MUTEX_INITIALIZER;


// Hàm xử lý tạo phòng mới
std::string handleCreateRoom(const std::string& username, const std::string& room_code) {
    pthread_mutex_lock(&rooms_mutex);
    
    // Kiểm tra phòng đã tồn tại
    if (rooms.find(room_code) != rooms.end()) {
        pthread_mutex_unlock(&rooms_mutex);
        return "ROOM_EXISTS";
    }

    // Tạo phòng mới
    RoomInfo room;
    room.room_code = room_code;
    room.players.push_back(username);
    room.host = username;
    room.is_playing = false;
    rooms[room_code] = room;

    // Cập nhật thông tin người chơi
    pthread_mutex_lock(&players_mutex);
    if (players.find(username) != players.end()) {
        players[username].current_room = room_code;
        players[username].is_host = true;
    }
    pthread_mutex_unlock(&players_mutex);

    pthread_mutex_unlock(&rooms_mutex);
    return "ROOM_CREATED:" + room_code;
}

// Hàm xử lý tham gia phòng
std::string handleJoinRoom(const std::string& username, const std::string& room_code) {
    pthread_mutex_lock(&rooms_mutex);
    
    // Kiểm tra phòng tồn tại
    if (rooms.find(room_code) == rooms.end()) {
        pthread_mutex_unlock(&rooms_mutex);
        return "ROOM_NOT_FOUND";
    }

    // Kiểm tra người chơi đã trong phòng
    auto& room = rooms[room_code];
    for (const auto& player : room.players) {
        if (player == username) {
            pthread_mutex_unlock(&rooms_mutex);
            return "ALREADY_IN_ROOM";
        }
    }

    // Thêm người chơi vào phòng
    room.players.push_back(username);

    // Cập nhật thông tin người chơi
    pthread_mutex_lock(&players_mutex);
    if (players.find(username) != players.end()) {
        players[username].current_room = room_code;
        players[username].is_host = false;
    }
    pthread_mutex_unlock(&players_mutex);

    // Tạo danh sách người chơi
    std::string player_list = "ROOM_JOINED:" + room_code + ":";
    for (const auto& player : room.players) {
        player_list += player + ",";
    }
    if (!room.players.empty()) {
        player_list.pop_back(); // Xóa dấu phẩy cuối cùng
    }

    pthread_mutex_unlock(&rooms_mutex);
    return player_list;
}

// Hàm cập nhật danh sách người chơi
std::string getUpdatedPlayerList(const std::string& room_code) {
    pthread_mutex_lock(&rooms_mutex);
    std::string response = "PLAYER_LIST:";
    
    if (rooms.find(room_code) != rooms.end()) {
        for (const auto& player : rooms[room_code].players) {
            response += player + ",";
        }
        if (response.back() == ',') {
            response.pop_back();
        }
    }
    
    pthread_mutex_unlock(&rooms_mutex);
    return response;
}

// Hàm xử lý TCP client
void* handleTCPClient(void* arg) {
    ClientData* client_data = static_cast<ClientData*>(arg);
    SOCKET client_socket = client_data->client_socket;
    delete client_data;

    char buffer[BUFFER_SIZE] = {0};
    recv(client_socket, buffer, sizeof(buffer), 0);
    std::string request(buffer);

    std::string response;
    if (request.find("CREATE_ROOM:") == 0) {
        // Format: CREATE_ROOM:username:room_code
        size_t first_colon = request.find(':');
        size_t second_colon = request.find(':', first_colon + 1);
        std::string username = request.substr(first_colon + 1, second_colon - first_colon - 1);
        std::string room_code = request.substr(second_colon + 1);
        response = handleCreateRoom(username, room_code);
    }
    else if (request.find("JOIN_ROOM:") == 0) {
        // Format: JOIN_ROOM:username:room_code
        size_t first_colon = request.find(':');
        size_t second_colon = request.find(':', first_colon + 1);
        std::string username = request.substr(first_colon + 1, second_colon - first_colon - 1);
        std::string room_code = request.substr(second_colon + 1);
        response = handleJoinRoom(username, room_code);
    }
    else if (request.find("UPDATE_PLAYERS:") == 0) {
        // Format: UPDATE_PLAYERS:room_code
        std::string room_code = request.substr(request.find(':') + 1);
        response = getUpdatedPlayerList(room_code);
    }
    else if (request.find("signin:") == 0) {
        // Xử lý đăng nhập
        std::string credentials = request.substr(7); // Bỏ qua "signin:"
        size_t separator = credentials.find('&');
        std::string username = credentials.substr(0, separator);
        std::string password = credentials.substr(separator + 1);

        // Kiểm tra thông tin đăng nhập
        std::ifstream user_log("user.log");
        std::string line;
        bool login_success = false;

        while (std::getline(user_log, line)) {
            if (line == username + ":" + password) {
                login_success = true;
                break;
            }
        }
        user_log.close();

        if (login_success) {
            pthread_mutex_lock(&players_mutex);
            PlayerInfo player;
            player.username = username;
            player.is_authenticated = true;
            player.current_room = "";
            player.is_host = false;
            players[username] = player;
            pthread_mutex_unlock(&players_mutex);
            response = "OK";
        } else {
            response = "FAILED";
        }
    }
    else {
        // Xử lý đăng ký như cũ
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

            pthread_mutex_lock(&players_mutex);
            PlayerInfo player;
            player.username = username;
            player.is_authenticated = true;
            player.current_room = "";
            player.is_host = false;
            players[username] = player;
            pthread_mutex_unlock(&players_mutex);

            response = "OK";
        } else {
            response = "EXISTS";
        }
    }

    send(client_socket, response.c_str(), response.length(), 0);
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