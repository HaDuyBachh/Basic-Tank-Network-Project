#include "game_online.h"
#include "../objects/eagle.h"
#include "../appconfig.h"
#include "menu.h"
#include <sstream>
#include <fstream>

#include <SDL2/SDL.h>
#include <stdlib.h>
#include <ctime>
#include <fstream>
#include <algorithm>
#include <iostream>
#include <cmath>
#include <windows.h>

GameOnline::GameOnline(const std::string &room_code, bool is_host, const std::vector<std::string> &players) : Game(2)
{
    m_room_code = room_code;
    m_is_host = is_host;
    m_last_sync_time = 0;
    m_level_columns_count = 0;
    m_level_rows_count = 0;
    m_current_level = 0;
    m_eagle = nullptr;
    m_player_count = 2;
    m_enemy_redy_time = 0;
    m_pause = false;
    m_level_end_time = 0;
    m_protect_eagle = false;
    m_protect_eagle_time = 0;
    m_enemy_respown_position = 0;

    nextLevel();

    if (!is_host)
    {
        AllocConsole();
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
        std::cout << "START DEBUG \n";
    }
}

GameOnline::~GameOnline()
{
}

GameOnline::GameSnapshot GameOnline::captureGameState()
{
    GameSnapshot snapshot;

    // Capture players state
    for (auto player : m_players)
    {
        PlayerSnapshot p_snapshot;
        p_snapshot.pos_x = player->pos_x;
        p_snapshot.pos_y = player->pos_y;
        p_snapshot.direction = player->direction;
        p_snapshot.is_firing = player->m_fire_time > AppConfig::player_reload_time;
        p_snapshot.is_destroyed = player->testFlag(TSF_DESTROYED);
        p_snapshot.lives_count = player->lives_count;
        p_snapshot.star_count = player->star_count;

        // Capture player's bullets
        for (auto bullet : player->bullets)
        {
            BulletSnapshot b_snapshot;
            b_snapshot.pos_x = bullet->pos_x;
            b_snapshot.pos_y = bullet->pos_y;
            b_snapshot.direction = bullet->direction;
            b_snapshot.collide = bullet->collide;
            b_snapshot.increased_damage = bullet->increased_damage;
            p_snapshot.bullets.push_back(b_snapshot);
        }

        snapshot.players.push_back(p_snapshot);
    }

    // Capture enemies state
    for (auto enemy : m_enemies)
    {
        EnemySnapshot e_snapshot;
        e_snapshot.pos_x = enemy->pos_x;
        e_snapshot.pos_y = enemy->pos_y;
        e_snapshot.direction = enemy->direction;
        e_snapshot.type = enemy->type;
        e_snapshot.lives_count = enemy->lives_count;
        e_snapshot.is_destroyed = enemy->testFlag(TSF_DESTROYED);
        e_snapshot.has_bonus = enemy->testFlag(TSF_BONUS);

        // Capture enemy's bullets
        for (auto bullet : enemy->bullets)
        {
            BulletSnapshot b_snapshot;
            b_snapshot.pos_x = bullet->pos_x;
            b_snapshot.pos_y = bullet->pos_y;
            b_snapshot.direction = bullet->direction;
            b_snapshot.collide = bullet->collide;
            b_snapshot.increased_damage = bullet->increased_damage;
            e_snapshot.bullets.push_back(b_snapshot);
        }

        snapshot.enemies.push_back(e_snapshot);
    }

    // Capture bonuses
    for (auto bonus : m_bonuses)
    {
        BonusData b_data;
        b_data.pos_x = bonus->pos_x;
        b_data.pos_y = bonus->pos_y;
        b_data.type = bonus->type;
        b_data.is_active = !bonus->to_erase;
        snapshot.bonuses.push_back(b_data);
    }

    // Capture game state
    snapshot.eagle_destroyed = m_eagle->type == ST_DESTROY_EAGLE;
    snapshot.current_level = m_current_level;
    snapshot.game_over = m_game_over;
    snapshot.game_over_position = m_game_over_position;

    return snapshot;
}

std::string GameOnline::GameStateSendData()
{
    GameSnapshot snapshot = captureGameState();
    std::stringstream ss;

    ss << "game_state:" << m_room_code << ":"
       << snapshot.current_level << ","
       << snapshot.game_over << ","
       << snapshot.game_over_position << ","
       << snapshot.eagle_destroyed;

    // Serialize players
    ss << ":players:";
    for (auto &p : snapshot.players)
    {
        ss << p.pos_x << "," << p.pos_y << ","
           << p.direction << "," << p.is_firing << ","
           << p.is_destroyed << "," << p.lives_count << ","
           << p.star_count << ";";
        // Serialize player bullets
        for (auto &b : p.bullets)
        {
            ss << b.pos_x << "," << b.pos_y << ","
               << b.direction << "," << b.collide << ","
               << b.increased_damage << "|";
        }
        ss << "#";
    }

    // Serialize enemies
    ss << ":enemies:";
    for (auto &e : snapshot.enemies)
    {
        ss << e.pos_x << "," << e.pos_y << ","
           << e.direction << "," << e.type << ","
           << e.lives_count << "," << e.is_destroyed << ","
           << e.has_bonus << ";";

        // Enemy bullets
        for (auto &b : e.bullets)
        {
            ss << b.pos_x << "," << b.pos_y << ","
               << b.direction << "," << b.collide << ","
               << b.increased_damage << "|";
        }
        ss << "#";
    }

    // Serialize bonuses
    ss << ":bonuses:";
    for (auto &b : snapshot.bonuses)
    {
        ss << b.pos_x << "," << b.pos_y << ","
           << b.type << "," << b.is_active << "#";
    }

    std::string data = ss.str();
    std::cout << "Send game state: " << data.length() << std::endl;

    // Debug print game state values ----------------------------------
    std::cout << "\n=== Game State Debug ===\n";
    std::cout << "Level: " << snapshot.current_level << std::endl;
    std::cout << "Game over: " << snapshot.game_over << std::endl;
    std::cout << "Position: " << snapshot.game_over_position << std::endl;
    std::cout << "Eagle: " << snapshot.eagle_destroyed << std::endl;

    // Format header
    ss << "game_state:" << m_room_code << ":"
       << snapshot.current_level << ","
       << (snapshot.game_over ? "1" : "0") << ","                                  // Convert bool to string
       << std::fixed << std::setprecision(6) << snapshot.game_over_position << "," // Fix precision
       << (snapshot.eagle_destroyed ? "1" : "0");

    // Debug print players count
    std::cout << "Players count: " << snapshot.players.size() << std::endl;

    // Serialize players with verification
    ss << ":players:";
    for (auto &p : snapshot.players)
    {
        if (std::isnan(p.pos_x) || std::isnan(p.pos_y))
        {
            std::cout << "Invalid player position detected!" << std::endl;
            continue;
        }
        ss << std::fixed << std::setprecision(6)
           << p.pos_x << "," << p.pos_y << ","
           << p.direction << ","
           << (p.is_firing ? "1" : "0") << ","
           << (p.is_destroyed ? "1" : "0") << ","
           << p.lives_count << ","
           << p.star_count << ";";

        // Debug print bullets count
        std::cout << "Player bullets: " << p.bullets.size() << std::endl;

        for (auto &b : p.bullets)
        {
            if (std::isnan(b.pos_x) || std::isnan(b.pos_y))
            {
                std::cout << "Invalid bullet position detected!" << std::endl;
                continue;
            }
            ss << std::fixed << std::setprecision(6)
               << b.pos_x << "," << b.pos_y << ","
               << b.direction << ","
               << (b.collide ? "1" : "0") << ","
               << (b.increased_damage ? "1" : "0") << "|";
        }
        ss << "#";
    }

    // Similar debug for enemies and bonuses
    std::cout << "Enemies count: " << snapshot.enemies.size() << std::endl;
    std::cout << "Bonuses count: " << snapshot.bonuses.size() << std::endl;

    // ...rest of serialization code...

    std::string data = ss.str();
    std::cout << "Final data length: " << data.length() << std::endl;
    std::cout << "First 100 chars: " << data.substr(0, 100) << "...\n";

    return data;
}

void GameOnline::RecvGameState(const std::string &data)
{
    // Clear old objects not in new state
    for (auto enemy : m_enemies)
    {
        delete enemy;
    }
    m_enemies.clear();

    for (auto bonus : m_bonuses)
    {
        delete bonus;
    }
    m_bonuses.clear();

    std::stringstream ss(data);
    std::string header, room_code, game_data, players_data;

    // Parse header: "game_state:room_code:level,game_over,pos,eagle:players:"
    std::getline(ss, header, ':');
    if (header != "game_state")
        return;

    std::getline(ss, room_code, ':');
    if (room_code != m_room_code)
        return;

    // Parse game state
    std::getline(ss, game_data, ':');
    std::stringstream game_ss(game_data);
    std::string value;

    std::vector<std::string> game_values;
    while (std::getline(game_ss, value, ','))
    {
        game_values.push_back(value);
    }

    if (game_values.size() >= 4)
    {
        m_current_level = std::stoi(game_values[0]);
        m_game_over = game_values[1] == "1";
        m_game_over_position = std::stod(game_values[2]);
        bool eagle_destroyed = game_values[3] == "1";
        if (eagle_destroyed)
        {
            m_eagle->destroy();
        }
    }

    // Skip "players:" marker
    std::getline(ss, header, ':');

    // Parse players data
    while (std::getline(ss, players_data, '#'))
    {
        if (players_data.empty())
            continue;

        // Split player data and bullets
        size_t semi_pos = players_data.find(';');
        if (semi_pos == std::string::npos)
            continue;

        std::string player_data = players_data.substr(0, semi_pos);
        std::string bullets_data = players_data.substr(semi_pos + 1);

        // Parse player attributes
        std::stringstream player_ss(player_data);
        std::vector<std::string> values;

        while (std::getline(player_ss, value, ','))
        {
            values.push_back(value);
        }

        if (values.size() >= 7 && !m_players.empty())
        {
            Player *player = m_players[0]; // Update host player
            player->pos_x = std::stod(values[0]);
            player->pos_y = std::stod(values[1]);
            player->direction = static_cast<Direction>(std::stoi(values[2]));
            player->m_fire_time = (values[3] == "1") ? AppConfig::player_reload_time + 1 : 0;
            if (values[4] == "1")
            {
                player->setFlag(TSF_DESTROYED);
            }
            else
            {
                player->clearFlag(TSF_DESTROYED);
            }
            player->lives_count = std::stoi(values[5]);
            player->star_count = std::stoi(values[6]);

            // Parse bullets
            player->bullets.clear();
            std::stringstream bullets_ss(bullets_data);
            std::string bullet_data;

            while (std::getline(bullets_ss, bullet_data, '|'))
            {
                if (bullet_data.empty())
                    continue;

                std::stringstream bullet_ss(bullet_data);
                std::vector<std::string> bullet_values;

                while (std::getline(bullet_ss, value, ','))
                {
                    bullet_values.push_back(value);
                }

                if (bullet_values.size() >= 5)
                {
                    Bullet *bullet = new Bullet(
                        std::stod(bullet_values[0]),
                        std::stod(bullet_values[1]));
                    bullet->direction = static_cast<Direction>(std::stoi(bullet_values[2]));
                    bullet->collide = bullet_values[3] == "1";
                    bullet->increased_damage = bullet_values[4] == "1";
                    player->bullets.push_back(bullet);
                }
            }
        }
    }

    // Parse enemies data
    std::getline(ss, header, ':'); // Skip "enemies:" marker
    std::string enemies_data;
    while (std::getline(ss, enemies_data, '#'))
    {
        if (enemies_data.empty())
            continue;

        // Split enemy data and bullets
        size_t semi_pos = enemies_data.find(';');
        if (semi_pos == std::string::npos)
            continue;

        std::string enemy_data = enemies_data.substr(0, semi_pos);
        std::string bullets_data = enemies_data.substr(semi_pos + 1);

        std::stringstream enemy_ss(enemy_data);
        std::vector<std::string> values;
        while (std::getline(enemy_ss, value, ','))
        {
            values.push_back(value);
        }

        if (values.size() >= 7)
        {
            Enemy *enemy = new Enemy(
                std::stod(values[0]),
                std::stod(values[1]),
                static_cast<SpriteType>(std::stoi(values[3])));
            enemy->direction = static_cast<Direction>(std::stoi(values[2]));
            enemy->lives_count = std::stoi(values[4]);
            if (values[5] == "1")
                enemy->setFlag(TSF_DESTROYED);
            if (values[6] == "1")
                enemy->setFlag(TSF_BONUS);

            // Parse enemy bullets
            std::stringstream bullets_ss(bullets_data);
            std::string bullet_data;
            while (std::getline(bullets_ss, bullet_data, '|'))
            {
                if (bullet_data.empty())
                    continue;

                std::stringstream bullet_ss(bullet_data);
                std::vector<std::string> bullet_values;
                while (std::getline(bullet_ss, value, ','))
                {
                    bullet_values.push_back(value);
                }

                if (bullet_values.size() >= 5)
                {
                    Bullet *bullet = new Bullet(
                        std::stod(bullet_values[0]), // pos_x
                        std::stod(bullet_values[1])  // pos_y
                    );
                    bullet->direction = static_cast<Direction>(std::stoi(bullet_values[2]));
                    bullet->collide = bullet_values[3] == "1";
                    bullet->increased_damage = bullet_values[4] == "1";
                    enemy->bullets.push_back(bullet);
                }
            }
            m_enemies.push_back(enemy);
        }
    }

    // Parse bonuses data
    std::getline(ss, header, ':'); // Skip "bonuses:" marker
    std::string bonus_data;
    while (std::getline(ss, bonus_data, '#'))
    {
        if (bonus_data.empty())
            continue;

        std::stringstream bonus_ss(bonus_data);
        std::vector<std::string> values;
        while (std::getline(bonus_ss, value, ','))
        {
            values.push_back(value);
        }

        if (values.size() >= 4)
        {
            Bonus *bonus = new Bonus(
                std::stod(values[0]),
                std::stod(values[1]),
                static_cast<SpriteType>(std::stoi(values[2])));
            bonus->to_erase = !(values[3] == "1");
            m_bonuses.push_back(bonus);
        }
    }
}

void GameOnline::HandleClientData()
{
    // Khởi tạo Winsock
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    // Tạo socket TCP
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    // Cấu hình địa chỉ server
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8888);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Lấy trạng thái input
    const Uint8 *keystate = SDL_GetKeyboardState(NULL);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == 0)
    {
        std::stringstream ss;
        ss << "client_data:" << m_room_code << ":";
        ss << (keystate[SDL_SCANCODE_UP] ? "1" : "0") << ",";
        ss << (keystate[SDL_SCANCODE_DOWN] ? "1" : "0") << ",";
        ss << (keystate[SDL_SCANCODE_LEFT] ? "1" : "0") << ",";
        ss << (keystate[SDL_SCANCODE_RIGHT] ? "1" : "0") << ",";
        ss << (keystate[SDL_SCANCODE_RCTRL] ? "1" : "0");

        std::string data = ss.str();
        send(sock, data.c_str(), data.length(), 0);

        // Nhận dữ liệu game state từ server
        char buffer[2000] = {0};
        if (recv(sock, buffer, sizeof(buffer), 0) > 0)
        {
            std::string response(buffer);
            if (response.find("game_state:") == 0)
            {
                std::cout << "Response: |" << response.length() << std::endl;
                RecvGameState(response);
            }
        }
    }

    closesocket(sock);
    WSACleanup();
}

void GameOnline::HandleHostData()
{
    // Khởi tạo Winsock
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    // Tạo socket TCP
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    // Cấu hình địa chỉ server
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8888);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Kết nối đến server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == 0)
    {
        // Đóng gói dữ liệu player
        std::stringstream ss;

        std::string data = GameStateSendData();

        // Gửi dữ liệu
        send(sock, data.c_str(), data.length(), 0);

        // Nhận phản hồi từ server
        char buffer[1024] = {0};
        if (recv(sock, buffer, sizeof(buffer), 0) > 0)
        {
            std::string response(buffer);
            std::stringstream ss(response);

            // std::cout << "Raw: |" <<response<< std::endl;

            std::string header;
            std::getline(ss, header, ':');

            // std::cout << "Raw response: |" << header << "|" << response << std::endl;

            if (header == "host_response")
            {
                std::string input_data;
                std::getline(ss, input_data);
                std::vector<bool> inputs;
                std::stringstream input_ss(input_data);
                std::string value;

                while (std::getline(input_ss, value, ','))
                {
                    inputs.push_back(value == "1");
                }

                if (inputs.size() >= 5)
                {
                    m_client_input.up = inputs[0];
                    m_client_input.down = inputs[1];
                    m_client_input.left = inputs[2];
                    m_client_input.right = inputs[3];
                    m_client_input.fire = inputs[4];

                    // std::cout << "Parsed inputs: "
                    //  << inputs[0] << ","
                    //  << inputs[1] << ","
                    //  << inputs[2] << ","
                    //  << inputs[3] << ","
                    //  << inputs[4] << std::endl;
                }
            }
        }
    }

    // Đóng socket và cleanup
    closesocket(sock);
    WSACleanup();
}

void GameOnline::checkConnect()
{
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8888);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == 0)
    {
        std::stringstream ss;
        ss << "game_data:" << m_room_code << ":"
           << (m_is_host ? "host" : "client") << ":"
           << m_players.size();
        std::string request = ss.str();

        send(sock, request.c_str(), request.length(), 0);

        char buffer[1024] = {0};
        recv(sock, buffer, sizeof(buffer), 0);
        std::string response(buffer);
    }

    closesocket(sock);
    WSACleanup();
}

void GameOnline::ClientUpdate()
{
}

void GameOnline::HostUpdate(Uint32 dt)
{
    if (m_pause)
        return;

    updateCollider(dt);

    // ấn định mục tiêu của đối thủ
    int min_metric; // 2 * 26 * 16
    int metric;
    SDL_Point target;
    for (auto enemy : m_enemies)
    {
        min_metric = 832;
        if (enemy->type == ST_TANK_A || enemy->type == ST_TANK_D)
            for (auto player : m_players)
            {
                metric = fabs(player->dest_rect.x - enemy->dest_rect.x) + fabs(player->dest_rect.y - enemy->dest_rect.y);
                if (metric < min_metric)
                {
                    min_metric = metric;
                    target = {player->dest_rect.x + player->dest_rect.w / 2, player->dest_rect.y + player->dest_rect.h / 2};
                }
            }
        metric = fabs(m_eagle->dest_rect.x - enemy->dest_rect.x) + fabs(m_eagle->dest_rect.y - enemy->dest_rect.y);
        if (metric < min_metric)
        {
            min_metric = metric;
            target = {m_eagle->dest_rect.x + m_eagle->dest_rect.w / 2, m_eagle->dest_rect.y + m_eagle->dest_rect.h / 2};
        }

        enemy->target_position = target;
    }

    // Cập nhật tất cả các đối tượng
    for (auto enemy : m_enemies)
        enemy->update(dt);

    for (auto player : m_players)
    {
        if (player->player_keys == AppConfig::player_keys.at(1))
            player->updateWithInput(dt, m_client_input.up, m_client_input.down, m_client_input.left, m_client_input.right, m_client_input.fire);
        else
            player->update(dt);
    }

    for (auto bonus : m_bonuses)
        bonus->update(dt);
    m_eagle->update(dt);

    for (auto row : m_level)
        for (auto item : row)
            if (item != nullptr)
                item->update(dt);

    for (auto bush : m_bushes)
        bush->update(dt);

    // loại bỏ các phần tử không cần thiết
    m_enemies.erase(std::remove_if(m_enemies.begin(), m_enemies.end(), [](Enemy *e)
                                   {if(e->to_erase) {delete e; return true;} return false; }),
                    m_enemies.end());
    m_players.erase(std::remove_if(m_players.begin(), m_players.end(), [this](Player *p)
                                   {if(p->to_erase) {m_killed_players.push_back(p); return true;} return false; }),
                    m_players.end());
    m_bonuses.erase(std::remove_if(m_bonuses.begin(), m_bonuses.end(), [](Bonus *b)
                                   {if(b->to_erase) {delete b; return true;} return false; }),
                    m_bonuses.end());
    m_bushes.erase(std::remove_if(m_bushes.begin(), m_bushes.end(), [](Object *b)
                                  {if(b->to_erase) {delete b; return true;} return false; }),
                   m_bushes.end());

    // thêm đối thủ mới
    m_enemy_redy_time += dt;
    if (static_cast<int>(m_enemies.size()) < (AppConfig::enemy_max_count_on_map < m_enemy_to_kill ? AppConfig::enemy_max_count_on_map : m_enemy_to_kill) && m_enemy_redy_time > AppConfig::enemy_redy_time)
    {
        m_enemy_redy_time = 0;
        generateEnemy();
    }

    if (m_enemies.empty() && m_enemy_to_kill <= 0)
    {
        m_level_end_time += dt;
        if (m_level_end_time > AppConfig::level_end_time)
            m_finished = true;
    }

    if (m_players.empty() && !m_game_over)
    {
        m_eagle->destroy();
        m_game_over_position = AppConfig::map_rect.h;
        m_game_over = true;
    }

    if (m_game_over)
    {
        if (m_game_over_position < 10)
            m_finished = true;
        else
            m_game_over_position -= AppConfig::game_over_entry_speed * dt;
    }

    if (m_protect_eagle)
    {
        m_protect_eagle_time += dt;
        if (m_protect_eagle_time > AppConfig::protect_eagle_time)
        {
            m_protect_eagle = false;
            m_protect_eagle_time = 0;
            for (int i = 0; i < 3; i++)
            {
                if (m_level.at(m_level_rows_count - i - 1).at(11) != nullptr)
                    delete m_level.at(m_level_rows_count - i - 1).at(11);
                m_level.at(m_level_rows_count - i - 1).at(11) = new Brick(11 * AppConfig::tile_rect.w, (m_level_rows_count - i - 1) * AppConfig::tile_rect.h);

                if (m_level.at(m_level_rows_count - i - 1).at(14) != nullptr)
                    delete m_level.at(m_level_rows_count - i - 1).at(14);
                m_level.at(m_level_rows_count - i - 1).at(14) = new Brick(14 * AppConfig::tile_rect.w, (m_level_rows_count - i - 1) * AppConfig::tile_rect.h);
            }
            for (int i = 12; i < 14; i++)
            {
                if (m_level.at(m_level_rows_count - 3).at(i) != nullptr)
                    delete m_level.at(m_level_rows_count - 3).at(i);
                m_level.at(m_level_rows_count - 3).at(i) = new Brick(i * AppConfig::tile_rect.w, (m_level_rows_count - 3) * AppConfig::tile_rect.h);
            }
        }

        if (m_protect_eagle && m_protect_eagle_time > AppConfig::protect_eagle_time / 4 * 3 && m_protect_eagle_time / AppConfig::bonus_blink_time % 2)
        {
            for (int i = 0; i < 3; i++)
            {
                if (m_level.at(m_level_rows_count - i - 1).at(11) != nullptr)
                    delete m_level.at(m_level_rows_count - i - 1).at(11);
                m_level.at(m_level_rows_count - i - 1).at(11) = new Brick(11 * AppConfig::tile_rect.w, (m_level_rows_count - i - 1) * AppConfig::tile_rect.h);

                if (m_level.at(m_level_rows_count - i - 1).at(14) != nullptr)
                    delete m_level.at(m_level_rows_count - i - 1).at(14);
                m_level.at(m_level_rows_count - i - 1).at(14) = new Brick(14 * AppConfig::tile_rect.w, (m_level_rows_count - i - 1) * AppConfig::tile_rect.h);
            }
            for (int i = 12; i < 14; i++)
            {
                if (m_level.at(m_level_rows_count - 3).at(i) != nullptr)
                    delete m_level.at(m_level_rows_count - 3).at(i);
                m_level.at(m_level_rows_count - 3).at(i) = new Brick(i * AppConfig::tile_rect.w, (m_level_rows_count - 3) * AppConfig::tile_rect.h);
            }
        }
        else if (m_protect_eagle)
        {
            for (int i = 0; i < 3; i++)
            {
                if (m_level.at(m_level_rows_count - i - 1).at(11) != nullptr)
                    delete m_level.at(m_level_rows_count - i - 1).at(11);
                m_level.at(m_level_rows_count - i - 1).at(11) = new Object(11 * AppConfig::tile_rect.w, (m_level_rows_count - i - 1) * AppConfig::tile_rect.h, ST_STONE_WALL);

                if (m_level.at(m_level_rows_count - i - 1).at(14) != nullptr)
                    delete m_level.at(m_level_rows_count - i - 1).at(14);
                m_level.at(m_level_rows_count - i - 1).at(14) = new Object(14 * AppConfig::tile_rect.w, (m_level_rows_count - i - 1) * AppConfig::tile_rect.h, ST_STONE_WALL);
            }
            for (int i = 12; i < 14; i++)
            {
                if (m_level.at(m_level_rows_count - 3).at(i) != nullptr)
                    delete m_level.at(m_level_rows_count - 3).at(i);
                m_level.at(m_level_rows_count - 3).at(i) = new Object(i * AppConfig::tile_rect.w, (m_level_rows_count - 3) * AppConfig::tile_rect.h, ST_STONE_WALL);
            }
        }
    }
}

void GameOnline::update(Uint32 dt)
{
    if (dt > 40)
        return;

    if (m_level_start_screen)
    {
        if (m_level_start_time > AppConfig::level_start_time)
            m_level_start_screen = false;

        m_level_start_time += dt;
    }
    else if (m_is_host)
    {
        HostUpdate(dt);
        HandleHostData();
    }
    else if (!m_is_host)
    {
        //HandleClientData();
    }
}
