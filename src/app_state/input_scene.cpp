#include "input_scene.h"
#include "menu.h"

InputScene::InputScene()
{
    m_input_text = "";
    m_finished = false;
    m_back_to_menu = false;
}

InputScene::~InputScene()
{
}

void InputScene::draw()
{
    Renderer* renderer = Engine::getEngine().getRenderer();
    renderer->clear();

    renderer->drawRect(&AppConfig::map_rect, {0, 0, 0, 255}, true);
    renderer->drawRect(&AppConfig::status_rect, {0, 0, 0, 255}, true);

    // Input field
    SDL_Point text_start = { 20, 120 };  // Căn lề bên trái
    renderer->drawText(&text_start, "Input: " + m_input_text, {255, 255, 255, 255}, 2);

    // Back to menu text
    text_start = { 20, 160 };  // Căn lề bên trái
    renderer->drawText(&text_start, "Back to Menu (Press ESC)", {255, 255, 255, 255}, 2);

    renderer->flush();
}

void InputScene::update(Uint32 dt)
{
    // No specific update logic for now
}

void InputScene::eventProcess(SDL_Event *ev)
{
    if(ev->type == SDL_KEYDOWN)
    {
        if(ev->key.keysym.sym == SDLK_ESCAPE)
        {
            m_back_to_menu = true;
            m_finished = true;
        }
        else if(ev->key.keysym.sym == SDLK_RETURN)
        {
            // Process the input text if needed
            m_finished = true;
        }
        else if(ev->key.keysym.sym == SDLK_BACKSPACE)
        {
            // Xóa ký tự cuối cùng khi bấm phím Backspace
            if (!m_input_text.empty())
            {
                m_input_text.pop_back();
            }
        }
    }
    else if(ev->type == SDL_TEXTINPUT)
    {
        m_input_text += ev->text.text;
    }
}

bool InputScene::finished() const
{
    return m_finished;
}

AppState* InputScene::nextState()
{
    if(m_back_to_menu)
    {
        return new Menu();
    }
    return nullptr; // Or transition to another state if needed
}