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

GameOnline::GameOnline(const std::string &room_code, bool is_host, const std::vector<std::string>& players) : Game(2)
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

    if (is_host)
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

void GameOnline::HandleClientData() {
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
    const Uint8* keystate = SDL_GetKeyboardState(NULL);
    
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == 0) {
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
        char buffer[1024] = {0};
        recv(sock, buffer, sizeof(buffer), 0);
        // TODO: Parse game state và update
    }

    closesocket(sock);
    WSACleanup();
}

void GameOnline::HandleHostData() {
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

        ss << "host_data:" << m_room_code << ":"
           << m_players[0]->pos_x << ","
           << m_players[0]->pos_y << ","
           << m_players[0]->direction << ","
           << (m_players[0]->m_fire_time > AppConfig::player_reload_time) << ","
           << m_players[0]->testFlag(TSF_DESTROYED);

        std::string data = ss.str();

        // Gửi dữ liệu
        send(sock, data.c_str(), data.length(), 0);

        // Nhận phản hồi từ server
        char buffer[1024] = {0};
        if(recv(sock, buffer, sizeof(buffer), 0) > 0) {
            std::string response(buffer);
            std::stringstream ss(response);
            
            // std::cout << "Raw: |" <<response<< std::endl;

            std::string header;
            std::getline(ss, header, ':');
            
            // std::cout << "Raw response: |" << header << "|" << response << std::endl;

            if(header == "host_response") {
                std::string input_data;
                std::getline(ss, input_data);
                std::vector<bool> inputs;
                std::stringstream input_ss(input_data);
                std::string value;
                
                while(std::getline(input_ss, value, ',')) {
                    inputs.push_back(value == "1");
                }
                
                if(inputs.size() >= 5) {
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

void GameOnline::checkConnect() {
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
        HandleClientData();
    }
}
