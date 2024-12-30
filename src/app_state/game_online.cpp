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

GameOnline::GameOnline(const std::string &room_code, bool is_host,
                       const std::vector<std::string> &players)
{
    m_room_code = room_code;
    m_is_host = is_host;
    m_last_sync_time = 0;
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
    if (m_is_host)
    {
        PlayerOnline *p1 = new PlayerOnline(AppConfig::player_starting_point.at(0).x,
                                            AppConfig::player_starting_point.at(0).y,
                                            ST_PLAYER_1, true, room_code);
        p1->player_keys = AppConfig::player_keys.at(0);
        m_players.push_back(p1);
    }
    else
    {
        PlayerOnline *p2 = new PlayerOnline(AppConfig::player_starting_point.at(1).x,
                                            AppConfig::player_starting_point.at(1).y,
                                            ST_PLAYER_2, false, room_code);
        p2->player_keys = AppConfig::player_keys.at(1);
        m_players.push_back(p2);
    }

    nextLevel();
}

GameOnline::~GameOnline()
{
    closesocket(m_sync_socket);
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

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == 0) {
        // std::string request = "game_data: host is " + (this->m_is_host ? "true" : "false");
        
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
    else
    if (m_is_host)
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
            player->update(dt);
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
    }
    checkConnect();
}
