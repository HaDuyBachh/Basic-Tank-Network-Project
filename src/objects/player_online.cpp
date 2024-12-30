#include "player_online.h" 
#include "../appconfig.h"
#include <sstream>             
#include <vector>              
#include <string>            

PlayerOnline::PlayerOnline(double x, double y, SpriteType type, bool is_host, const std::string& room_code) 
    : Player(x, y, type), m_is_host(is_host), m_room_code(room_code) {
    
    // Khởi tạo socket
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    
    // Cấu hình server address
    m_server_addr.sin_family = AF_INET;
    m_server_addr.sin_port = htons(8889); // Port UDP
    m_server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
}

PlayerOnline::~PlayerOnline() {
    closesocket(m_socket);
    WSACleanup();
}

void PlayerOnline::update(Uint32 dt) {
    Player::update(dt);
    
    // Sau khi update, gửi dữ liệu mới
    if (m_is_host) {
        sendData();
    } else {
        receiveData();
    }
}

void PlayerOnline::sendData() {
    // Đóng gói dữ liệu player
    m_player_data.pos_x = pos_x;
    m_player_data.pos_y = pos_y;
    m_player_data.direction = direction;
    m_player_data.is_firing = (m_fire_time > AppConfig::player_reload_time);
    m_player_data.is_destroyed = testFlag(TSF_DESTROYED);
    
    // Gửi dữ liệu qua UDP
    std::string data = m_room_code + ":" + std::to_string(m_player_data.pos_x) + "," + 
                      std::to_string(m_player_data.pos_y) + "," +
                      std::to_string(m_player_data.direction) + "," +
                      std::to_string(m_player_data.is_firing) + "," +
                      std::to_string(m_player_data.is_destroyed);
                      
    sendto(m_socket, data.c_str(), data.length(), 0,
           (struct sockaddr*)&m_server_addr, sizeof(m_server_addr));
}

void PlayerOnline::receiveData() {
    char buffer[1024];
    struct sockaddr_in from_addr;
    int from_len = sizeof(from_addr);
    
    // Nhận dữ liệu từ server
    int recv_len = recvfrom(m_socket, buffer, sizeof(buffer), 0,
                           (struct sockaddr*)&from_addr, &from_len);
    
    if (recv_len > 0) {
        buffer[recv_len] = '\0';
        std::string data(buffer);
        
        // Parse dữ liệu
        size_t pos = data.find(':');
        if (pos != std::string::npos && data.substr(0, pos) == m_room_code) {
            std::string player_data = data.substr(pos + 1);
            std::stringstream ss(player_data);
            std::string item;
            std::vector<std::string> values;
            
            while (std::getline(ss, item, ',')) {
                values.push_back(item);
            }
            
            if (values.size() >= 5) {
                pos_x = std::stod(values[0]);
                pos_y = std::stod(values[1]); 
                // direction = std::stoi(values[2]);
                // Chuyển đổi an toàn từ int sang Direction
                int dir_value = std::stoi(values[2]);
                switch(dir_value) {
                    case 0: direction = D_UP; break;
                    case 1: direction = D_RIGHT; break;
                    case 2: direction = D_DOWN; break;
                    case 3: direction = D_LEFT; break;
                    default: direction = D_UP; break;  // Giá trị mặc định
                }
                bool is_firing = std::stoi(values[3]);
                bool is_destroyed = std::stoi(values[4]);
                
                if (is_firing) {
                    fire();
                }
                
                if (is_destroyed) {
                    destroy();
                }
            }
        }
    }
}