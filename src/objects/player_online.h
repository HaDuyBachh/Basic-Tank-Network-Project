#ifndef PLAYER_ONLINE_H 
#define PLAYER_ONLINE_H

#include "player.h"
#include <string>
#include <winsock2.h>

// Cấu trúc dữ liệu để gửi/nhận thông tin player
struct PlayerData {
    double pos_x;
    double pos_y; 
    int direction;
    bool is_firing;
    bool is_destroyed;
};

class PlayerOnline : public Player {
public:
    PlayerOnline(double x, double y, SpriteType type, bool is_host, const std::string& room_code);
    ~PlayerOnline();
    
    void update(Uint32 dt) override;
    void sendData(); // Gửi dữ liệu cho player khác
    void receiveData(); // Nhận dữ liệu từ player khác
    
private:
    bool m_is_host;
    std::string m_room_code;
    SOCKET m_socket;
    struct sockaddr_in m_server_addr;
    PlayerData m_player_data;
};

#endif