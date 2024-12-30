#ifndef GAME_H
#define GAME_H
#include "appstate.h"
#include "../objects/object.h"
#include "../objects/player.h"
#include "../objects/enemy.h"
#include "../objects/bullet.h"
#include "../objects/brick.h"
#include "../objects/eagle.h"
#include "../objects/bonus.h"
#include <vector>
#include <string>

/**
 * @brief Lớp này chịu trách nhiệm cho việc di chuyển của tất cả xe tăng và tương tác giữa các xe tăng với nhau cũng như giữa xe tăng với các đối tượng khác trên bản đồ
 */
class Game : public AppState
{
public:
    /**
     * Constructor mặc định - cho phép chơi đơn người
     */
    Game();
    /**
     * Constructor cho phép chỉ định số lượng người chơi ban đầu. Số người chơi có thể là 1 hoặc 2, các giá trị khác sẽ khởi động trò chơi ở chế độ một người chơi.
     * Constructor này được gọi trong @a Menu::nextState.
     * @param players_count - số lượng người chơi 1 hoặc 2
     */
    Game(int players_count);
    /**
     * Constructor nhận các người chơi đã tồn tại.
     * Được gọi trong @a Score::nextState
     * @param players - container chứa người chơi
     * @param previous_level - biến lưu trữ số cấp độ trước đó
     */
    Game(std::vector<Player *> players, int previous_level);

    ~Game();
    /**
     * Hàm trả về @a true nếu người chơi đã tiêu diệt tất cả kẻ thù hoặc khi đại bàng bị bắn trúng hoặc người chơi mất hết mạng, tức là đã thua.
     * @return @a true hoặc @a false
     */
    bool finished() const;
    /**
     * Hàm hiển thị số vòng chơi ở đầu mỗi vòng. Trong quá trình chơi, hàm chịu trách nhiệm vẽ cấp độ (gạch, đá, nước, băng, bụi cây),
     * người chơi, kẻ thù, phần thưởng, đại bàng, trạng thái trò chơi trên bảng điều khiển bên phải (kẻ thù còn lại, mạng còn lại của người chơi, số vòng).
     * Sau khi thua hoặc trong khi tạm dừng, hiển thị thông tin tương ứng ở giữa màn hình.
     */
    void draw();
    /**
     * Hàm cập nhật trạng thái của tất cả các đối tượng trên bản đồ (xe tăng, phần thưởng, chướng ngại vật). Ngoài ra còn kiểm tra va chạm giữa các xe tăng, giữa xe tăng và các thành phần cấp độ cũng như giữa đạn với xe tăng và các thành phần bản đồ.
     * Tại đây diễn ra việc xóa các đối tượng bị phá hủy, thêm xe tăng địch mới và kiểm tra điều kiện kết thúc vòng.
     * @param dt - thời gian kể từ lần gọi hàm trước tính bằng mili giây
     */
    void update(Uint32 dt);
    /**
     * Tại đây xử lý phản ứng với các phím:
     * @li Enter - tạm dừng trò chơi
     * @li Esc - trở về menu
     * @li N - chuyển sang vòng tiếp theo, nếu trò chơi chưa thua
     * @li B - trở lại vòng trước, nếu trò chơi chưa thua
     * @li T - hiển thị đường đi đến mục tiêu của xe tăng địch
     * @param ev - con trỏ đến union SDL_Event lưu trữ loại và tham số của các sự kiện khác nhau, bao gồm cả sự kiện bàn phím
     */
    void eventProcess(SDL_Event *ev);
    /**
     * Chuyển sang trạng thái tiếp theo.
     * @return con trỏ đến đối tượng lớp @a Scores nếu người chơi vượt qua vòng hoặc thua. Nếu người chơi nhấn Esc hàm trả về con trỏ đến đối tượng @a Menu.
     */
    AppState *nextState();

    void updateCollider(Uint32 dt);

    // ... [Các getter và setter giữ nguyên không đổi]

protected:
    /**
     * Tải bản đồ cấp độ từ tệp
     * @param path - đường dẫn đến tệp bản đồ
     */
    void loadLevel(std::string path);
    /**
     * Xóa các kẻ thù còn lại, người chơi, đối tượng bản đồ và phần thưởng
     */
    void clearLevel();
    /**
     * Tải cấp độ mới và tạo người chơi mới nếu chưa tồn tại.
     * @see Game::loadLevel(std::string path)
     */
    void nextLevel();
    /**
     * Tạo kẻ thù mới nếu số lượng đối thủ trên bản đồ ít hơn 4 với điều kiện chưa tạo đủ 20 kẻ thù trên bản đồ.
     * Hàm tạo ra các mức độ giáp khác nhau cho kẻ thù tùy thuộc vào cấp độ; càng cao số vòng chơi, càng có nhiều khả năng đối thủ có giáp cấp 4.
     * Số cấp độ giáp cho biết cần bắn trúng bao nhiêu lần để tiêu diệt đối thủ. Số này nhận giá trị từ 1 đến 4 và tương ứng với các màu xe tăng khác nhau.
     * Đối thủ được tạo ra có thêm cơ hội khi bị bắn trúng sẽ tạo ra phần thưởng trên bản đồ.
     */
    void generateEnemy();
    /**
     * Hàm tạo ngẫu nhiên phần thưởng trên bản đồ và đặt nó ở vị trí không va chạm với đại bàng.
     */
    void generateBonus();

    /**
     * Kiểm tra xem xe tăng có thể di chuyển tự do về phía trước không, nếu không thì dừng lại. Hàm không cho phép di chuyển ra ngoài bản đồ.
     * Nếu xe tăng đi vào băng sẽ gây ra hiệu ứng trượt. Nếu xe tăng có phần thưởng "Thuyền" có thể đi qua nước. Xe tăng không thể đi qua đại bàng.
     * @param tank - xe tăng cần kiểm tra va chạm
     * @param dt - thay đổi thời gian gần nhất, giả định với những thay đổi nhỏ trong các bước thời gian tiếp theo có thể dự đoán vị trí tiếp theo của xe tăng và phản ứng phù hợp.
     */
    void checkCollisionTankWithLevel(Tank *tank, Uint32 dt);
    /**
     * Kiểm tra xem có va chạm giữa các xe tăng được kiểm tra không, nếu có thì cả hai đều dừng lại.
     * @param tank1
     * @param tank2
     * @param dt
     */
    void checkCollisionTwoTanks(Tank *tank1, Tank *tank2, Uint32 dt);
    /**
     * Kiểm tra xem viên đạn đã chọn có va chạm với bất kỳ phần tử nào trên bản đồ không (nước và băng được bỏ qua). Nếu có thì đạn và đối tượng bị phá hủy.
     * Nếu bắn trúng đại bàng thì trò chơi kết thúc với thất bại.
     * @param bullet - viên đạn
     */
    void checkCollisionBulletWithLevel(Bullet *bullet);
    /**
     * Kiểm tra va chạm của đạn với bụi cây trên bản đồ. Bụi cây và đạn bị phá hủy chỉ khi đạn có sát thương tăng cường.
     * @param bullet - viên đạn
     * @see Bullet::increased_damage
     */
    void checkCollisionBulletWithBush(Bullet *bullet);
    /**
     * Kiểm tra xem người chơi đã bắn trúng kẻ thù được chọn chưa. Nếu có, người chơi nhận được điểm và kẻ thù mất một cấp độ giáp.
     * @param player - người chơi
     * @param enemy - kẻ thù
     */
    void checkCollisionPlayerBulletsWithEnemy(Player *player, Enemy *enemy);
    /**
     * Kiểm tra xem kẻ thù có bắn trúng người chơi không. Nếu có thì người chơi mất một mạng trừ khi có lá chắn bảo vệ.
     * @param enemy - kẻ thù
     * @param player - người chơi
     */
    void checkCollisionEnemyBulletsWithPlayer(Enemy *enemy, Player *player);
    /**
     * Nếu hai viên đạn va chạm vào nhau, cả hai đều bị phá hủy.
     * @param bullet1
     * @param bullet2
     */
    void checkCollisionTwoBullets(Bullet *bullet1, Bullet *bullet2);
    /**
     * Kiểm tra xem người chơi có nhặt được phần thưởng không. Nếu có thì xảy ra phản ứng tương ứng:
     * @li Lựu đạn - các kẻ thù đang hiển thị bị tiêu diệt
     * @li Mũ bảo hiểm - người chơi nhận được bảo vệ tạm thời khỏi mọi viên đạn
     * @li Đồng hồ - làm đứng yên các kẻ thù đang hiển thị
     * @li Xẻng - Tạo tường đá tạm thời xung quanh đại bàng
     * @li Xe tăng - tăng số mạng của người chơi thêm 1
     * @li Ngôi sao - nâng cấp xe tăng của người chơi (tăng tốc độ, số lượng đạn)
     * @li Vũ khí - nâng cấp tối đa cho người chơi
     * @li Thuyền - khả năng đi qua nước
     * Người chơi nhận được điểm thưởng thêm khi lấy được phần thưởng.
     * @param player
     * @param bonus
     */
    void checkCollisionPlayerWithBonus(Player *player, Bonus *bonus);

    /**
     * Số cột trong lưới bản đồ.
     */
    int m_level_columns_count;
    /**
     * Số hàng trong lưới bản đồ.
     */
    int m_level_rows_count;
    /**
     * Chướng ngại vật trên bản đồ.
     */
    std::vector<std::vector<Object *>> m_level;
    /**
     * Bụi cây trên bản đồ.
     */
    std::vector<Object *> m_bushes;

    /**
     * Tập hợp kẻ thù.
     */
    std::vector<Enemy *> m_enemies;
    /**
     * Tập hợp người chơi còn lại.
     */
    std::vector<Player *> m_players;
    /**
     * Tập hợp người chơi đã bị tiêu diệt.
     */
    std::vector<Player *> m_killed_players;
    /**
     * Tập hợp phần thưởng trên bản đồ.
     */
    std::vector<Bonus *> m_bonuses;
    /**
     * Đối tượng đại bàng.
     */
    Eagle *m_eagle;

    /**
     * Số cấp độ hiện tại.
     */
    int m_current_level;
    /**
     * Số người chơi trong chế độ chơi đã chọn 1 hoặc 2.
     */
    int m_player_count;
    /**
     * Số kẻ thù còn lại cần tiêu diệt ở cấp độ hiện tại
     */
    int m_enemy_to_kill;

    /**
     * Biến lưu trữ xem màn hình bắt đầu cấp độ có đang được hiển thị không.
     */
    bool m_level_start_screen;
    /**
     * Biến lưu trữ thông tin xem đại bàng có đang được bảo vệ bởi tường đá không.
     */
    bool m_protect_eagle;
    /**
     * Thời gian màn hình bắt đầu cấp độ đã được hiển thị.
     */
    Uint32 m_level_start_time;
    /**
     * Thời gian từ lần cuối tạo kẻ thù.
     */
    Uint32 m_enemy_redy_time;
    /**
     * Thời gian đã trôi qua kể từ khi thắng bản đồ.
     */
    Uint32 m_level_end_time;
    /**
     * Thời gian đại bàng đã được bảo vệ bởi tường đá.
     */
    Uint32 m_protect_eagle_time;

    /**
     * Trạng thái thua cuộc.
     */
    bool m_game_over;
    /**
     * Vị trí của dòng chữ "GAME OVER" nếu @a m_game_over là @a true.
     */
    double m_game_over_position;
    /**
     * Biến lưu trữ thông tin xem có nên kết thúc trạng thái trò chơi hiện tại và chuyển sang hiển thị điểm số hoặc menu trò chơi không.
     */
    bool m_finished;
    /**
     * Biến cho biết trò chơi có đang tạm dừng không.
     */
    bool m_pause;
    /**
     * Số vị trí tạo kẻ thù mới. Thay đổi mỗi khi tạo kẻ thù.
     */
    int m_enemy_respown_position;

};



#endif // GAME_H