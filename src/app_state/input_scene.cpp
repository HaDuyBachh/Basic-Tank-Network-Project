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
    SDL_Point text_start = { 180, 120 };
    renderer->drawText(&text_start, "Input: " + m_input_text, {255, 255, 255, 255}, 2);

    // Back to menu text
    text_start = { 180, 160 };
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
    }
    else if(ev->type == SDL_TEXTINPUT)
    {
        m_input_text += ev->text.text;
    }
    else if(ev->type == SDL_TEXTEDITING)
    {
        // Handle text editing
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