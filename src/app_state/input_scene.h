#ifndef INPUT_SCENE_H
#define INPUT_SCENE_H

#include "../app_state/appstate.h"
#include "../engine/engine.h"
#include "../engine/renderer.h"
#include "../appconfig.h"
#include "../type.h"

class InputScene : public AppState
{
public:
    InputScene();
    ~InputScene();

    void draw() override;
    void update(Uint32 dt) override;
    void eventProcess(SDL_Event *ev) override;
    bool finished() const override;
    AppState* nextState() override;

private:
    std::string m_input_text;
    bool m_finished;
    bool m_back_to_menu;
};

#endif // INPUT_SCENE_H