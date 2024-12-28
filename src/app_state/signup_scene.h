#ifndef SIGNUP_SCENE_H
#define SIGNUP_SCENE_H

#include <winsock2.h>
#include "appstate.h"
#include "../engine/engine.h"
#include "../engine/renderer.h"
#include "../appconfig.h"
#include "../type.h"
#include <string>

class SignupScene : public AppState {
public:
    SignupScene();
    ~SignupScene();

    void draw() override;
    void update(Uint32 dt) override;
    void eventProcess(SDL_Event *ev) override; 
    bool finished() const override;
    AppState* nextState() override;

private:
    std::string m_username;
    std::string m_password;
    std::string m_confirm_password;
    int m_current_field; // 0: username, 1: password, 2: confirm password, 3: submit button
    bool m_finished;
    bool m_back_to_menu;
    bool m_registration_success;
    
    void submitRegistration();
    bool connectToServer(const std::string& username, const std::string& password);
    void renderInputField(const std::string& label, const std::string& content, int yPos, bool isSelected, bool isPassword = false);
};

#endif // SIGNUP_SCENE_H