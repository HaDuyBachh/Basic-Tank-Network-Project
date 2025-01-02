#ifndef APPSTATE_H
#define APPSTATE_H

#include <SDL2/SDL_events.h>
#include <string>

/**
 * @brief
 * Klasa jest interfejsem, po którym dziedziczą klasy @a Game, @a Menu, @a Scores
 */
class AppState
{
public:
    virtual ~AppState() {}

    /**
     * Funkcja sprawdza czy aktualny stan gry się skończył.
     * @return @a true jeżeli obecny stan gry się skończył, w przeciwnym wypadku @a false.
     */
    virtual bool finished() const = 0;
    /**
     * Một hàm vẽ các phần tử trò chơi thuộc một trạng thái nhất định
     */
    virtual void draw() = 0;
    /**
     * Chức năng cập nhật trạng thái của đồ vật và bộ đếm trong game
     * @param dt - thời gian kể từ lần gọi hàm cuối cùng tính bằng mili giây
     */
    virtual void update(Uint32 dt) = 0;
    /**
     * Funkcja umożliwiająca obsługę zdarzeń wykrywanych przez bibliotekę SDL2.
     * @param ev - wskaźnik na unię SDL_Event przechowującą typ i parametry różnych zdarzeń
     */
    virtual void eventProcess(SDL_Event *ev) = 0;
    /**
     * Hàm trả về trạng thái tiếp theo sau khi trạng thái hiện tại kết thúc. Hàm chỉ nên được gọi nếu @a hoàn thành trả về @a true.
     *     @return lưu trò chơi tiếp theo
     */
    virtual AppState *nextState() = 0;
};
#endif // APPSTATE_H
