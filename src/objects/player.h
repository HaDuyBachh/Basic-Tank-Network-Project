#ifndef PLAYER_H
#define PLAYER_H

#include "tank.h"

/**
 * @brief Một lớp tương ứng với xe tăng của người chơi.
 */
class Player : public Tank
{
public:

    /**
     * @brief Một cấu trúc chứa các phím tương ứng với việc điều khiển xe tăng của người chơi.
     */
    struct PlayerKeys
    {
        PlayerKeys(): up(SDL_SCANCODE_UNKNOWN), down(SDL_SCANCODE_UNKNOWN), left(SDL_SCANCODE_UNKNOWN), right(SDL_SCANCODE_UNKNOWN), fire(SDL_SCANCODE_UNKNOWN) {}
        PlayerKeys(SDL_Scancode u, SDL_Scancode d, SDL_Scancode l, SDL_Scancode r, SDL_Scancode f): up(u), down(d), left(l), right(r), fire(f) {}
        /**
         * Chìa khóa tương ứng với việc lái xe lên.
         */
        SDL_Scancode up;
        /**
         * Phím xuống.
         */
        SDL_Scancode down;
        /**
         * key tương ứng với việc lái xe sang trái.
         */
        SDL_Scancode left;
        /**
         * key tương ứng với việc lái xe bên phải.
         */
        SDL_Scancode right;
        /**
         * key tương ứng với việc bắn tên lửa.
         */
        SDL_Scancode fire;
    };

    /**
     * Tạo người chơi ở vị trí người chơi đầu tiên.
     * @see AppConfig::player_starting_point
     */
    Player();
    /**
     * Tạo xe tăng của người chơi
     * @param x - vị trí bắt đầu hoành độ
     * @param y - vị trí bắt đầu theo tung độ
     * @param type - kiểu người chơi
     */
    Player(double x, double y, SpriteType type);


    /**
     * Chức năng này chịu trách nhiệm thay đổi hình ảnh động của xe tăng của người chơi và kiểm tra trạng thái của các phím được nhấn cũng như phản ứng với các phím điều khiển xe tăng của người chơi.
     * @param dt - thời gian kể từ lần gọi hàm cuối cùng, được sử dụng khi thay đổi hoạt ảnh
     */
    void update(Uint32 dt);
    /**
     * Chức năng có nhiệm vụ trừ máu, xóa hết cờ và bật hoạt ảnh đội hình xe tăng.
     */
    void respawn();
    /**
     * Chức năng này có nhiệm vụ bật hoạt ảnh nổ xe tăng nếu xe tăng không có nắp, thuyền hoặc ba ngôi sao.
     */
    void destroy();
    /**
     * Hàm này chịu trách nhiệm tạo một đường đạn nếu số lượng tối đa của chúng chưa được tạo,
     * tăng tốc độ hơn nếu người chơi có ít nhất một sao và tăng thêm sát thương nếu người chơi có ba sao.
     * @return con trỏ tới đường đạn đã tạo, nếu không có đường đạn nào được tạo sẽ trả về @a nullptr
     */
    Bullet* fire();

    /**
     * Chức năng thay đổi số sao bạn hiện có. Khi số sao khác 0, tốc độ mặc định của xe tăng sẽ tăng lên,
     * a cho số lượng sao lớn hơn 1 và với mỗi @a c dương thì số lượng đạn tối đa được tăng lên.
     * @param c - sự thay đổi số lượng sao có thể âm
     */
    void changeStarCountBy(int c);

    /**
     * Các phím điều khiển chuyển động của người chơi hiện tại.
     */
    PlayerKeys player_keys;
    /**
     * Điểm hiện tại của người chơi.
     */
    unsigned score;

private:
    /**
     * Số lượng sao hiện tại; có thể nằm trong khoảng [0, 3].
     */
    int star_count;
    /**
     * Thời gian đã trôi qua kể từ lần bắn tên lửa cuối cùng.
     */
    Uint32 m_fire_time;
};

#endif // PLAYER_H
