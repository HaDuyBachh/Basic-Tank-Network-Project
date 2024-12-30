#ifndef GAME_ONLINE_H
#define GAME_ONLINE_H

#include "game.h"
#include "../objects/player_online.h"
#include "../appconfig.h"

class GameOnline : public Game {

struct GameState {
        struct EnemyData {
            double pos_x;
            double pos_y;
            int direction;
            int type;
            int lives_count;
            bool is_destroyed;
        };

        struct BonusData {
            double pos_x;
            double pos_y;
            int type;
            bool is_active;
        };

        std::vector<PlayerData> players;
        std::vector<EnemyData> enemies;
        std::vector<BonusData> bonuses;
        bool eagle_destroyed;
        int current_level;
        int enemy_count;
    };

public:

    GameOnline(const std::string& room_code, bool is_host, const std::vector<std::string>& players);
    ~GameOnline();
    
    void update(Uint32 dt) override;

    void checkConnect();
    void HandleHostData();
    void HandleClientData();
    void HostUpdate(Uint32 dt);
    void ClientUpdate();

protected:


private:
    std::string m_room_code;
    bool m_is_host;
    SOCKET m_sync_socket;
    struct sockaddr_in m_server_addr;
    Uint32 m_last_sync_time;
    static const Uint32 SYNC_INTERVAL = 50; // 50ms
    GameState m_game_state;
};

#endif