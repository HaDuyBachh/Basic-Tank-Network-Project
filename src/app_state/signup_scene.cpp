#include "signup_scene.h"
#include "menu.h"
#include <sstream>

SignupScene::SignupScene() {
    m_username = "";
    m_password = "";
    m_confirm_password = "";
    m_current_field = 0;
    m_finished = false;
    m_back_to_menu = false;
    m_registration_success = false;
}

SignupScene::~SignupScene() {
}

void SignupScene::draw() {
    Renderer* renderer = Engine::getEngine().getRenderer();
    renderer->clear();

    renderer->drawRect(&AppConfig::map_rect, {0, 0, 0, 255}, true);
    renderer->drawRect(&AppConfig::status_rect, {0, 0, 0, 255}, true);

    // Title
    SDL_Point text_pos = {160, 50};
    renderer->drawText(&text_pos, "SIGN UP", {255, 255, 255, 255}, 2);

    // Input fields
    renderInputField("Username:", m_username, 120, m_current_field == 0);
    renderInputField("Password:", m_password, 160, m_current_field == 1, true);
    renderInputField("Confirm Password:", m_confirm_password, 200, m_current_field == 2, true);

    // Submit button
    SDL_Point submit_pos = {160, 240};
    SDL_Color submit_color = m_current_field == 3 ? 
                            SDL_Color{255, 255, 0, 255} : 
                            SDL_Color{255, 255, 255, 255};
    renderer->drawText(&submit_pos, "SUBMIT", submit_color, 2);

    // Back to menu text
    SDL_Point back_pos = {20, 280};
    renderer->drawText(&back_pos, "Back to Menu (Press ESC)", {255, 255, 255, 255}, 2);

    // Show registration status if applicable
    if (m_registration_success) {
        SDL_Point status_pos = {100, 320};
        renderer->drawText(&status_pos, "Registration Successful!", {0, 255, 0, 255}, 2);
    }

    renderer->flush();
}

void SignupScene::renderInputField(const std::string& label, const std::string& content, 
                                 int yPos, bool isSelected, bool isPassword) {
    Renderer* renderer = Engine::getEngine().getRenderer();
    
    // Label
    SDL_Point label_pos = {20, yPos};
    SDL_Color text_color = isSelected ? 
                          SDL_Color{255, 255, 0, 255} : 
                          SDL_Color{255, 255, 255, 255};
    renderer->drawText(&label_pos, label, text_color, 2);

    // Input field content
    SDL_Point content_pos = {180, yPos};
    std::string display_text = isPassword ? 
                              std::string(content.length(), '*') : 
                              content;
    renderer->drawText(&content_pos, display_text + (isSelected ? "_" : ""), text_color, 2);
}

void SignupScene::update(Uint32 dt) {
    // No specific update logic needed
}

void SignupScene::eventProcess(SDL_Event *ev) {
    if (ev->type == SDL_KEYDOWN) {
        switch (ev->key.keysym.sym) {
            case SDLK_ESCAPE:
                m_back_to_menu = true;
                m_finished = true;
                break;

            case SDLK_RETURN:
                if (m_current_field == 3) {
                    submitRegistration();
                }
                break;

            case SDLK_UP:
                m_current_field = (m_current_field - 1 + 4) % 4;
                break;

            case SDLK_DOWN:
                m_current_field = (m_current_field + 1) % 4;
                break;

            case SDLK_BACKSPACE:
                switch(m_current_field) {
                    case 0:
                        if (!m_username.empty()) m_username.pop_back();
                        break;
                    case 1:
                        if (!m_password.empty()) m_password.pop_back();
                        break;
                    case 2:
                        if (!m_confirm_password.empty()) m_confirm_password.pop_back();
                        break;
                }
                break;

            default:
                break;
        }
    }
    else if (ev->type == SDL_TEXTINPUT && m_current_field < 3) {
        switch(m_current_field) {
            case 0:
                m_username += ev->text.text;
                break;
            case 1:
                m_password += ev->text.text;
                break;
            case 2:
                m_confirm_password += ev->text.text;
                break;
        }
    }
}

void SignupScene::submitRegistration() {
    if (m_username.empty() || m_password.empty() || m_confirm_password.empty()) {
        return; // All fields must be filled
    }

    if (m_password != m_confirm_password) {
        return; // Passwords must match
    }

    if (connectToServer(m_username, m_password)) {
        m_registration_success = true;
        // Wait a moment before transitioning
        SDL_Delay(2000);
        m_finished = true;
    }
}

bool SignupScene::connectToServer(const std::string& username, const std::string& password) {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8888);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == 0) {
        std::string request = "username=" + username + "&password=" + password;
        send(sock, request.c_str(), request.length(), 0);

        char buffer[1024] = {0};
        recv(sock, buffer, sizeof(buffer), 0);
        std::string response(buffer);

        closesocket(sock);
        WSACleanup();

        return response == "OK";
    }

    closesocket(sock);
    WSACleanup();
    return false;
}

bool SignupScene::finished() const {
    return m_finished;
}

AppState* SignupScene::nextState() {
    return new Menu();
}