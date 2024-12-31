#ifndef GAME_ONLINE_H
#define GAME_ONLINE_H

#include "game.h"
#include "../appconfig.h"
#include <winsock2.h>
#include <vector>
#include "../objects/object.h"

class GameOnline : public AppState
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

    // Brick states
    struct BrickData
    {
        int row;
        int col;
        int state_code;
        int collision_count;
        bool to_erase;
    };

    // Bonus states
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

    // struct để lưu trữ snapshot của game
    struct GameSnapshot
    {
        std::vector<PlayerSnapshot> players;
        std::vector<EnemySnapshot> enemies;
        std::vector<BonusData> bonuses;
        std::vector<BrickData> bricks;
        bool eagle_destroyed;
        int current_level;
        bool game_over;
        double game_over_position;
    };

    GameOnline(const std::string &room_code, bool is_host, const std::vector<std::string> &players);
    virtual ~GameOnline();

    void update(Uint32 dt);
    void eventProcess(SDL_Event *ev);
    AppState *nextState();

    void updateCollider(Uint32 dt);
    bool finished() const;

    void checkConnect();
    void HandleHostData();
    void HandleClientData();
    void HostUpdate(Uint32 dt);
    void ClientUpdate(Uint32 dt);

    GameSnapshot captureGameState();
    std::string GameStateSendData();
    void RecvGameState(const std::string &data);

    void loadLevel(std::string path);
    void clearLevel();
    void nextLevel();
    void generateEnemy();
    void generateBonus();
    void draw();

    void checkCollisionTankWithLevel(Tank *tank, Uint32 dt);
    void checkCollisionTwoTanks(Tank *tank1, Tank *tank2, Uint32 dt);
    void checkCollisionBulletWithLevel(Bullet *bullet);
    void checkCollisionBulletWithBush(Bullet *bullet);
    void checkCollisionPlayerBulletsWithEnemy(Player *player, Enemy *enemy);
    void checkCollisionEnemyBulletsWithPlayer(Enemy *enemy, Player *player);
    void checkCollisionTwoBullets(Bullet *bullet1, Bullet *bullet2);
    void checkCollisionPlayerWithBonus(Player *player, Bonus *bonus);

    int m_level_columns_count;
    int m_level_rows_count;
    std::vector<std::vector<Object *>> m_level;
    std::vector<Object *> m_bushes;
    std::vector<Enemy *> m_enemies;
    std::vector<Player *> m_players;
    std::vector<Player *> m_killed_players;
    std::vector<Bonus *> m_bonuses;
    Eagle *m_eagle;
    int m_current_level;
    int m_player_count;
    int m_enemy_to_kill;
    bool m_level_start_screen;
    bool m_protect_eagle;
    Uint32 m_level_start_time;
    Uint32 m_enemy_redy_time;
    Uint32 m_level_end_time;
    Uint32 m_protect_eagle_time;

    bool m_game_over;
    double m_game_over_position;
    bool m_finished;
    bool m_pause;
    int m_enemy_respown_position;

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