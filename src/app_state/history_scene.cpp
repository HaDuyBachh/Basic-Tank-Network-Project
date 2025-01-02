#include "history_scene.h"
#include "../engine/engine.h"
#include "../appconfig.h"
#include "menu.h"
#include <fstream>
#include <sstream>
#include <winsock2.h>
#include "settings.h"

HistoryScene::HistoryScene(const std::string &username)
{
    m_scroll_offset = 0;
    m_finished = false;

    // // Read history from file using same format as server
    // std::string filename = "scores/" + username + ".txt";
    // std::ifstream file(filename);

    // if (file.is_open())
    // {
    //     std::string line;
    //     while (std::getline(file, line))
    //     {
    //         m_history_lines.push_back(line);
    //     }
    //     file.close();
    // }

    HandleHistoryData(username);
}

void HistoryScene::HandleHistoryData(const std::string &username)
{
    // Request history from server
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8888);
    server_addr.sin_addr.s_addr = inet_addr(Settings::getServerIP().c_str());

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == 0)
    {
        // Format: get_history:username
        std::string request = "get_history:" + username;
        send(sock, request.c_str(), request.length(), 0);

        char buffer[4096] = {0};
        recv(sock, buffer, sizeof(buffer), 0);
        std::string response(buffer);

        // Parse response line by line
        std::stringstream ss(response);
        std::string line;
        while (std::getline(ss, line, ';'))
        {
            if (!line.empty())
            {
                m_history_lines.push_back(line);
            }
        }
    }

    closesocket(sock);
    WSACleanup();
}

HistoryScene::~HistoryScene() {}

void HistoryScene::draw()
{
    Renderer *renderer = Engine::getEngine().getRenderer();
    renderer->clear();

    renderer->drawRect(&AppConfig::map_rect, {0, 0, 0, 255}, true);

    // Draw title
    SDL_Point title_pos = {120, 40};
    renderer->drawText(&title_pos, "Match History", {255, 255, 0, 255}, 2);

    // Draw scroll instructions
    SDL_Point instr_pos = {20, 60};
    renderer->drawText(&instr_pos, "UP/DOWN to scroll, ESC to return",
                       {200, 200, 200, 255}, 3);

    // Draw history lines with formatting
    int y_pos = 100;
    int lines_per_page = 10;

    for (int i = m_scroll_offset;
         i < std::min((int)m_history_lines.size(), m_scroll_offset + lines_per_page);
         i++)
    {
        // Split line by '|' delimiter
        std::stringstream ss(m_history_lines[i]);
        std::string segment;
        int segment_y = y_pos;

        while (std::getline(ss, segment, '|'))
        {
            if (!segment.empty())
            {
                SDL_Point text_pos = {5, segment_y};
                renderer->drawText(&text_pos, segment, {255, 255, 255, 255}, 3);
                segment_y += 20; // Space between segments
            }
        }

        // Add separator line
        SDL_Point sep_pos = {5, segment_y};
        renderer->drawText(&sep_pos, "-----------------------------------",
                           {200, 200, 200, 255}, 3);

        y_pos = segment_y + 30; // Space between matches
    }

    renderer->flush();
}

void HistoryScene::update(Uint32 dt) {}

void HistoryScene::eventProcess(SDL_Event *ev)
{
    if (ev->type == SDL_KEYDOWN)
    {
        switch (ev->key.keysym.sym)
        {
        case SDLK_UP:
            if (m_scroll_offset > 0)
                m_scroll_offset--;
            break;

        case SDLK_DOWN:
            if (m_scroll_offset < (int)m_history_lines.size() - 10)
                m_scroll_offset++;
            break;

        case SDLK_ESCAPE:
            m_finished = true;
            break;
        }
    }
}

bool HistoryScene::finished() const
{
    return m_finished;
}

AppState *HistoryScene::nextState()
{
    return new Menu();
}