#include "enemy.h"
#include "../appconfig.h"
#include <stdlib.h>
#include <ctime>
#include <iostream>

Enemy::Enemy()
    : Tank(AppConfig::enemy_starting_point.at(0).x, AppConfig::enemy_starting_point.at(0).y, ST_TANK_A)
{
    direction = D_DOWN;
    m_direction_time = 0;
    m_keep_direction_time = 100;

    m_speed_time = 0;
    m_try_to_go_time = 100;

    m_fire_time = 0;
    m_reload_time = 100;
    lives_count = 1;

    m_bullet_max_size = 1;

    m_frozen_time = 0;

    // Tank B là tank nhanh nhất
    if (type == ST_TANK_B)
        default_speed = AppConfig::tank_default_speed * 1.3;
    else
        default_speed = AppConfig::tank_default_speed;

    target_position = {-1, -1};

    respawn();
}

Enemy::Enemy(double x, double y, SpriteType type)
    : Tank(x, y, type)
{
    direction = D_DOWN;
    m_direction_time = 0;
    m_keep_direction_time = 100;

    m_speed_time = 0;
    m_try_to_go_time = 100;

    m_fire_time = 0;
    m_reload_time = 100;
    lives_count = 1;

    m_bullet_max_size = 1;

    m_frozen_time = 0;

    if (type == ST_TANK_B)
        default_speed = AppConfig::tank_default_speed * 1.3;
    else
        default_speed = AppConfig::tank_default_speed;

    target_position = {-1, -1};

    respawn();
}

void Enemy::spawnAt(double x, double y, Direction dir, bool isActive)
{
    pos_x = x;
    pos_y = y;
    direction = dir;

    if (isActive)
    {
        // Skip spawn animation, directly set active state
        m_sprite = Engine::getEngine().getSpriteConfig()->getSpriteData(type);
        clearFlag(TSF_CREATE);
        setFlag(TSF_LIFE);
        m_current_frame = 0;
    }
    else
    {
        // Normal spawn with animation
        respawn();
    }
}

// active nhanh 1 enemy mà không khởi tạo hiệu ứng
void Enemy::forceActiveState()
{
    if (!testFlag(TSF_LIFE))
    {
        m_sprite = Engine::getEngine().getSpriteConfig()->getSpriteData(type);
        clearFlag(TSF_CREATE);
        setFlag(TSF_LIFE);
        m_current_frame = 0;
    }
}

void Enemy::draw()
{
    if (to_erase)
        return;
    if (AppConfig::show_enemy_target)
    {
        SDL_Color c;
        if (type == ST_TANK_A)
            c = {250, 0, 0, 250};
        if (type == ST_TANK_B)
            c = {0, 0, 250, 255};
        if (type == ST_TANK_C)
            c = {0, 255, 0, 250};
        if (type == ST_TANK_D)
            c = {250, 0, 255, 250};
        SDL_Rect r = {min(target_position.x, dest_rect.x + dest_rect.w / 2), dest_rect.y + dest_rect.h / 2, abs(target_position.x - (dest_rect.x + dest_rect.w / 2)), 1};
        Engine::getEngine().getRenderer()->drawRect(&r, c, true);
        r = {target_position.x, min(target_position.y, dest_rect.y + dest_rect.h / 2), 1, abs(target_position.y - (dest_rect.y + dest_rect.h / 2))};
        Engine::getEngine().getRenderer()->drawRect(&r, c, true);
    }
    Tank::draw();
}

void Enemy::updateOnline(Uint32 dt)
{
    if (to_erase)
        return;

    // Update base state
    Tank::update(dt);

    // Update animation frame
    if (testFlag(TSF_LIFE))
    {
        if (testFlag(TSF_BONUS))
            src_rect = moveRect(m_sprite->rect, (testFlag(TSF_ON_ICE) ? new_direction : direction) - 4, m_current_frame);
        else
            src_rect = moveRect(m_sprite->rect, (testFlag(TSF_ON_ICE) ? new_direction : direction) + (lives_count - 1) * 4, m_current_frame);
    }
    else
    {
        src_rect = moveRect(m_sprite->rect, 0, m_current_frame);
    }

    if (testFlag(TSF_FROZEN))
        return;
}

void Enemy::update(Uint32 dt)
{
    if (to_erase)
        return;

    /// Tank update chỉ xử lí hình ảnh và hiệu ứng
    Tank::update(dt);

    // Kiểm tra coi đã sinh ra tank hay chưa
    if (testFlag(TSF_LIFE))
    {
        if (testFlag(TSF_BONUS))
            src_rect = moveRect(m_sprite->rect, (testFlag(TSF_ON_ICE) ? new_direction : direction) - 4, m_current_frame);
        else
            src_rect = moveRect(m_sprite->rect, (testFlag(TSF_ON_ICE) ? new_direction : direction) + (lives_count - 1) * 4, m_current_frame);
    }
    else
        src_rect = moveRect(m_sprite->rect, 0, m_current_frame);

    // Bị đóng băng thì k update nữa
    if (testFlag(TSF_FROZEN))
        return;

    m_direction_time += dt;
    m_speed_time += dt;
    m_fire_time += dt;

    /// Kiểm tra thời gian đổi hướng của AI Game
    if (m_direction_time > m_keep_direction_time)
    {
        //Thời gian để đổi hướng 
        m_direction_time = 0;

        ///Thời gian giữ hướng
        m_keep_direction_time = rand() % 800 + 100;

        // P là biến quản lí cơ hội đi được đén mục tiêu
        float p = static_cast<float>(rand()) / RAND_MAX;

        // Tank loại A có 80% cơ hội đi về phía target
        // Tank khác có 50% cơ hội đi về phía target
        if (p < (type == ST_TANK_A ? 0.8 : 0.5) && target_position.x > 0 && target_position.y > 0)
        {
            // Tính khoảng cách đến mục tiêu theo x và y
            int dx = target_position.x - (dest_rect.x + dest_rect.w / 2);
            int dy = target_position.y - (dest_rect.y + dest_rect.h / 2);

            // Nếu khoảng cách x lớn hơn y
            p = static_cast<float>(rand()) / RAND_MAX;

            // 70% đi theo trục x, 30% đi theo trục y
            if (abs(dx) > abs(dy))
                setDirection(p < 0.7 ? (dx < 0 ? D_LEFT : D_RIGHT) : (dy < 0 ? D_UP : D_DOWN));
            else
                // 70% đi theo trục y, 30% đi theo trục x
                setDirection(p < 0.7 ? (dy < 0 ? D_UP : D_DOWN) : (dx < 0 ? D_LEFT : D_RIGHT));
        }
        else
            // 70% đi theo trục y, 30% đi theo trục x
            setDirection(static_cast<Direction>(rand() % 4));
    }

    /// Kiểm tra di chuyển
    if (m_speed_time > m_try_to_go_time)
    {
        m_speed_time = 0;
        m_try_to_go_time = rand() % 300;
        speed = default_speed;
    }

    /// Kiểm tra bắn đạn
    if (m_fire_time > m_reload_time)
    {
        m_fire_time = 0;

        // Tank D (tốc độ bắn cao nhất)
        if (type == ST_TANK_D)
        {
            m_reload_time = rand() % 400; // Tốc độ bắn nhanh nhất
            int dx = target_position.x - (dest_rect.x + dest_rect.w / 2);
            int dy = target_position.y - (dest_rect.y + dest_rect.h / 2);

            // Chỉ bắn khi:
            // - Tank đứng yên HOẶC
            // - Target nằm thẳng hàng với hướng tank đang nhìn
            if (stop)
                fire();
            else
                switch (direction)
                {
                case D_UP:
                    if (dy < 0 && abs(dx) < dest_rect.w)
                        fire();
                    break;
                case D_RIGHT:
                    if (dx > 0 && abs(dy) < dest_rect.h)
                        fire();
                    break;
                case D_DOWN:
                    if (dy > 0 && abs(dx) < dest_rect.w)
                        fire();
                    break;
                case D_LEFT:
                    if (dx < 0 && abs(dy) < dest_rect.h)
                        fire();
                    break;
                }
        }
         // Tank C: Bắn ngẫu nhiên nhưng nhanh 
        else if (type == ST_TANK_C)
        {
            m_reload_time = rand() % 800;
            fire();
        }
        // Tank A,B: Bắn chậm và ngẫu nhiên
        else
        {
            m_reload_time = rand() % 1000;
            fire();
        }
    }

    stop = false;
}

void Enemy::destroy()
{
    lives_count--;
    //    clearFlag(TSF_BONUS); //możliwe jednokrotne wypadnięcie bonusu
    if (lives_count <= 0)
    {
        lives_count = 0;
        Tank::destroy();
    }
}

unsigned Enemy::scoreForHit()
{
    if (lives_count > 0)
        return 50;
    return 100;
}
