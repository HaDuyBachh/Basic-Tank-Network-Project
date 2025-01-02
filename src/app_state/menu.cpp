#include "menu.h"
#include "../engine/engine.h"
#include "../engine/renderer.h"
#include "../appconfig.h"
#include "../type.h"
#include "../app_state/game.h"
#include "input_scene.h"
#include "game_online.h"
#include "signup_scene.h"
#include "signin_scene.h"
#include "room_scene.h"
#include "history_scene.h"

#include <iostream>

bool Menu::s_is_logged_in = false;
std::string Menu::s_logged_in_username = "";

void Menu::setLoggedInStatus(bool status, const std::string& username) {
    s_is_logged_in = status;
    s_logged_in_username = username;
}

bool Menu::isLoggedIn() {
    return s_is_logged_in;
}

std::string Menu::getLoggedInUsername() {
    return s_logged_in_username;
}

Menu::Menu()
{
    // Kiểm tra trạng thái đăng nhập để hiển thị menu phù hợp
    if (s_is_logged_in) {
        m_menu_texts.clear();
        m_menu_texts.push_back("Welcome " + s_logged_in_username);
        m_menu_texts.push_back("Create Room");
        m_menu_texts.push_back("Join Room");
        m_menu_texts.push_back("Match History");
        m_menu_texts.push_back("Single Player");
        m_menu_texts.push_back("Logout");
        m_menu_texts.push_back("Exit");
    } else {
        m_menu_texts.push_back("Sign In");
        m_menu_texts.push_back("Sign Up");
        m_menu_texts.push_back("1 Player Guest");
        m_menu_texts.push_back("2 Player Guest");
        m_menu_texts.push_back("Exit");
    }
    
    m_menu_index = 0;
    m_tank_pointer = new Player(0, 0, ST_PLAYER_1);
    m_tank_pointer->direction = D_RIGHT;    
    m_tank_pointer->pos_x = 120;
    m_tank_pointer->pos_y = (m_menu_index + 1) * 32 + 112;
    m_tank_pointer->setFlag(TSF_LIFE);
    m_tank_pointer->update(0);
    m_tank_pointer->clearFlag(TSF_LIFE);
    m_tank_pointer->clearFlag(TSF_SHIELD);
    m_tank_pointer->setFlag(TSF_MENU);
    m_finished = false;
}

Menu::~Menu()
{
    delete m_tank_pointer;
}

void Menu::draw()
{
    Renderer* renderer = Engine::getEngine().getRenderer();
    renderer->clear();

    renderer->drawRect(&AppConfig::map_rect, {0, 0, 0, 255}, true);
    renderer->drawRect(&AppConfig::status_rect, {0, 0, 0, 255}, true);

    // LOGO
    const SpriteData* logo = Engine::getEngine().getSpriteConfig()->getSpriteData(ST_TANKS_LOGO);
    SDL_Rect dst = {(AppConfig::map_rect.w + AppConfig::status_rect.w - logo->rect.w)/2, 50, logo->rect.w, logo->rect.h};
    renderer->drawObject(&logo->rect, &dst);

    int i = 0;
    SDL_Point text_start;
    
    // Hiển thị tất cả menu items, kể cả khi đã đăng nhập
    for(auto text : m_menu_texts) {
        text_start = {160, (i + 1) * 32 + 120};
        
        // Nếu là welcome message thì đổi màu
        if (s_is_logged_in && i == 0) {
            renderer->drawText(&text_start, text, {0, 255, 0, 255}, 2);
        } else {
            renderer->drawText(&text_start, text, {255, 255, 255, 255}, 2);
        }
        i++;
    }

    m_tank_pointer->draw();
    renderer->flush();
}

void Menu::update(Uint32 dt)
{
    m_tank_pointer->speed = m_tank_pointer->default_speed;
    m_tank_pointer->stop = true;
    m_tank_pointer->update(dt);
}

void Menu::eventProcess(SDL_Event *ev)
{
    if(ev->type == SDL_KEYDOWN)
    {
        if(ev->key.keysym.sym == SDLK_UP)
        {
            m_menu_index--;
            if(m_menu_index < 0)
                m_menu_index = m_menu_texts.size() - 1;

            m_tank_pointer->pos_y = (m_menu_index + 1) * 32 + 110;
        }
        else if(ev->key.keysym.sym == SDLK_DOWN)
        {
            m_menu_index++;
            if(m_menu_index >= m_menu_texts.size())
                m_menu_index = 0;

            m_tank_pointer->pos_y = (m_menu_index + 1) * 32 + 110;
        }
        else if(ev->key.keysym.sym == SDLK_SPACE || ev->key.keysym.sym == SDLK_RETURN)
        {
            m_finished = true;
        }
        else if(ev->key.keysym.sym == SDLK_ESCAPE)
        {
            m_menu_index = -1;
            m_finished = true;
        }
    }
}

bool Menu::finished() const
{
    return m_finished;
}

AppState* Menu::nextState() {
    if (s_is_logged_in) {
        // Menu cho người dùng đã đăng nhập
        switch(m_menu_index) {
            case 0:
                return nullptr;
            case 1: // Create Room
                return new RoomScene(true,s_logged_in_username);
            case 2: // Join Room
                return new RoomScene(false,s_logged_in_username); 
            case 3: // Match History
                return new HistoryScene(s_logged_in_username);   
            case 4:// Join Room
                return new Game(1);
            case 5: // Logout
                s_is_logged_in = false;
                s_logged_in_username = "";
                return new Menu();
            case 6: // Exit
                return nullptr;
        }
    } else {
        // Menu cho người dùng chưa đăng nhập
        switch(m_menu_index) {
            case 0: // Sign In
                return new SigninScene();
            case 1: // Sign Up
                return new SignupScene();
            case 2: // Play as Guest
                return new Game(1);
            case 3: // Exit
                return new Game(2);
            default:
                return nullptr;
        }
    }
    return nullptr;
}
