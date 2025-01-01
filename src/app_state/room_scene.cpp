// room_scene.cpp
#include "room_scene.h"
#include "menu.h"
#include "game_online.h"
#include <sstream>
#include <fstream>
#include <winsock2.h>
#include "../appconfig.h"

RoomScene::RoomScene(bool isHost, const std::string &username)
{
    m_selected_level = 2;
    m_room_code = "";
    m_current_room_code = "";
    m_finished = false;
    m_back_to_menu = false;
    m_room_joined = false;
    m_is_host = isHost;
    m_current_field = 0;
    m_username = username;
}

void RoomScene::renderLevelSelector(int yPos)
{
    Renderer *renderer = Engine::getEngine().getRenderer();

    // Hiển thị level hiện tại
    SDL_Point level_pos = {160, yPos};
    std::string level_text = "Level: " + std::to_string(m_selected_level);
    renderer->drawText(&level_pos, level_text, {255, 255, 255, 255}, 2);

    // Hiển thị controls nếu là host
    if (m_is_host)
    {
        SDL_Point controls_pos = {160, yPos + 30};
        renderer->drawText(&controls_pos, "Press UP/DOWN to change level",
                           {200, 200, 200, 255}, 3);
    }
}

RoomScene::~RoomScene()
{
    if (m_room_joined)
    {
        leaveRoom();
    }
}

void RoomScene::draw()
{
    Renderer *renderer = Engine::getEngine().getRenderer();
    renderer->clear();

    renderer->drawRect(&AppConfig::map_rect, {0, 0, 0, 255}, true);
    renderer->drawRect(&AppConfig::status_rect, {0, 0, 0, 255}, true);

    // Title
    SDL_Point text_pos = {160, 50};
    renderer->drawText(&text_pos, m_is_host ? "CREATE ROOM" : "JOIN ROOM",
                       {255, 255, 255, 255}, 2);

    if (!m_room_joined)
    {
        // Room code input
        renderInputField("Room Code:", m_room_code, 120, m_current_field == 0);

        // Create/Join button
        SDL_Point button_pos = {160, 200};
        SDL_Color button_color = m_current_field == 1 ? SDL_Color{255, 255, 0, 255} : SDL_Color{255, 255, 255, 255};
        renderer->drawText(&button_pos, m_is_host ? "CREATE" : "JOIN",
                           button_color, 2);
    }
    else
    {
        // Room info
        SDL_Point room_info_pos = {160, 120};
        renderer->drawText(&room_info_pos, "Room: " + m_current_room_code,
                           {255, 255, 255, 255}, 2);

        // Player list
        int y_pos = 160;
        for (const auto &player : m_players_in_room)
        {
            SDL_Point player_pos = {160, y_pos};
            renderer->drawText(&player_pos, player, {255, 255, 255, 255}, 2);
            y_pos += 30;
        }

        // level selector trước nút Start
        renderLevelSelector(220);

        if (m_is_host)
        {
            SDL_Point start_pos = {160, 280}; // Đẩy nút Start xuống
            SDL_Color start_color = m_current_field == 1 ? SDL_Color{255, 255, 0, 255} : SDL_Color{255, 255, 255, 255};
            renderer->drawText(&start_pos, "START GAME", start_color, 2);
        }

        // Start game button (only for host)
        if (m_is_host)
        {
            SDL_Point start_pos = {160, 280};
            SDL_Color start_color = m_current_field == 1 ? SDL_Color{255, 255, 0, 255} : SDL_Color{255, 255, 255, 255};
            renderer->drawText(&start_pos, "START GAME", start_color, 2);
        }
    }

    // Back to menu text
    SDL_Point back_pos = {20, 320};
    renderer->drawText(&back_pos, "Back to Menu (Press ESC)",
                       {255, 255, 255, 255}, 2);

    renderer->flush();
}

void RoomScene::renderInputField(const std::string &label, const std::string &content,
                                 int yPos, bool isSelected)
{
    Renderer *renderer = Engine::getEngine().getRenderer();

    SDL_Point label_pos = {20, yPos};
    SDL_Color text_color = isSelected ? SDL_Color{255, 255, 0, 255} : SDL_Color{255, 255, 255, 255};
    renderer->drawText(&label_pos, label, text_color, 2);

    SDL_Point content_pos = {180, yPos};
    renderer->drawText(&content_pos, content + (isSelected ? "_" : ""),
                       text_color, 2);
}

bool RoomScene::checkGameStarted()
{
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8888);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    bool game_started = false;

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == 0)
    {
        std::string request = "check_game:" + m_current_room_code;
        send(sock, request.c_str(), request.length(), 0);

        char buffer[1024] = {0};
        recv(sock, buffer, sizeof(buffer), 0);
        std::string response(buffer);

        game_started = (response == "STARTED");
    }

    closesocket(sock);
    WSACleanup();
    return game_started;
}

void RoomScene::startGame()
{
    if (!m_is_host)
        return;

    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8888);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == 0)
    {
        std::string request = "start_game:" + m_current_room_code;
        send(sock, request.c_str(), request.length(), 0);

        char buffer[1024] = {0};
        recv(sock, buffer, sizeof(buffer), 0);
    }

    closesocket(sock);
    WSACleanup();
}

bool RoomScene::createRoom(const std::string &room_code)
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
        std::string request = "create_room:" + room_code + ":" +
                              Menu::getLoggedInUsername();
        send(sock, request.c_str(), request.length(), 0);

        char buffer[1024] = {0};
        recv(sock, buffer, sizeof(buffer), 0);
        std::string response(buffer);

        closesocket(sock);
        WSACleanup();

        if (response == "OK")
        {
            m_current_room_code = room_code;
            m_room_joined = true;
            return true;
        }
    }

    closesocket(sock);
    WSACleanup();
    return false;
}

bool RoomScene::joinRoom(const std::string &room_code)
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
        std::string request = "join_room:" + room_code + ":" +
                              Menu::getLoggedInUsername();
        send(sock, request.c_str(), request.length(), 0);

        char buffer[1024] = {0};
        recv(sock, buffer, sizeof(buffer), 0);
        std::string response(buffer);

        closesocket(sock);
        WSACleanup();

        if (response == "OK")
        {
            m_current_room_code = room_code;
            m_room_joined = true;
            return true;
        }
    }

    closesocket(sock);
    WSACleanup();
    return false;
}

void RoomScene::leaveRoom()
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
        std::string request = "leave_room:" + m_current_room_code + ":" +
                              Menu::getLoggedInUsername();
        send(sock, request.c_str(), request.length(), 0);

        char buffer[1024] = {0};
        recv(sock, buffer, sizeof(buffer), 0);
    }

    closesocket(sock);
    WSACleanup();
}

void RoomScene::updatePlayerList()
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
        std::string request = "get_players:" + m_current_room_code;
        send(sock, request.c_str(), request.length(), 0);

        char buffer[1024] = {0};
        recv(sock, buffer, sizeof(buffer), 0);
        std::string response(buffer);

        m_players_in_room.clear();
        std::stringstream ss(response);
        std::string player;
        while (std::getline(ss, player, ','))
        {
            if (!player.empty())
            {
                m_players_in_room.push_back(player);
            }
        }
    }

    closesocket(sock);
    WSACleanup();
}

void RoomScene::sendLevelUpdate()
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
        std::string request = "update_level:" + m_current_room_code + ":" +
                              std::to_string(m_selected_level);
        send(sock, request.c_str(), request.length(), 0);
    }

    closesocket(sock);
    WSACleanup();
}

void RoomScene::checkLevelUpdate()
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
        std::string request = "get_level:" + m_current_room_code;
        send(sock, request.c_str(), request.length(), 0);

        char buffer[1024] = {0};
        recv(sock, buffer, sizeof(buffer), 0);
        std::string response(buffer);

        if (!response.empty())
        {
            try
            {
                if (response.find_first_not_of("0123456789") == std::string::npos)
                {
                    int new_level = std::stoi(response);
                    if (new_level >= 1 && new_level <= 35)
                    {
                        m_selected_level = new_level;
                    }
                }
                else
                {
                    std::cout << "Invalid level format received" << std::endl;
                }
            }
            catch (const std::exception &e)
            {
                std::cout << "Error parsing level: " << e.what() << std::endl;
            }
        }
    }

    closesocket(sock);
    WSACleanup();
}

void RoomScene::update(Uint32 dt)
{
    if (m_room_joined)
    {
        updatePlayerList();
        checkLevelUpdate();

        if (!m_is_host && checkGameStarted())
        {
            m_finished = true;
        }
    }
}

void RoomScene::eventProcess(SDL_Event *ev)
{
    if (ev->type == SDL_KEYDOWN)
    {
        if (m_room_joined && m_is_host)
        {
            switch (ev->key.keysym.sym)
            {
            case SDLK_UP:
                if (m_selected_level < 35)
                { // Max level
                    m_selected_level++;
                    sendLevelUpdate();
                }
                break;

            case SDLK_DOWN:
                if (m_selected_level > 1)
                {
                    m_selected_level--;
                    sendLevelUpdate();
                }
                break;
            }
        }
        switch (ev->key.keysym.sym)
        {
        case SDLK_ESCAPE:
            if (m_room_joined)
            {
                leaveRoom();
            }
            m_back_to_menu = true;
            m_finished = true;
            break;

        case SDLK_RETURN:
            if (!m_room_joined && m_current_field == 1)
            {
                if (m_is_host)
                {
                    createRoom(m_room_code);
                }
                else
                {
                    joinRoom(m_room_code);
                }
            }
            else if (m_room_joined && m_is_host && m_current_field == 1)
            {
                // Start game
                startGame();
                m_finished = true;
            }
            break;
        case SDLK_UP:
        case SDLK_DOWN:
            if (!m_room_joined)
            {
                m_current_field = (m_current_field + 1) % 2;
            }
            break;

        case SDLK_BACKSPACE:
            if (!m_room_joined && m_current_field == 0 && !m_room_code.empty())
            {
                m_room_code.pop_back();
            }
            break;
        }
    }
    else if (ev->type == SDL_TEXTINPUT && !m_room_joined && m_current_field == 0)
    {
        m_room_code += ev->text.text;
    }
}

bool RoomScene::finished() const
{
    return m_finished;
}

AppState *RoomScene::nextState()
{
    if (m_back_to_menu)
    {
        return new Menu();
    }
    if (m_room_joined && m_finished)
    {
        // Truyền level được chọn vào GameOnline
        m_selected_level -= 1;
        return new GameOnline(m_username, m_current_room_code, m_is_host,
                              m_players_in_room, m_selected_level);
    }
    return nullptr;
}