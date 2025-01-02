#include "ip_config_scene.h"
#include "../app_state/settings.h"
#include "menu.h"

IPConfigScene::IPConfigScene() {
    m_ip = "";
    m_finished = false;
    m_confirmed = false;
}

void IPConfigScene::draw() {
    Renderer* renderer = Engine::getEngine().getRenderer();
    renderer->clear();
    
    // Draw input field
    SDL_Point text_pos = {160, 100};
    renderer->drawText(&text_pos, "Enter Server IP:", {255, 255, 255, 255}, 2);
    
    text_pos = {160, 150};
    renderer->drawText(&text_pos, m_ip + "_", {255, 255, 0, 255}, 2);
    
    text_pos = {160, 200};
    renderer->drawText(&text_pos, "Press ENTER to confirm", {200, 200, 200, 255}, 1);
    
    renderer->flush();
}

void IPConfigScene::eventProcess(SDL_Event* ev) {
    if(ev->type == SDL_KEYDOWN) {
        if(ev->key.keysym.sym == SDLK_RETURN) {
            if(!m_ip.empty()) {
                Settings::setServerIP(m_ip);
                m_confirmed = true;
                m_finished = true;
            }
        }
        else if(ev->key.keysym.sym == SDLK_BACKSPACE) {
            if(!m_ip.empty())
                m_ip.pop_back();
        }
    }
    else if(ev->type == SDL_TEXTINPUT) {
        m_ip += ev->text.text;
    }
}

void IPConfigScene::update(Uint32 dt) {
    // Empty implementation - no update logic needed
}

bool IPConfigScene::finished() const {
    return m_finished;
}

AppState* IPConfigScene::nextState() {
    if(!m_ip.empty() && m_confirmed) {
        Settings::setServerIP(m_ip);
    }
    return new Menu();
}