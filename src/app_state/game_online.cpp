#include "game_online.h"
#include "../objects/eagle.h"
#include "../appconfig.h"
#include <sstream>

GameOnline::GameOnline(const std::string& room_code, bool is_host, 
                      const std::vector<std::string>& players)
    : Game(), 
    m_room_code(room_code),
    m_is_host(is_host),
    m_last_sync_time(0) {
    
    m_level_columns_count = 0;
    m_level_rows_count = 0;
    m_current_level = 0;
    m_eagle = nullptr;
    m_player_count = players.size();
    m_enemy_redy_time = 0;
    m_pause = false;
    m_level_end_time = 0;
    m_protect_eagle = false;
    m_protect_eagle_time = 0;
    m_enemy_respown_position = 0;

        WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    m_sync_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    
    m_server_addr.sin_family = AF_INET;
    m_server_addr.sin_port = htons(8890);
    m_server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    m_last_sync_time = SDL_GetTicks();

    // Tạo players online
    if (m_is_host) {
        PlayerOnline* p1 = new PlayerOnline(AppConfig::player_starting_point.at(0).x,
                                          AppConfig::player_starting_point.at(0).y,
                                          ST_PLAYER_1, true, room_code);
        p1->player_keys = AppConfig::player_keys.at(0);
        m_players.push_back(p1);
    } else {
        PlayerOnline* p2 = new PlayerOnline(AppConfig::player_starting_point.at(1).x,
                                          AppConfig::player_starting_point.at(1).y,
                                          ST_PLAYER_2, false, room_code);
        p2->player_keys = AppConfig::player_keys.at(1);
        m_players.push_back(p2);
    }

    nextLevel();
}

GameOnline::~GameOnline() {
    closesocket(m_sync_socket);
    WSACleanup();
}

void GameOnline::syncGameState() {
    // Cập nhật game state
    m_game_state.enemies.clear();
    for (auto enemy : m_enemies) {
        GameState::EnemyData enemy_data;
        enemy_data.pos_x = enemy->pos_x;
        enemy_data.pos_y = enemy->pos_y;
        enemy_data.direction = enemy->direction;
        enemy_data.type = enemy->type;
        enemy_data.lives_count = enemy->lives_count;
        enemy_data.is_destroyed = enemy->testFlag(TSF_DESTROYED);
        m_game_state.enemies.push_back(enemy_data);
    }

    m_game_state.bonuses.clear();
    for (auto bonus : m_bonuses) {
        GameState::BonusData bonus_data;
        bonus_data.pos_x = bonus->pos_x;
        bonus_data.pos_y = bonus->pos_y;
        bonus_data.type = bonus->type;
        bonus_data.is_active = !bonus->to_erase;
        m_game_state.bonuses.push_back(bonus_data);
    }

    // m_game_state.eagle_destroyed = m_eagle->hasFlag(TSF_DESTROYED);
    m_game_state.current_level = m_current_level;
    m_game_state.enemy_count = m_enemy_to_kill;

    // Đóng gói và gửi game state
    std::stringstream ss;
    ss << "GAMESTATE:" << m_room_code << ":";
    
    // Đóng gói enemies
    ss << m_game_state.enemies.size() << ";";
    for (const auto& enemy : m_game_state.enemies) {
        ss << enemy.pos_x << "," << enemy.pos_y << "," 
           << enemy.direction << "," << enemy.type << ","
           << enemy.lives_count << "," << enemy.is_destroyed << ";";
    }
    
    // Đóng gói bonuses
    ss << m_game_state.bonuses.size() << ";";
    for (const auto& bonus : m_game_state.bonuses) {
        ss << bonus.pos_x << "," << bonus.pos_y << ","
           << bonus.type << "," << bonus.is_active << ";";
    }
    
    // Đóng gói thông tin khác
    ss << m_game_state.eagle_destroyed << ";"
       << m_game_state.current_level << ";"
       << m_game_state.enemy_count;

    std::string data = ss.str();
    sendto(m_sync_socket, data.c_str(), data.length(), 0,
           (struct sockaddr*)&m_server_addr, sizeof(m_server_addr));
}

void GameOnline::handleGameSync(const std::string& data) {
    // Kiểm tra và parse dữ liệu
    if (data.find("GAMESTATE:" + m_room_code + ":") != 0) {
        return;
    }

    std::stringstream ss(data.substr(data.find(":") + m_room_code.length() + 2));
    std::string item;
    
    // Parse enemies
    std::getline(ss, item, ';');
    int enemy_count = std::stoi(item);
    
    m_enemies.clear();
    for (int i = 0; i < enemy_count; i++) {
        std::getline(ss, item, ';');
        std::stringstream enemy_ss(item);
        std::string value;
        
        std::getline(enemy_ss, value, ',');
        double pos_x = std::stod(value);
        std::getline(enemy_ss, value, ',');
        double pos_y = std::stod(value);
        std::getline(enemy_ss, value, ',');
        int direction = std::stoi(value);
        std::getline(enemy_ss, value, ',');
        SpriteType type = static_cast<SpriteType>(std::stoi(value));
        std::getline(enemy_ss, value, ',');
        int lives = std::stoi(value);
        std::getline(enemy_ss, value, ',');
        bool is_destroyed = std::stoi(value);

        Enemy* enemy = new Enemy(pos_x, pos_y, type);
        enemy->direction = static_cast<Direction>(direction);
        enemy->lives_count = lives;
        if (is_destroyed) {
            enemy->setFlag(TSF_DESTROYED);
        }
        m_enemies.push_back(enemy);
    }

    // Parse bonuses
    std::getline(ss, item, ';');
    int bonus_count = std::stoi(item);
    
    m_bonuses.clear();
    for (int i = 0; i < bonus_count; i++) {
        std::getline(ss, item, ';');
        std::stringstream bonus_ss(item);
        std::string value;
        
        std::getline(bonus_ss, value, ',');
        double pos_x = std::stod(value);
        std::getline(bonus_ss, value, ',');
        double pos_y = std::stod(value);
        std::getline(bonus_ss, value, ',');
        SpriteType type = static_cast<SpriteType>(std::stoi(value));
        std::getline(bonus_ss, value, ',');
        bool is_active = std::stoi(value);

        Bonus* bonus = new Bonus(pos_x, pos_y, type);
        bonus->to_erase = !is_active;
        m_bonuses.push_back(bonus);
    }

    // // Parse eagle state
    // std::getline(ss, item, ';');
    // bool eagle_destroyed = std::stoi(item);
    // if (eagle_destroyed && !m_eagle->hasFlag(TSF_DESTROYED)) {
    //     m_eagle->destroy();
    // }

    // Parse level info
    std::getline(ss, item, ';');
    m_current_level = std::stoi(item);
    std::getline(ss, item, ';');
    m_enemy_to_kill = std::stoi(item);
}

void GameOnline::update(Uint32 dt) {
    Game::update(dt);
    
    Uint32 current_time = SDL_GetTicks();
    
    // Host đồng bộ game state định kỳ
    if (m_is_host && current_time - m_last_sync_time >= SYNC_INTERVAL) {
        syncGameState();
        m_last_sync_time = current_time;
    }
    
    // Client nhận game state
    if (!m_is_host) {
        char buffer[4096];
        struct sockaddr_in from_addr;
        int from_len = sizeof(from_addr);
        
        int recv_len = recvfrom(m_sync_socket, buffer, sizeof(buffer), 0,
                               (struct sockaddr*)&from_addr, &from_len);
        
        if (recv_len > 0) {
            buffer[recv_len] = '\0';
            std::string data(buffer);
            handleGameSync(data);
        }
    }    
}

