// room_scene.h
#pragma once
#include "appstate.h"
#include "../engine/renderer.h"
#include <vector>
#include <string>

class RoomScene : public AppState {
private:
    std::string m_room_code;
    std::string m_current_room_code;
    std::vector<std::string> m_players_in_room;
    bool m_finished;
    bool m_back_to_menu;
    bool m_room_joined;
    bool m_is_host;
    int m_current_field; // 0: room code input, 1: start button
    
    void renderInputField(const std::string& label, const std::string& content, 
                         int yPos, bool isSelected);
    bool createRoom(const std::string& room_code);
    void leaveRoom();
    bool joinRoom(const std::string& room_code);
    void updatePlayerList();
    void startGame();
    bool checkGameStarted();

public:
    RoomScene(bool isHost = true); // isHost: true để tạo phòng, false để join phòng
    ~RoomScene();

    virtual void draw() override;
    virtual void update(Uint32 dt) override;
    virtual void eventProcess(SDL_Event* ev) override;
    virtual bool finished() const override;
    virtual AppState* nextState() override;
};