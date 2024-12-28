#ifndef SIGNIN_SCENE_H
#define SIGNIN_SCENE_H

#include "appstate.h"
#include "../engine/engine.h"
#include "../engine/renderer.h"
#include "../appconfig.h"
#include "../type.h"
#include <string>
#include <winsock2.h>

class SigninScene : public AppState {
public:
    SigninScene();
    ~SigninScene();

    void draw() override;
    void update(Uint32 dt) override;
    void eventProcess(SDL_Event *ev) override;
    bool finished() const override;
    AppState* nextState() override;

    static std::string getCurrentUser() { return current_user; }

private:
    std::string m_username;
    std::string m_password;
    int m_current_field; // 0: username, 1: password, 2: submit button
    bool m_finished;
    bool m_back_to_menu;
    bool m_signin_success;
    
    void submitSignin();
    bool connectToServer(const std::string& username, const std::string& password);
    void renderInputField(const std::string& label, const std::string& content, int yPos, bool isSelected, bool isPassword = false);

    static std::string current_user; // Lưu tên người dùng hiện tại
};

#endif // SIGNIN_SCENE_H