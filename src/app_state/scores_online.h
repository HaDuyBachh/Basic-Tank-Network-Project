#ifndef SCORES_ONLINE_H
#define SCORES_ONLINE_H

#include "appstate.h"
#include "../objects/player.h"
#include <vector>
#include <string>

class ScoresOnline : public AppState
{
private:
    std::vector<Player *> m_players;
    std::string m_room_code;
    int m_level;
    bool m_game_over;
    Uint32 m_show_time;
    bool m_score_counter_run;
    int m_score_counter;
    int m_max_score;
    bool m_is_host;
    std::vector<std::string> m_player_in_room;
    std::string m_username;

    void saveScores();

public:
    ScoresOnline(const std::string &username, bool is_host,
                 std::vector<std::string> &player_in_room,
                 std::vector<Player *> &players,
                 const std::string &room_code,
                 int level, bool game_over);
    virtual ~ScoresOnline();

    virtual void draw() override;
    virtual void update(Uint32 dt) override;
    virtual void eventProcess(SDL_Event *ev) override;
    virtual bool finished() const override;
    virtual AppState *nextState() override;
};

#endif // SCORES_ONLINE_H