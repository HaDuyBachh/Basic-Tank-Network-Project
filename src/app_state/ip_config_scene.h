#ifndef IP_CONFIG_SCENE_H
#define IP_CONFIG_SCENE_H

#include "appstate.h"
#include <string>

class IPConfigScene : public AppState {
private:
    std::string m_ip;
    bool m_finished;
    bool m_confirmed;

public:
    IPConfigScene();
    virtual void draw() override;
    virtual void update(Uint32 dt) override;
    virtual void eventProcess(SDL_Event* ev) override;
    virtual bool finished() const override;
    virtual AppState* nextState() override;
};

#endif