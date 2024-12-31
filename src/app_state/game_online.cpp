#include "game_online.h"
#include "../objects/eagle.h"
#include "../appconfig.h"
#include "scores.h"
#include "menu.h"
#include <sstream>
#include <fstream>

#include <SDL2/SDL.h>
#include <stdlib.h>
#include <ctime>
#include <fstream>
#include <algorithm>
#include <iostream>
#include <cmath>
#include <windows.h>
#include <iomanip>

GameOnline::GameOnline(const std::string &room_code, bool is_host, const std::vector<std::string> &players)
{
    m_room_code = room_code;
    m_is_host = is_host;
    m_last_sync_time = 0;
    m_level_columns_count = 0;
    m_level_rows_count = 0;
    m_current_level = 0;
    m_eagle = nullptr;
    m_player_count = 2;
    m_enemy_redy_time = 0;
    m_pause = false;
    m_level_end_time = 0;
    m_protect_eagle = false;
    m_protect_eagle_time = 0;
    m_enemy_respown_position = 0;

    nextLevel();

    if (is_host)
    {
        AllocConsole();
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
        std::cout << "START DEBUG \n";
    }
}

GameOnline::~GameOnline()
{
    clearLevel();
}

void GameOnline::draw()
{
    Engine &engine = Engine::getEngine();
    Renderer *renderer = engine.getRenderer();
    renderer->clear();

    if (m_level_start_screen)
    {
        std::string level_name = "STAGE " + Engine::intToString(m_current_level);
        renderer->drawText(nullptr, level_name, {255, 255, 255, 255}, 1);
    }
    else
    {
        renderer->drawRect(&AppConfig::map_rect, {0, 0, 0, 0}, true);
        for (auto row : m_level)
            for (auto item : row)
                if (item != nullptr)
                    item->draw();

        for (auto player : m_players)
            player->draw();
        for (auto enemy : m_enemies)
            enemy->draw();
        for (auto bush : m_bushes)
            bush->draw();
        for (auto bonus : m_bonuses)
            if (m_is_host)
            {
                bonus->draw(); // Host uses normal draw with blinking
            }
            else
            {
                bonus->drawOnline(); // Client always shows bonus
            }
            
        m_eagle->draw();

        if (m_game_over)
        {
            SDL_Point pos;
            pos.x = -1;
            pos.y = m_game_over_position;
            renderer->drawText(&pos, AppConfig::game_over_text, {255, 10, 10, 255});
        }

        //===========Status gry===========
        SDL_Rect src = engine.getSpriteConfig()->getSpriteData(ST_LEFT_ENEMY)->rect;
        SDL_Rect dst;
        SDL_Point p_dst;
        // wrogowie do zabicia
        for (int i = 0; i < m_enemy_to_kill; i++)
        {
            dst = {AppConfig::status_rect.x + 8 + src.w * (i % 2), 5 + src.h * (i / 2), src.w, src.h};
            renderer->drawObject(&src, &dst);
        }
        // życia graczy
        int i = 0;
        for (auto player : m_players)
        {
            dst = {AppConfig::status_rect.x + 5, i * 18 + 180, 16, 16};
            p_dst = {dst.x + dst.w + 2, dst.y + 3};
            i++;
            renderer->drawObject(&player->src_rect, &dst);
            renderer->drawText(&p_dst, Engine::intToString(player->lives_count), {0, 0, 0, 255}, 3);
        }
        // numer mapy
        src = engine.getSpriteConfig()->getSpriteData(ST_STAGE_STATUS)->rect;
        dst = {AppConfig::status_rect.x + 8, static_cast<int>(185 + (m_players.size() + m_killed_players.size()) * 18), src.w, src.h};
        p_dst = {dst.x + 10, dst.y + 26};
        renderer->drawObject(&src, &dst);
        renderer->drawText(&p_dst, Engine::intToString(m_current_level), {0, 0, 0, 255}, 2);

        if (m_pause)
            renderer->drawText(nullptr, std::string("PAUSE"), {200, 0, 0, 255}, 1);
    }

    renderer->flush();
}

void GameOnline::updateCollider(Uint32 dt)
{
    std::vector<Player *>::iterator pl1, pl2;
    std::vector<Enemy *>::iterator en1, en2;
    // kiểm tra va chạm giữa xe tăng của người chơi với nhau
    for (pl1 = m_players.begin(); pl1 != m_players.end(); pl1++)
        for (pl2 = pl1 + 1; pl2 != m_players.end(); pl2++)
            checkCollisionTwoTanks(*pl1, *pl2, dt);

    // kiểm tra va chạm của xe tăng địch với nhau
    for (en1 = m_enemies.begin(); en1 != m_enemies.end(); en1++)
        for (en2 = en1 + 1; en2 != m_enemies.end(); en2++)
            checkCollisionTwoTanks(*en1, *en2, dt);

    // kiểm tra va chạm của quả bóng với tường
    for (auto enemy : m_enemies)
        for (auto bullet : enemy->bullets)
            checkCollisionBulletWithLevel(bullet);
    for (auto player : m_players)
        for (auto bullet : player->bullets)
        {
            checkCollisionBulletWithLevel(bullet);
            checkCollisionBulletWithBush(bullet);
        }

    for (auto player : m_players)
        for (auto enemy : m_enemies)
        {
            // kiểm tra va chạm giữa xe tăng của đối thủ và người chơi
            checkCollisionTwoTanks(player, enemy, dt);
            // kiểm tra sự va chạm của đạn của người chơi với đối thủ
            checkCollisionPlayerBulletsWithEnemy(player, enemy);

            // kiểm tra sự va chạm của đạn của người chơi với đạn của đối phương
            for (auto bullet1 : player->bullets)
                for (auto bullet2 : enemy->bullets)
                    checkCollisionTwoBullets(bullet1, bullet2);
        }

    // kiểm tra va chạm của tên lửa với người chơi
    for (auto enemy : m_enemies)
        for (auto player : m_players)
            checkCollisionEnemyBulletsWithPlayer(enemy, player);

    // kiểm tra va chạm giữa người chơi và bonus
    for (auto player : m_players)
        for (auto bonus : m_bonuses)
            checkCollisionPlayerWithBonus(player, bonus);

    // Kiểm tra xe tăng có va chạm với cấp không
    for (auto enemy : m_enemies)
        checkCollisionTankWithLevel(enemy, dt);
    for (auto player : m_players)
        checkCollisionTankWithLevel(player, dt);
}

void GameOnline::eventProcess(SDL_Event *ev)
{
    if (ev->type == SDL_KEYDOWN)
    {
        switch (ev->key.keysym.sym)
        {
        case SDLK_n:
            m_enemy_to_kill = 0;
            m_finished = true;
            break;
        case SDLK_b:
            m_enemy_to_kill = 0;
            m_current_level -= 2;
            m_finished = true;
            break;
        case SDLK_t:
            AppConfig::show_enemy_target = !AppConfig::show_enemy_target;
            break;
        case SDLK_RETURN:
            m_pause = !m_pause;
            break;
        case SDLK_ESCAPE:
            m_finished = true;
            break;
        }
    }
}

void GameOnline::loadLevel(std::string path)
{
    std::fstream level(path, std::ios::in);
    std::string line;
    int j = -1;

    if (level.is_open())
    {
        while (!level.eof())
        {
            std::getline(level, line);
            std::vector<Object *> row;
            j++;
            for (unsigned i = 0; i < line.size(); i++)
            {
                Object *obj;
                switch (line.at(i))
                {
                case '#':
                    obj = new Brick(i * AppConfig::tile_rect.w, j * AppConfig::tile_rect.h);
                    break;
                case '@':
                    obj = new Object(i * AppConfig::tile_rect.w, j * AppConfig::tile_rect.h, ST_STONE_WALL);
                    break;
                case '%':
                    m_bushes.push_back(new Object(i * AppConfig::tile_rect.w, j * AppConfig::tile_rect.h, ST_BUSH));
                    obj = nullptr;
                    break;
                case '~':
                    obj = new Object(i * AppConfig::tile_rect.w, j * AppConfig::tile_rect.h, ST_WATER);
                    break;
                case '-':
                    obj = new Object(i * AppConfig::tile_rect.w, j * AppConfig::tile_rect.h, ST_ICE);
                    break;
                default:
                    obj = nullptr;
                }
                row.push_back(obj);
            }
            m_level.push_back(row);
        }
    }

    m_level_rows_count = m_level.size();
    if (m_level_rows_count)
        m_level_columns_count = m_level.at(0).size();
    else
        m_level_columns_count = 0;

    // tworzymy orzełka
    m_eagle = new Eagle(12 * AppConfig::tile_rect.w, (m_level_rows_count - 2) * AppConfig::tile_rect.h);

    // wyczyszczenie miejsca orzełeka
    for (int i = 12; i < 14 && i < m_level_columns_count; i++)
    {
        for (int j = m_level_rows_count - 2; j < m_level_rows_count; j++)
        {
            if (m_level.at(j).at(i) != nullptr)
            {
                delete m_level.at(j).at(i);
                m_level.at(j).at(i) = nullptr;
            }
        }
    }
}

bool GameOnline::finished() const
{
    return m_finished;
}

AppState *GameOnline::nextState()
{
    if (m_game_over || m_enemy_to_kill <= 0)
    {
        m_players.erase(std::remove_if(m_players.begin(), m_players.end(), [this](Player *p)
                                       {m_killed_players.push_back(p); return true; }),
                        m_players.end());
        Scores *scores = new Scores(m_killed_players, m_current_level, m_game_over);
        return scores;
    }
    Menu *m = new Menu;
    return m;
}

void GameOnline::clearLevel()
{
    for (auto enemy : m_enemies)
        delete enemy;
    m_enemies.clear();

    for (auto player : m_players)
        delete player;
    m_players.clear();

    for (auto bonus : m_bonuses)
        delete bonus;
    m_bonuses.clear();

    for (auto row : m_level)
    {
        for (auto item : row)
            if (item != nullptr)
                delete item;
        row.clear();
    }
    m_level.clear();

    for (auto bush : m_bushes)
        delete bush;
    m_bushes.clear();

    if (m_eagle != nullptr)
        delete m_eagle;
    m_eagle = nullptr;
}

void GameOnline::checkCollisionTankWithLevel(Tank *tank, Uint32 dt)
{
    if (tank->to_erase)
        return;

    int row_start, row_end;
    int column_start, column_end;

    SDL_Rect pr, *lr;
    Object *o;

    //========================kolizja z elementami mapy========================
    switch (tank->direction)
    {
    case D_UP:
        row_end = tank->collision_rect.y / AppConfig::tile_rect.h;
        row_start = row_end - 1;
        column_start = tank->collision_rect.x / AppConfig::tile_rect.w - 1;
        column_end = (tank->collision_rect.x + tank->collision_rect.w) / AppConfig::tile_rect.w + 1;
        break;
    case D_RIGHT:
        column_start = (tank->collision_rect.x + tank->collision_rect.w) / AppConfig::tile_rect.w;
        column_end = column_start + 1;
        row_start = tank->collision_rect.y / AppConfig::tile_rect.h - 1;
        row_end = (tank->collision_rect.y + tank->collision_rect.h) / AppConfig::tile_rect.h + 1;
        break;
    case D_DOWN:
        row_start = (tank->collision_rect.y + tank->collision_rect.h) / AppConfig::tile_rect.h;
        row_end = row_start + 1;
        column_start = tank->collision_rect.x / AppConfig::tile_rect.w - 1;
        column_end = (tank->collision_rect.x + tank->collision_rect.w) / AppConfig::tile_rect.w + 1;
        break;
    case D_LEFT:
        column_end = tank->collision_rect.x / AppConfig::tile_rect.w;
        column_start = column_end - 1;
        row_start = tank->collision_rect.y / AppConfig::tile_rect.h - 1;
        row_end = (tank->collision_rect.y + tank->collision_rect.h) / AppConfig::tile_rect.h + 1;
        break;
    }
    if (column_start < 0)
        column_start = 0;
    if (row_start < 0)
        row_start = 0;
    if (column_end >= m_level_columns_count)
        column_end = m_level_columns_count - 1;
    if (row_end >= m_level_rows_count)
        row_end = m_level_rows_count - 1;

    pr = tank->nextCollisionRect(dt);
    SDL_Rect intersect_rect;

    for (int i = row_start; i <= row_end; i++)
        for (int j = column_start; j <= column_end; j++)
        {
            if (tank->stop)
                break;
            o = m_level.at(i).at(j);
            if (o == nullptr)
                continue;
            if (tank->testFlag(TSF_BOAT) && o->type == ST_WATER)
                continue;

            lr = &o->collision_rect;

            intersect_rect = intersectRect(lr, &pr);
            if (intersect_rect.w > 0 && intersect_rect.h > 0)
            {
                if (o->type == ST_ICE)
                {
                    if (intersect_rect.w > 10 && intersect_rect.h > 10)
                        tank->setFlag(TSF_ON_ICE);
                    continue;
                }
                else
                    tank->collide(intersect_rect);
                break;
            }
        }

    //========================kolizja z granicami mapy========================
    SDL_Rect outside_map_rect;
    // hình chữ nhật ở phía bên trái của bản đồ
    outside_map_rect.x = -AppConfig::tile_rect.w;
    outside_map_rect.y = -AppConfig::tile_rect.h;
    outside_map_rect.w = AppConfig::tile_rect.w;
    outside_map_rect.h = AppConfig::map_rect.h + 2 * AppConfig::tile_rect.h;
    intersect_rect = intersectRect(&outside_map_rect, &pr);
    if (intersect_rect.w > 0 && intersect_rect.h > 0)
        tank->collide(intersect_rect);

    // hình chữ nhật ở phía bên phải của bản đồ
    outside_map_rect.x = AppConfig::map_rect.w;
    outside_map_rect.y = -AppConfig::tile_rect.h;
    outside_map_rect.w = AppConfig::tile_rect.w;
    outside_map_rect.h = AppConfig::map_rect.h + 2 * AppConfig::tile_rect.h;
    intersect_rect = intersectRect(&outside_map_rect, &pr);
    if (intersect_rect.w > 0 && intersect_rect.h > 0)
        tank->collide(intersect_rect);

    // hình chữ nhật ở phía trên của bản đồ
    outside_map_rect.x = 0;
    outside_map_rect.y = -AppConfig::tile_rect.h;
    outside_map_rect.w = AppConfig::map_rect.w;
    outside_map_rect.h = AppConfig::tile_rect.h;
    intersect_rect = intersectRect(&outside_map_rect, &pr);
    if (intersect_rect.w > 0 && intersect_rect.h > 0)
        tank->collide(intersect_rect);

    // hình chữ nhật ở phía dưới bản đồ
    outside_map_rect.x = 0;
    outside_map_rect.y = AppConfig::map_rect.h;
    outside_map_rect.w = AppConfig::map_rect.w;
    outside_map_rect.h = AppConfig::tile_rect.h;
    intersect_rect = intersectRect(&outside_map_rect, &pr);
    if (intersect_rect.w > 0 && intersect_rect.h > 0)
        tank->collide(intersect_rect);

    //========================va chạm với đại bàng========================
    intersect_rect = intersectRect(&m_eagle->collision_rect, &pr);
    if (intersect_rect.w > 0 && intersect_rect.h > 0)
        tank->collide(intersect_rect);
}

void GameOnline::checkCollisionTwoTanks(Tank *tank1, Tank *tank2, Uint32 dt)
{
    SDL_Rect cr1 = tank1->nextCollisionRect(dt);
    SDL_Rect cr2 = tank2->nextCollisionRect(dt);
    SDL_Rect intersect_rect = intersectRect(&cr1, &cr2);

    if (intersect_rect.w > 0 && intersect_rect.h > 0)
    {
        tank1->collide(intersect_rect);
        tank2->collide(intersect_rect);
    }
}

void GameOnline::checkCollisionBulletWithLevel(Bullet *bullet)
{
    if (bullet == nullptr)
        return;
    if (bullet->collide)
        return;

    int row_start, row_end;
    int column_start, column_end;

    SDL_Rect *br, *lr;
    SDL_Rect intersect_rect;
    Object *o;

    //========================va chạm với các phần tử bản đồ========================
    switch (bullet->direction)
    {
    case D_UP:
        row_start = row_end = bullet->collision_rect.y / AppConfig::tile_rect.h;
        column_start = bullet->collision_rect.x / AppConfig::tile_rect.w;
        column_end = (bullet->collision_rect.x + bullet->collision_rect.w) / AppConfig::tile_rect.w;
        break;
    case D_RIGHT:
        column_start = column_end = (bullet->collision_rect.x + bullet->collision_rect.w) / AppConfig::tile_rect.w;
        row_start = bullet->collision_rect.y / AppConfig::tile_rect.h;
        row_end = (bullet->collision_rect.y + bullet->collision_rect.h) / AppConfig::tile_rect.h;
        break;
    case D_DOWN:
        row_start = row_end = (bullet->collision_rect.y + bullet->collision_rect.h) / AppConfig::tile_rect.h;
        column_start = bullet->collision_rect.x / AppConfig::tile_rect.w;
        column_end = (bullet->collision_rect.x + bullet->collision_rect.w) / AppConfig::tile_rect.w;
        break;
    case D_LEFT:
        column_start = column_end = bullet->collision_rect.x / AppConfig::tile_rect.w;
        row_start = bullet->collision_rect.y / AppConfig::tile_rect.h;
        row_end = (bullet->collision_rect.y + bullet->collision_rect.h) / AppConfig::tile_rect.h;
        break;
    }
    if (column_start < 0)
        column_start = 0;
    if (row_start < 0)
        row_start = 0;
    if (column_end >= m_level_columns_count)
        column_end = m_level_columns_count - 1;
    if (row_end >= m_level_rows_count)
        row_end = m_level_rows_count - 1;

    br = &bullet->collision_rect;

    for (int i = row_start; i <= row_end; i++)
        for (int j = column_start; j <= column_end; j++)
        {
            o = m_level.at(i).at(j);
            if (o == nullptr)
                continue;
            if (o->type == ST_ICE || o->type == ST_WATER)
                continue;

            lr = &o->collision_rect;
            intersect_rect = intersectRect(lr, br);

            if (intersect_rect.w > 0 && intersect_rect.h > 0)
            {
                if (bullet->increased_damage)
                {
                    delete o;
                    m_level.at(i).at(j) = nullptr;
                }
                else if (o->type == ST_BRICK_WALL)
                {
                    Brick *brick = dynamic_cast<Brick *>(o);
                    brick->bulletHit(bullet->direction);
                    if (brick->to_erase)
                    {
                        delete brick;
                        m_level.at(i).at(j) = nullptr;
                    }
                }
                bullet->destroy();
            }
        }

    //========================va chạm với ranh giới bản đồ========================
    if (br->x < 0 || br->y < 0 || br->x + br->w > AppConfig::map_rect.w || br->y + br->h > AppConfig::map_rect.h)
    {
        bullet->destroy();
    }
    //========================va chạm với đại bàng========================
    if (m_eagle->type == ST_EAGLE && !m_game_over)
    {
        intersect_rect = intersectRect(&m_eagle->collision_rect, br);
        if (intersect_rect.w > 0 && intersect_rect.h > 0)
        {
            bullet->destroy();
            m_eagle->destroy();
            m_game_over_position = AppConfig::map_rect.h;
            m_game_over = true;
        }
    }
}

void GameOnline::checkCollisionBulletWithBush(Bullet *bullet)
{
    if (bullet == nullptr)
        return;
    if (bullet->collide)
        return;
    if (!bullet->increased_damage)
        return;

    SDL_Rect *br, *lr;
    SDL_Rect intersect_rect;
    br = &bullet->collision_rect;

    for (auto bush : m_bushes)
    {
        if (bush->to_erase)
            continue;
        lr = &bush->collision_rect;
        intersect_rect = intersectRect(lr, br);

        if (intersect_rect.w > 0 && intersect_rect.h > 0)
        {
            bullet->destroy();
            bush->to_erase = true;
        }
    }
}

void GameOnline::checkCollisionPlayerBulletsWithEnemy(Player *player, Enemy *enemy)
{
    if (player->to_erase || enemy->to_erase)
        return;
    if (enemy->testFlag(TSF_DESTROYED))
        return;
    SDL_Rect intersect_rect;

    for (auto bullet : player->bullets)
    {
        if (!bullet->to_erase && !bullet->collide)
        {
            intersect_rect = intersectRect(&bullet->collision_rect, &enemy->collision_rect);
            if (intersect_rect.w > 0 && intersect_rect.h > 0)
            {
                if (enemy->testFlag(TSF_BONUS))
                    generateBonus();

                bullet->destroy();
                enemy->destroy();
                if (enemy->lives_count <= 0)
                    m_enemy_to_kill--;
                player->score += enemy->scoreForHit();
            }
        }
    }
}

void GameOnline::checkCollisionEnemyBulletsWithPlayer(Enemy *enemy, Player *player)
{
    if (enemy->to_erase || player->to_erase)
        return;
    if (player->testFlag(TSF_DESTROYED))
        return;
    SDL_Rect intersect_rect;

    for (auto bullet : enemy->bullets)
    {
        if (!bullet->to_erase && !bullet->collide)
        {
            intersect_rect = intersectRect(&bullet->collision_rect, &player->collision_rect);
            if (intersect_rect.w > 0 && intersect_rect.h > 0)
            {
                bullet->destroy();
                player->destroy();
            }
        }
    }
}

void GameOnline::checkCollisionTwoBullets(Bullet *bullet1, Bullet *bullet2)
{
    if (bullet1 == nullptr || bullet2 == nullptr)
        return;
    if (bullet1->to_erase || bullet2->to_erase)
        return;

    SDL_Rect intersect_rect = intersectRect(&bullet1->collision_rect, &bullet2->collision_rect);

    if (intersect_rect.w > 0 && intersect_rect.h > 0)
    {
        bullet1->destroy();
        bullet2->destroy();
    }
}

void GameOnline::checkCollisionPlayerWithBonus(Player *player, Bonus *bonus)
{
    if (player->to_erase || bonus->to_erase)
        return;

    SDL_Rect intersect_rect = intersectRect(&player->collision_rect, &bonus->collision_rect);
    if (intersect_rect.w > 0 && intersect_rect.h > 0)
    {
        player->score += 300;

        if (bonus->type == ST_BONUS_GRENADE)
        {
            for (auto enemy : m_enemies)
            {
                if (!enemy->to_erase)
                {
                    player->score += 200;
                    while (enemy->lives_count > 0)
                        enemy->destroy();
                    m_enemy_to_kill--;
                }
            }
        }
        else if (bonus->type == ST_BONUS_HELMET)
        {
            player->setFlag(TSF_SHIELD);
        }
        else if (bonus->type == ST_BONUS_CLOCK)
        {
            for (auto enemy : m_enemies)
                if (!enemy->to_erase)
                    enemy->setFlag(TSF_FROZEN);
        }
        else if (bonus->type == ST_BONUS_SHOVEL)
        {
            m_protect_eagle = true;
            m_protect_eagle_time = 0;
            for (int i = 0; i < 3; i++)
            {
                if (m_level.at(m_level_rows_count - i - 1).at(11) != nullptr)
                    delete m_level.at(m_level_rows_count - i - 1).at(11);
                m_level.at(m_level_rows_count - i - 1).at(11) = new Object(11 * AppConfig::tile_rect.w, (m_level_rows_count - i - 1) * AppConfig::tile_rect.h, ST_STONE_WALL);

                if (m_level.at(m_level_rows_count - i - 1).at(14) != nullptr)
                    delete m_level.at(m_level_rows_count - i - 1).at(14);
                m_level.at(m_level_rows_count - i - 1).at(14) = new Object(14 * AppConfig::tile_rect.w, (m_level_rows_count - i - 1) * AppConfig::tile_rect.h, ST_STONE_WALL);
            }
            for (int i = 12; i < 14; i++)
            {
                if (m_level.at(m_level_rows_count - 3).at(i) != nullptr)
                    delete m_level.at(m_level_rows_count - 3).at(i);
                m_level.at(m_level_rows_count - 3).at(i) = new Object(i * AppConfig::tile_rect.w, (m_level_rows_count - 3) * AppConfig::tile_rect.h, ST_STONE_WALL);
            }
        }
        else if (bonus->type == ST_BONUS_TANK)
        {
            player->lives_count++;
        }
        else if (bonus->type == ST_BONUS_STAR)
        {
            player->changeStarCountBy(1);
        }
        else if (bonus->type == ST_BONUS_GUN)
        {
            player->changeStarCountBy(3);
        }
        else if (bonus->type == ST_BONUS_BOAT)
        {
            player->setFlag(TSF_BOAT);
        }
        bonus->to_erase = true;
    }
}

void GameOnline::nextLevel()
{
    m_current_level++;
    if (m_current_level > 35)
        m_current_level = 1;
    if (m_current_level < 0)
        m_current_level = 35;

    m_level_start_screen = true;
    m_level_start_time = 0;
    m_game_over = false;
    m_finished = false;
    m_enemy_to_kill = AppConfig::enemy_start_count;

    std::string level_path = AppConfig::levels_path + Engine::intToString(m_current_level);
    loadLevel(level_path);

    if (m_players.empty())
    {
        if (m_player_count == 2)
        {
            Player *p1 = new Player(AppConfig::player_starting_point.at(0).x, AppConfig::player_starting_point.at(0).y, ST_PLAYER_1);
            Player *p2 = new Player(AppConfig::player_starting_point.at(1).x, AppConfig::player_starting_point.at(1).y, ST_PLAYER_2);
            p1->player_keys = AppConfig::player_keys.at(0);
            p2->player_keys = AppConfig::player_keys.at(1);
            m_players.push_back(p1);
            m_players.push_back(p2);
        }
        else
        {
            Player *p1 = new Player(AppConfig::player_starting_point.at(0).x, AppConfig::player_starting_point.at(0).y, ST_PLAYER_1);
            p1->player_keys = AppConfig::player_keys.at(0);
            m_players.push_back(p1);
        }
    }
}

void GameOnline::generateEnemy()
{
    float p = static_cast<float>(rand()) / RAND_MAX;
    SpriteType type = static_cast<SpriteType>(p < (0.00735 * m_current_level + 0.09265) ? ST_TANK_D : rand() % (ST_TANK_C - ST_TANK_A + 1) + ST_TANK_A);
    Enemy *e = new Enemy(AppConfig::enemy_starting_point.at(m_enemy_respown_position).x, AppConfig::enemy_starting_point.at(m_enemy_respown_position).y, type);
    m_enemy_respown_position++;
    if (m_enemy_respown_position >= AppConfig::enemy_starting_point.size())
        m_enemy_respown_position = 0;

    double a, b, c;
    if (m_current_level <= 17)
    {
        a = -0.040625 * m_current_level + 0.940625;
        b = -0.028125 * m_current_level + 0.978125;
        c = -0.014375 * m_current_level + 0.994375;
    }
    else
    {
        a = -0.012778 * m_current_level + 0.467222;
        b = -0.025000 * m_current_level + 0.925000;
        c = -0.036111 * m_current_level + 1.363889;
    }

    p = static_cast<float>(rand()) / RAND_MAX;
    if (p < a)
        e->lives_count = 1;
    else if (p < b)
        e->lives_count = 2;
    else if (p < c)
        e->lives_count = 3;
    else
        e->lives_count = 4;

    p = static_cast<float>(rand()) / RAND_MAX;
    if (p < 0.12)
        e->setFlag(TSF_BONUS);

    m_enemies.push_back(e);
}

void GameOnline::generateBonus()
{
    Bonus *b = new Bonus(0, 0, static_cast<SpriteType>(rand() % (ST_BONUS_BOAT - ST_BONUS_GRENADE + 1) + ST_BONUS_GRENADE));
    SDL_Rect intersect_rect;
    do
    {
        b->pos_x = rand() % (AppConfig::map_rect.x + AppConfig::map_rect.w - 1 * AppConfig::tile_rect.w);
        b->pos_y = rand() % (AppConfig::map_rect.y + AppConfig::map_rect.h - 1 * AppConfig::tile_rect.h);
        b->update(0);
        intersect_rect = intersectRect(&b->collision_rect, &m_eagle->collision_rect);
    } while (intersect_rect.w > 0 && intersect_rect.h > 0);

    m_bonuses.push_back(b);
}

GameOnline::GameSnapshot GameOnline::captureGameState()
{
    GameSnapshot snapshot;

    // Capture players state
    for (auto player : m_players)
    {
        PlayerSnapshot p_snapshot;
        p_snapshot.pos_x = player->pos_x;
        p_snapshot.pos_y = player->pos_y;
        p_snapshot.direction = player->direction;
        p_snapshot.is_firing = player->m_fire_time > AppConfig::player_reload_time;
        p_snapshot.is_destroyed = player->testFlag(TSF_DESTROYED);
        p_snapshot.lives_count = player->lives_count;
        p_snapshot.star_count = player->star_count;

        // Capture player's bullets
        for (auto bullet : player->bullets)
        {
            BulletSnapshot b_snapshot;
            b_snapshot.pos_x = bullet->pos_x;
            b_snapshot.pos_y = bullet->pos_y;
            b_snapshot.direction = bullet->direction;
            b_snapshot.collide = bullet->collide;
            b_snapshot.increased_damage = bullet->increased_damage;
            p_snapshot.bullets.push_back(b_snapshot);
        }

        snapshot.players.push_back(p_snapshot);
    }

    // Capture enemies state
    for (auto enemy : m_enemies)
    {
        EnemySnapshot e_snapshot;
        e_snapshot.pos_x = enemy->pos_x;
        e_snapshot.pos_y = enemy->pos_y;
        e_snapshot.direction = enemy->direction;
        e_snapshot.type = enemy->type;
        e_snapshot.lives_count = enemy->lives_count;
        e_snapshot.is_destroyed = enemy->testFlag(TSF_DESTROYED);
        e_snapshot.has_bonus = enemy->testFlag(TSF_BONUS);

        // Capture enemy's bullets
        for (auto bullet : enemy->bullets)
        {
            BulletSnapshot b_snapshot;
            b_snapshot.pos_x = bullet->pos_x;
            b_snapshot.pos_y = bullet->pos_y;
            b_snapshot.direction = bullet->direction;
            b_snapshot.collide = bullet->collide;
            b_snapshot.increased_damage = bullet->increased_damage;
            e_snapshot.bullets.push_back(b_snapshot);
        }

        snapshot.enemies.push_back(e_snapshot);
    }

    // Capture bonuses
    for (auto bonus : m_bonuses)
    {
        BonusData b_data;
        b_data.pos_x = bonus->pos_x;
        b_data.pos_y = bonus->pos_y;
        b_data.type = bonus->type;
        b_data.is_active = !bonus->to_erase;
        snapshot.bonuses.push_back(b_data);
    }

    // Capture game state
    snapshot.eagle_destroyed = m_eagle->type == ST_DESTROY_EAGLE;
    snapshot.current_level = m_current_level;
    snapshot.game_over = m_game_over;
    snapshot.game_over_position = m_game_over_position;

    return snapshot;
}

std::string GameOnline::GameStateSendData()
{
    GameSnapshot snapshot = captureGameState();
    std::stringstream ss;

    // Game state header
    ss << "game_state:" << m_room_code << ":"
       << snapshot.current_level << ","
       << (snapshot.game_over ? "1" : "0") << ","
       << std::fixed << std::setprecision(3) << snapshot.game_over_position << ","
       << (snapshot.eagle_destroyed ? "1" : "0") << ","
       << m_enemy_to_kill;

    // Players section with count
    ss << ":players:" << snapshot.players.size() << ":";
    for (auto &p : snapshot.players)
    {
        if (std::isnan(p.pos_x) || std::isnan(p.pos_y))
            continue;

        ss << std::fixed << std::setprecision(3)
           << p.pos_x << "," << p.pos_y << ","
           << p.direction << ","
           << (p.is_firing ? "1" : "0") << ","
           << (p.is_destroyed ? "1" : "0") << ","
           << p.lives_count << ","
           << p.star_count << ","
           << p.score << ";"
           
        << p.bullets.size() << ";"; // Add bullet count
        for (auto &b : p.bullets)
        {
            if (std::isnan(b.pos_x) || std::isnan(b.pos_y))
                continue;
            ss << b.pos_x << "," << b.pos_y << ","
               << b.direction << ","
               << (b.collide ? "1" : "0") << ","
               << (b.increased_damage ? "1" : "0") << "|";
        }
        ss << "#";
    }

    // Enemies section with count
    ss << "enemies:" << snapshot.enemies.size() << ":";
    for (auto &e : snapshot.enemies)
    {
        if (std::isnan(e.pos_x) || std::isnan(e.pos_y))
            continue;

        ss << std::fixed << std::setprecision(3)
           << e.pos_x << "," << e.pos_y << ","
           << e.direction << "," << e.type << ","
           << e.lives_count << ","
           << (e.is_destroyed ? "1" : "0") << ","
           << (e.has_bonus ? "1" : "0") << ";"
           << e.bullets.size() << ";"; // Add bullet count

        for (auto &b : e.bullets)
        {
            if (std::isnan(b.pos_x) || std::isnan(b.pos_y))
                continue;
            ss << b.pos_x << "," << b.pos_y << ","
               << b.direction << ","
               << (b.collide ? "1" : "0") << ","
               << (b.increased_damage ? "1" : "0") << "|";
        }
        ss << "#";
    }

    // Bonuses section with count
    ss << "bonuses:" << snapshot.bonuses.size() << ":";
    for (auto &b : snapshot.bonuses)
    {
        if (std::isnan(b.pos_x) || std::isnan(b.pos_y))
            continue;
        ss << std::fixed << std::setprecision(3)
           << b.pos_x << "," << b.pos_y << ","
           << b.type << ","
           << (b.is_active ? "1" : "0") << "#";
    }

    // Add brick state section
    ss << "brick_sprites:";
    for (int i = 0; i < m_level_rows_count; i++)
    {
        for (int j = 0; j < m_level_columns_count; j++)
        {
            if (m_level[i][j] != nullptr && m_level[i][j]->type == ST_BRICK_WALL)
            {
                auto brick = m_level[i][j];
                ss << i << "," << j << ","
                   << brick->src_rect.x << ","
                   << brick->src_rect.y << ","
                   << brick->src_rect.w << ","
                   << brick->src_rect.h << "#";
            }
        }
    }

    return ss.str();
}

void GameOnline::RecvGameState(const std::string &data)
{
    try
    {
        // Clear existing objects
        for (auto enemy : m_enemies)
            delete enemy;
        m_enemies.clear();

        for (auto bonus : m_bonuses)
            delete bonus;
        m_bonuses.clear();

        std::stringstream ss(data);
        std::string header, room_code, game_data;

        // Parse header
        std::getline(ss, header, ':');
        if (header != "game_state")
        {
            std::cout << "Invalid header: " << header << std::endl;
            return;
        }

        // Parse room code
        std::getline(ss, room_code, ':');
        if (room_code != m_room_code)
        {
            std::cout << "Wrong room code: " << room_code << std::endl;
            return;
        }

        // Parse game state
        std::getline(ss, game_data, ':');
        std::vector<std::string> game_values;
        std::stringstream game_ss(game_data);
        std::string value;

        while (std::getline(game_ss, value, ','))
        {
            game_values.push_back(value);
        }

        if (game_values.size() >= 5)
        {
            m_current_level = std::stoi(game_values[0]);
            m_game_over = game_values[1] == "1";
            m_game_over_position = std::stod(game_values[2]);
            bool eagle_destroyed = game_values[3] == "1";
            m_enemy_to_kill = std::stoi(game_values[4]);
            if (eagle_destroyed)
                m_eagle->destroy();
        }

        // Parse players section
        std::getline(ss, header, ':');
        if (header != "players")
        {
            std::cout << "Invalid players section" << std::endl;
            return;
        }

        // Get player count
        std::string count_str;
        std::getline(ss, count_str, ':');
        int player_count = std::stoi(count_str);
        std::cout << "Players count: " << player_count << std::endl;

        // Parse each player
        for (int i = 0; i < player_count; i++)
        {
            std::string player_data;
            std::getline(ss, player_data, '#');
            if (player_data.empty())
                continue;

            // Split player data and bullets
            size_t semi_pos = player_data.find(';');
            if (semi_pos == std::string::npos)
                continue;

            std::string stats = player_data.substr(0, semi_pos);
            std::string bullets_data = player_data.substr(semi_pos + 1);

            // Parse player stats
            std::stringstream stats_ss(stats);
            std::vector<std::string> values;

            while (std::getline(stats_ss, value, ','))
            {
                values.push_back(value);
            }

            if (values.size() >= 8 && i < m_players.size())
            {
                Player *player = m_players[i];
                player->pos_x = std::stod(values[0]);
                player->pos_y = std::stod(values[1]);
                player->direction = static_cast<Direction>(std::stoi(values[2]));
                player->m_fire_time = (values[3] == "1") ? AppConfig::player_reload_time + 1 : 0;
                if (values[4] == "1")
                    player->setFlag(TSF_DESTROYED);
                else
                    player->clearFlag(TSF_DESTROYED);
                player->lives_count = std::stoi(values[5]);
                player->star_count = std::stoi(values[6]);
                player->score = std::stoi(values[7]);

                // Parse bullets count
                semi_pos = bullets_data.find(';');
                if (semi_pos != std::string::npos)
                {
                    int bullet_count = std::stoi(bullets_data.substr(0, semi_pos));
                    std::string bullets = bullets_data.substr(semi_pos + 1);

                    player->bullets.clear();
                    std::stringstream bullets_ss(bullets);
                    std::string bullet_data;

                    for (int j = 0; j < bullet_count &&
                                    std::getline(bullets_ss, bullet_data, '|');
                         j++)
                    {

                        std::stringstream bullet_ss(bullet_data);
                        std::vector<std::string> bullet_values;

                        while (std::getline(bullet_ss, value, ','))
                        {
                            bullet_values.push_back(value);
                        }

                        if (bullet_values.size() >= 5)
                        {
                            Bullet *bullet = new Bullet(
                                std::stod(bullet_values[0]),
                                std::stod(bullet_values[1]));
                            bullet->direction = static_cast<Direction>(std::stoi(bullet_values[2]));
                            bullet->collide = bullet_values[3] == "1";
                            bullet->increased_damage = bullet_values[4] == "1";
                            player->bullets.push_back(bullet);
                        }
                    }
                }
            }
        }

        // Parse enemies section
        std::getline(ss, header, ':');
        if (header != "enemies")
        {
            std::cout << "Invalid enemies section" << std::endl;
            return;
        }

        // Get enemy count
        std::getline(ss, count_str, ':');
        int enemy_count = std::stoi(count_str);
        std::cout << "Enemies count: " << enemy_count << std::endl;

        // Parse each enemy
        for (int i = 0; i < enemy_count; i++)
        {
            std::string enemy_data;
            std::getline(ss, enemy_data, '#');
            if (enemy_data.empty())
                continue;

            size_t semi_pos = enemy_data.find(';');
            if (semi_pos == std::string::npos)
                continue;

            std::string stats = enemy_data.substr(0, semi_pos);
            std::string bullets_data = enemy_data.substr(semi_pos + 1);

            std::stringstream stats_ss(stats);
            std::vector<std::string> values;

            while (std::getline(stats_ss, value, ','))
            {
                values.push_back(value);
            }

            if (values.size() >= 7)
            {
                Enemy *enemy = new Enemy(
                    std::stod(values[0]),
                    std::stod(values[1]),
                    static_cast<SpriteType>(std::stoi(values[3])));
                enemy->direction = static_cast<Direction>(std::stoi(values[2]));
                enemy->lives_count = std::stoi(values[4]);
                if (values[5] == "1")
                    enemy->setFlag(TSF_DESTROYED);
                if (values[6] == "1")
                    enemy->setFlag(TSF_BONUS);

                enemy->forceActiveState();

                // Parse bullets count
                semi_pos = bullets_data.find(';');
                if (semi_pos != std::string::npos)
                {
                    int bullet_count = std::stoi(bullets_data.substr(0, semi_pos));
                    std::string bullets = bullets_data.substr(semi_pos + 1);

                    std::stringstream bullets_ss(bullets);
                    std::string bullet_data;

                    for (int j = 0; j < bullet_count &&
                                    std::getline(bullets_ss, bullet_data, '|');
                         j++)
                    {

                        std::stringstream bullet_ss(bullet_data);
                        std::vector<std::string> bullet_values;

                        while (std::getline(bullet_ss, value, ','))
                        {
                            bullet_values.push_back(value);
                        }

                        if (bullet_values.size() >= 5)
                        {
                            Bullet *bullet = new Bullet(
                                std::stod(bullet_values[0]),
                                std::stod(bullet_values[1]));
                            bullet->direction = static_cast<Direction>(std::stoi(bullet_values[2]));
                            bullet->collide = bullet_values[3] == "1";
                            bullet->increased_damage = bullet_values[4] == "1";
                            enemy->bullets.push_back(bullet);
                        }
                    }
                }
                m_enemies.push_back(enemy);
            }
        }

        // Parse bonuses section
        std::getline(ss, header, ':');
        if (header != "bonuses")
        {
            std::cout << "Invalid bonuses section" << std::endl;
            return;
        }

        // Get bonus count
        std::getline(ss, count_str, ':');
        int bonus_count = std::stoi(count_str);
        std::cout << "Bonuses count: " << bonus_count << std::endl;

        // Parse each bonus
        for (int i = 0; i < bonus_count; i++)
        {
            std::string bonus_data;
            std::getline(ss, bonus_data, '#');
            if (bonus_data.empty())
                continue;

            std::stringstream bonus_ss(bonus_data);
            std::vector<std::string> values;

            while (std::getline(bonus_ss, value, ','))
            {
                values.push_back(value);
            }

            if (values.size() >= 4)
            {
                Bonus *bonus = new Bonus(
                    std::stod(values[0]),
                    std::stod(values[1]),
                    static_cast<SpriteType>(std::stoi(values[2])));
                bonus->to_erase = !(values[3] == "1");
                m_bonuses.push_back(bonus);
            }
        }

        // Parse bricks section
        std::getline(ss, header, ':');
        if (header == "brick_sprites")
        {
            std::string brick_data;
            while (std::getline(ss, brick_data, '#'))
            {
                if (brick_data.empty())
                    continue;

                std::stringstream brick_ss(brick_data);
                std::vector<std::string> values;
                std::string value;

                while (std::getline(brick_ss, value, ','))
                {
                    values.push_back(value);
                }

                if (values.size() >= 6)
                {
                    int row = std::stoi(values[0]);
                    int col = std::stoi(values[1]);

                    if (m_level[row][col] != nullptr &&
                        m_level[row][col]->type == ST_BRICK_WALL)
                    {
                        m_level[row][col]->src_rect = {
                            std::stoi(values[2]),
                            std::stoi(values[3]),
                            std::stoi(values[4]),
                            std::stoi(values[5])};
                    }
                }
            }
        }

        std::cout << "Game state updated successfully" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cout << "Error parsing game state: " << e.what() << std::endl;
    }
}

void GameOnline::HandleClientData()
{
    // Khởi tạo Winsock
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    // Tạo socket TCP
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    // Cấu hình địa chỉ server
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8888);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Lấy trạng thái input
    const Uint8 *keystate = SDL_GetKeyboardState(NULL);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == 0)
    {
        std::stringstream ss;
        ss << "client_data:" << m_room_code << ":";
        ss << (keystate[SDL_SCANCODE_UP] ? "1" : "0") << ",";
        ss << (keystate[SDL_SCANCODE_DOWN] ? "1" : "0") << ",";
        ss << (keystate[SDL_SCANCODE_LEFT] ? "1" : "0") << ",";
        ss << (keystate[SDL_SCANCODE_RIGHT] ? "1" : "0") << ",";
        ss << (keystate[SDL_SCANCODE_RCTRL] ? "1" : "0");

        std::string data = ss.str();
        send(sock, data.c_str(), data.length(), 0);

        // Nhận dữ liệu game state từ server
        char buffer[2000] = {0};
        if (recv(sock, buffer, sizeof(buffer), 0) > 0)
        {
            std::string response(buffer);
            if (response.find("game_state:") == 0)
            {
                std::cout << "Response: |" << response.length() << std::endl;
                RecvGameState(response);
            }
        }
    }

    closesocket(sock);
    WSACleanup();
}

void GameOnline::HandleHostData()
{
    // Khởi tạo Winsock
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    // Tạo socket TCP
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    // Cấu hình địa chỉ server
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8888);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Kết nối đến server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == 0)
    {
        // Đóng gói dữ liệu player
        std::stringstream ss;

        std::string data = GameStateSendData();

        // Gửi dữ liệu
        send(sock, data.c_str(), data.length(), 0);

        // Nhận phản hồi từ server
        char buffer[1024] = {0};
        if (recv(sock, buffer, sizeof(buffer), 0) > 0)
        {
            std::string response(buffer);
            std::stringstream ss(response);

            // std::cout << "Raw: |" <<response<< std::endl;

            std::string header;
            std::getline(ss, header, ':');

            // std::cout << "Raw response: |" << header << "|" << response << std::endl;

            if (header == "host_response")
            {
                std::string input_data;
                std::getline(ss, input_data);
                std::vector<bool> inputs;
                std::stringstream input_ss(input_data);
                std::string value;

                while (std::getline(input_ss, value, ','))
                {
                    inputs.push_back(value == "1");
                }

                if (inputs.size() >= 5)
                {
                    m_client_input.up = inputs[0];
                    m_client_input.down = inputs[1];
                    m_client_input.left = inputs[2];
                    m_client_input.right = inputs[3];
                    m_client_input.fire = inputs[4];

                    // std::cout << "Parsed inputs: "
                    //  << inputs[0] << ","
                    //  << inputs[1] << ","
                    //  << inputs[2] << ","
                    //  << inputs[3] << ","
                    //  << inputs[4] << std::endl;
                }
            }
        }
    }

    // Đóng socket và cleanup
    closesocket(sock);
    WSACleanup();
}

void GameOnline::checkConnect()
{
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8888);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == 0)
    {
        std::stringstream ss;
        ss << "game_data:" << m_room_code << ":"
           << (m_is_host ? "host" : "client") << ":"
           << m_players.size();
        std::string request = ss.str();

        send(sock, request.c_str(), request.length(), 0);

        char buffer[1024] = {0};
        recv(sock, buffer, sizeof(buffer), 0);
        std::string response(buffer);
    }

    closesocket(sock);
    WSACleanup();
}

void GameOnline::ClientUpdate(Uint32 dt)
{
    // kiểm tra va chạm của quả bóng với tường
    for (auto enemy : m_enemies)
        for (auto bullet : enemy->bullets)
            checkCollisionBulletWithLevel(bullet);
    for (auto player : m_players)
        for (auto bullet : player->bullets)
        {
            checkCollisionBulletWithLevel(bullet);
            checkCollisionBulletWithBush(bullet);
        }
    // Cập nhật tất cả các đối tượng
    for (auto enemy : m_enemies)
    {
        // Force enemy to active state for replay
        enemy->spawnAt(enemy->pos_x, enemy->pos_y, enemy->direction, true);
        enemy->updateOnline(dt);
    }

    for (auto player : m_players)
    {
        player->updateWithNoInput(dt);
    }

    for (auto bonus : m_bonuses)
        bonus->update(dt);
    m_eagle->update(dt);

    for (auto bush : m_bushes)
        bush->update(dt);
}

void GameOnline::HostUpdate(Uint32 dt)
{
    if (m_pause)
        return;

    updateCollider(dt);

    // ấn định mục tiêu của đối thủ
    int min_metric; // 2 * 26 * 16
    int metric;
    SDL_Point target;
    for (auto enemy : m_enemies)
    {
        min_metric = 832;
        if (enemy->type == ST_TANK_A || enemy->type == ST_TANK_D)
            for (auto player : m_players)
            {
                metric = fabs(player->dest_rect.x - enemy->dest_rect.x) + fabs(player->dest_rect.y - enemy->dest_rect.y);
                if (metric < min_metric)
                {
                    min_metric = metric;
                    target = {player->dest_rect.x + player->dest_rect.w / 2, player->dest_rect.y + player->dest_rect.h / 2};
                }
            }
        metric = fabs(m_eagle->dest_rect.x - enemy->dest_rect.x) + fabs(m_eagle->dest_rect.y - enemy->dest_rect.y);
        if (metric < min_metric)
        {
            min_metric = metric;
            target = {m_eagle->dest_rect.x + m_eagle->dest_rect.w / 2, m_eagle->dest_rect.y + m_eagle->dest_rect.h / 2};
        }

        enemy->target_position = target;
    }

    // Cập nhật tất cả các đối tượng
    for (auto enemy : m_enemies)
        enemy->update(dt);
    for (auto player : m_players)
    {
        if (player->player_keys == AppConfig::player_keys.at(1))
            player->updateWithInput(dt, m_client_input.up, m_client_input.down, m_client_input.left, m_client_input.right, m_client_input.fire);
        else
            player->update(dt);
    }

    for (auto bonus : m_bonuses)
        bonus->update(dt);
    m_eagle->update(dt);

    for (auto row : m_level)
        for (auto item : row)
            if (item != nullptr)
                item->update(dt);

    for (auto bush : m_bushes)
        bush->update(dt);

    // loại bỏ các phần tử không cần thiết
    m_enemies.erase(std::remove_if(m_enemies.begin(), m_enemies.end(), [](Enemy *e)
                                   {if(e->to_erase) {delete e; return true;} return false; }),
                    m_enemies.end());
    m_players.erase(std::remove_if(m_players.begin(), m_players.end(), [this](Player *p)
                                   {if(p->to_erase) {m_killed_players.push_back(p); return true;} return false; }),
                    m_players.end());
    m_bonuses.erase(std::remove_if(m_bonuses.begin(), m_bonuses.end(), [](Bonus *b)
                                   {if(b->to_erase) {delete b; return true;} return false; }),
                    m_bonuses.end());
    m_bushes.erase(std::remove_if(m_bushes.begin(), m_bushes.end(), [](Object *b)
                                  {if(b->to_erase) {delete b; return true;} return false; }),
                   m_bushes.end());

    // thêm đối thủ mới
    m_enemy_redy_time += dt;
    if (m_enemies.size() < (AppConfig::enemy_max_count_on_map < m_enemy_to_kill ? AppConfig::enemy_max_count_on_map : m_enemy_to_kill) && m_enemy_redy_time > AppConfig::enemy_redy_time)
    {
        m_enemy_redy_time = 0;
        generateEnemy();
    }

    if (m_enemies.empty() && m_enemy_to_kill <= 0)
    {
        m_level_end_time += dt;
        if (m_level_end_time > AppConfig::level_end_time)
            m_finished = true;
    }

    if (m_players.empty() && !m_game_over)
    {
        m_eagle->destroy();
        m_game_over_position = AppConfig::map_rect.h;
        m_game_over = true;
    }

    if (m_game_over)
    {
        if (m_game_over_position < 10)
            m_finished = true;
        else
            m_game_over_position -= AppConfig::game_over_entry_speed * dt;
    }

    if (m_protect_eagle)
    {
        m_protect_eagle_time += dt;
        if (m_protect_eagle_time > AppConfig::protect_eagle_time)
        {
            m_protect_eagle = false;
            m_protect_eagle_time = 0;
            for (int i = 0; i < 3; i++)
            {
                if (m_level.at(m_level_rows_count - i - 1).at(11) != nullptr)
                    delete m_level.at(m_level_rows_count - i - 1).at(11);
                m_level.at(m_level_rows_count - i - 1).at(11) = new Brick(11 * AppConfig::tile_rect.w, (m_level_rows_count - i - 1) * AppConfig::tile_rect.h);

                if (m_level.at(m_level_rows_count - i - 1).at(14) != nullptr)
                    delete m_level.at(m_level_rows_count - i - 1).at(14);
                m_level.at(m_level_rows_count - i - 1).at(14) = new Brick(14 * AppConfig::tile_rect.w, (m_level_rows_count - i - 1) * AppConfig::tile_rect.h);
            }
            for (int i = 12; i < 14; i++)
            {
                if (m_level.at(m_level_rows_count - 3).at(i) != nullptr)
                    delete m_level.at(m_level_rows_count - 3).at(i);
                m_level.at(m_level_rows_count - 3).at(i) = new Brick(i * AppConfig::tile_rect.w, (m_level_rows_count - 3) * AppConfig::tile_rect.h);
            }
        }

        if (m_protect_eagle && m_protect_eagle_time > AppConfig::protect_eagle_time / 4 * 3 && m_protect_eagle_time / AppConfig::bonus_blink_time % 2)
        {
            for (int i = 0; i < 3; i++)
            {
                if (m_level.at(m_level_rows_count - i - 1).at(11) != nullptr)
                    delete m_level.at(m_level_rows_count - i - 1).at(11);
                m_level.at(m_level_rows_count - i - 1).at(11) = new Brick(11 * AppConfig::tile_rect.w, (m_level_rows_count - i - 1) * AppConfig::tile_rect.h);

                if (m_level.at(m_level_rows_count - i - 1).at(14) != nullptr)
                    delete m_level.at(m_level_rows_count - i - 1).at(14);
                m_level.at(m_level_rows_count - i - 1).at(14) = new Brick(14 * AppConfig::tile_rect.w, (m_level_rows_count - i - 1) * AppConfig::tile_rect.h);
            }
            for (int i = 12; i < 14; i++)
            {
                if (m_level.at(m_level_rows_count - 3).at(i) != nullptr)
                    delete m_level.at(m_level_rows_count - 3).at(i);
                m_level.at(m_level_rows_count - 3).at(i) = new Brick(i * AppConfig::tile_rect.w, (m_level_rows_count - 3) * AppConfig::tile_rect.h);
            }
        }
        else if (m_protect_eagle)
        {
            for (int i = 0; i < 3; i++)
            {
                if (m_level.at(m_level_rows_count - i - 1).at(11) != nullptr)
                    delete m_level.at(m_level_rows_count - i - 1).at(11);
                m_level.at(m_level_rows_count - i - 1).at(11) = new Object(11 * AppConfig::tile_rect.w, (m_level_rows_count - i - 1) * AppConfig::tile_rect.h, ST_STONE_WALL);

                if (m_level.at(m_level_rows_count - i - 1).at(14) != nullptr)
                    delete m_level.at(m_level_rows_count - i - 1).at(14);
                m_level.at(m_level_rows_count - i - 1).at(14) = new Object(14 * AppConfig::tile_rect.w, (m_level_rows_count - i - 1) * AppConfig::tile_rect.h, ST_STONE_WALL);
            }
            for (int i = 12; i < 14; i++)
            {
                if (m_level.at(m_level_rows_count - 3).at(i) != nullptr)
                    delete m_level.at(m_level_rows_count - 3).at(i);
                m_level.at(m_level_rows_count - 3).at(i) = new Object(i * AppConfig::tile_rect.w, (m_level_rows_count - 3) * AppConfig::tile_rect.h, ST_STONE_WALL);
            }
        }
    }
}

void GameOnline::update(Uint32 dt)
{
    if (dt > 40)
        return;

    if (m_level_start_screen)
    {
        if (m_level_start_time > AppConfig::level_start_time)
            m_level_start_screen = false;

        m_level_start_time += dt;
    }
    else if (m_is_host)
    {
        HostUpdate(dt);
        HandleHostData();
    }
    else if (!m_is_host)
    {
        HandleClientData();
        ClientUpdate(dt);
    }
}