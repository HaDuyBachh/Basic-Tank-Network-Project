#ifndef HISTORY_SCENE_H
#define HISTORY_SCENE_H

#include "appstate.h"
#include "../appconfig.h"
#include <vector>
#include <string>

class HistoryScene : public AppState {
private:
    std::vector<std::string> m_history_lines;
    int m_scroll_offset;
    bool m_finished;
    
public:
    HistoryScene(const std::string& username);
    virtual ~HistoryScene();
    
    virtual void draw() override;
    virtual void update(Uint32 dt) override;
    virtual void eventProcess(SDL_Event* ev) override;
    virtual bool finished() const override;
    virtual AppState* nextState() override;
    void HandleHistoryData(const std::string& username);
};

#endif