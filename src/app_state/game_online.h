#ifndef GAME_ONLINE_H
#define GAME_ONLINE_H

#include "game.h"
#include "../appconfig.h"
#include <winsock2.h>
#include <vector>
#include "../objects/object.h"

class GameOnline : public Game
{

public:
    struct ClientInput
    {
        bool up;
        bool down;
        bool left;
        bool right;
        bool fire;
    };

    // struct để lưu trữ snapshot của game
    struct BonusData
    {
        double pos_x;
        double pos_y;
        SpriteType type;
        bool is_active;
    };

    // Bullet states
    struct BulletSnapshot
    {
        double pos_x, pos_y;
        int direction;
        bool collide;
        bool increased_damage;
    };

    // Player states
    struct PlayerSnapshot
    {
        double pos_x, pos_y;
        int direction;
        bool is_firing;
        bool is_destroyed;
        int lives_count;
        int star_count;
        std::vector<BulletSnapshot> bullets;
    };

    // Enemy states
    struct EnemySnapshot
    {
        double pos_x, pos_y;
        int direction;
        int type;
        int lives_count;
        bool is_destroyed;
        bool has_bonus;
        std::vector<BulletSnapshot> bullets;
    };

    struct GameSnapshot
    {
        std::vector<PlayerSnapshot> players;
        std::vector<EnemySnapshot> enemies;
        std::vector<BonusData> bonuses;
        bool eagle_destroyed;
        int current_level;
        bool game_over;
        double game_over_position;
    };

    GameOnline(const std::string &room_code, bool is_host, const std::vector<std::string> &players);
    ~GameOnline();

    void update(Uint32 dt) override;

    void checkConnect();
    void HandleHostData();
    void HandleClientData();
    void HostUpdate(Uint32 dt);
    void ClientUpdate();

    GameSnapshot captureGameState();
    std::string GameStateSendData();
    void RecvGameState(const std::string &data);

protected:
private:
    ClientInput m_client_input;
    std::string m_room_code;
    bool m_is_host;
    SOCKET m_sync_socket;
    struct sockaddr_in m_server_addr;
    Uint32 m_last_sync_time;
    static const Uint32 SYNC_INTERVAL = 50; // 50ms
};

#endif