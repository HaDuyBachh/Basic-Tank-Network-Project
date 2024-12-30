#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <vector>
#include <winsock2.h>
#include <pthread.h>
#include <map>
#include <algorithm>
#include <windows.h>
#include <direct.h>
#include <sstream>

#pragma comment(lib, "ws2_32.lib")

#define TCP_PORT 8888
#define UDP_PORT 8889
#define BUFFER_SIZE 1024

// Thêm vào đầu file sau các includes
struct EnemyData
{
    double pos_x;
    double pos_y;
    int direction;
    int type;
    int lives_count;
    bool is_destroyed;
    bool has_bonus;
};

struct BonusData
{
    double pos_x;
    double pos_y;
    int type;
    bool is_active;
};

struct ClientData
{
    SOCKET client_socket;
    struct sockaddr_in client_addr;
};

struct RoomInfo
{
    std::string room_code;
    std::vector<std::string> players;
    std::string host;
    bool is_playing;

    RoomInfo() : is_playing(false) {}
    std::vector<struct sockaddr_in> player_addrs;
};

struct PlayerInfo
{
    std::string username;
    bool is_authenticated;
    struct sockaddr_in udp_addr;
    std::string current_room;
    bool is_host;

    PlayerInfo() : is_authenticated(false), is_host(false) {}
};

// Thêm struct để lưu trữ game state
struct GameState
{
    std::vector<EnemyData> enemies;
    std::vector<BonusData> bonuses;
    bool eagle_destroyed;
    bool protect_eagle;
    int current_level;
    int enemy_to_kill;
};

// Maps toàn cục
std::map<std::string, PlayerInfo> players;
std::map<std::string, RoomInfo> rooms;
pthread_mutex_t players_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t rooms_mutex = PTHREAD_MUTEX_INITIALIZER;
// Map để lưu trữ game state cho mỗi phòng
std::map<std::string, GameState> game_states;
pthread_mutex_t game_states_mutex = PTHREAD_MUTEX_INITIALIZER;

// Hàm tạo và ghi file room
void writeRoomFile(const std::string &room_code, const std::string &host, const std::vector<std::string> &players)
{
    CreateDirectory("rooms", NULL);
    std::string filename = "rooms/room_" + room_code + ".txt";
    std::ofstream room_file(filename);
    if (room_file.is_open())
    {
        room_file << "=== Room Information ===" << std::endl;
        room_file << "Room Code: " << room_code << std::endl;
        room_file << "Host: " << host << std::endl;
        room_file << "=== Players List ===" << std::endl;
        int player_count = 1;
        for (const auto &player : players)
        {
            room_file << "Player " << player_count++ << ": " << player << std::endl;
        }
        room_file << "=== End of File ===" << std::endl;
        room_file.close();
    }
}

std::string serializeGameState(const GameState &state)
{
    std::stringstream ss;

    // Serialize enemies
    ss << state.enemies.size() << ";";
    for (const auto &enemy : state.enemies)
    {
        ss << enemy.pos_x << "," << enemy.pos_y << ","
           << enemy.direction << "," << enemy.type << ","
           << enemy.lives_count << "," << enemy.is_destroyed << ","
           << enemy.has_bonus << ";";
    }

    // Serialize bonuses
    ss << state.bonuses.size() << ";";
    for (const auto &bonus : state.bonuses)
    {
        ss << bonus.pos_x << "," << bonus.pos_y << ","
           << bonus.type << "," << bonus.is_active << ";";
    }

    ss << state.eagle_destroyed << ";"
       << state.protect_eagle << ";"
       << state.current_level << ";"
       << state.enemy_to_kill;

    return ss.str();
}

GameState deserializeGameState(const std::string &data)
{
    GameState state;
    std::stringstream ss(data);
    std::string segment;

    // Parse enemies
    std::getline(ss, segment, ';');
    int enemy_count = std::stoi(segment);
    for (int i = 0; i < enemy_count; i++)
    {
        std::getline(ss, segment, ';');
        std::stringstream enemy_ss(segment);
        std::string value;
        EnemyData enemy;

        std::getline(enemy_ss, value, ',');
        enemy.pos_x = std::stod(value);
        std::getline(enemy_ss, value, ',');
        enemy.pos_y = std::stod(value);
        std::getline(enemy_ss, value, ',');
        enemy.direction = std::stoi(value);
        std::getline(enemy_ss, value, ',');
        enemy.type = std::stoi(value);
        std::getline(enemy_ss, value, ',');
        enemy.lives_count = std::stoi(value);
        std::getline(enemy_ss, value, ',');
        enemy.is_destroyed = std::stoi(value);
        std::getline(enemy_ss, value, ',');
        enemy.has_bonus = std::stoi(value);

        state.enemies.push_back(enemy);
    }

    // Parse bonuses
    std::getline(ss, segment, ';');
    int bonus_count = std::stoi(segment);
    for (int i = 0; i < bonus_count; i++)
    {
        std::getline(ss, segment, ';');
        std::stringstream bonus_ss(segment);
        std::string value;
        BonusData bonus;

        std::getline(bonus_ss, value, ',');
        bonus.pos_x = std::stod(value);
        std::getline(bonus_ss, value, ',');
        bonus.pos_y = std::stod(value);
        std::getline(bonus_ss, value, ',');
        bonus.type = std::stoi(value);
        std::getline(bonus_ss, value, ',');
        bonus.is_active = std::stoi(value);

        state.bonuses.push_back(bonus);
    }

    // Parse other game states
    std::getline(ss, segment, ';');
    state.eagle_destroyed = std::stoi(segment);
    std::getline(ss, segment, ';');
    state.protect_eagle = std::stoi(segment);
    std::getline(ss, segment, ';');
    state.current_level = std::stoi(segment);
    std::getline(ss, segment, ';');
    state.enemy_to_kill = std::stoi(segment);

    return state;
}

std::string handleStartGame(const std::string &room_code)
{
    pthread_mutex_lock(&rooms_mutex);

    if (rooms.find(room_code) == rooms.end())
    {
        pthread_mutex_unlock(&rooms_mutex);
        return "ROOM_NOT_FOUND";
    }

    auto &room = rooms[room_code];
    room.is_playing = true;

    pthread_mutex_unlock(&rooms_mutex);
    return "OK";
}

std::string handleCheckGame(const std::string &room_code)
{
    pthread_mutex_lock(&rooms_mutex);

    if (rooms.find(room_code) == rooms.end())
    {
        pthread_mutex_unlock(&rooms_mutex);
        return "ROOM_NOT_FOUND";
    }

    auto &room = rooms[room_code];
    std::string response = room.is_playing ? "STARTED" : "NOT_STARTED";

    pthread_mutex_unlock(&rooms_mutex);
    return response;
}

void *handleGameSync(void *arg)
{
    SOCKET sync_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in sync_addr;
    sync_addr.sin_family = AF_INET;
    sync_addr.sin_port = htons(8890); // Port riêng cho game sync
    sync_addr.sin_addr.s_addr = INADDR_ANY;

    bind(sync_socket, (struct sockaddr *)&sync_addr, sizeof(sync_addr));

    while (true)
    {
        char buffer[4096];
        struct sockaddr_in client_addr;
        int client_len = sizeof(client_addr);

        int recv_len = recvfrom(sync_socket, buffer, sizeof(buffer), 0,
                                (struct sockaddr *)&client_addr, &client_len);

        if (recv_len > 0)
        {
            buffer[recv_len] = '\0';
            std::string data(buffer);

            // Parse room code và game state
            size_t pos = data.find(':');
            if (pos != std::string::npos)
            {
                std::string room_code = data.substr(0, pos);
                std::string state_data = data.substr(pos + 1);

                pthread_mutex_lock(&game_states_mutex);
                pthread_mutex_lock(&rooms_mutex);

                if (rooms.find(room_code) != rooms.end())
                {
                    auto &room = rooms[room_code];

                    // Nếu là host gửi game state
                    if (std::string(inet_ntoa(client_addr.sin_addr)) == room.host)
                    {
                        // Deserialize game state từ host
                        GameState state = deserializeGameState(state_data);
                        game_states[room_code] = state;

                        // Serialize lại và gửi cho tất cả clients
                        std::string serialized_state = serializeGameState(state);
                        std::string message = room_code + ":" + serialized_state;

                        // Gửi cho tất cả clients trừ host
                        for (auto &player_addr : room.player_addrs)
                        {
                            if (player_addr.sin_addr.s_addr != client_addr.sin_addr.s_addr)
                            {
                                sendto(sync_socket, message.c_str(), message.length(), 0,
                                       (struct sockaddr *)&player_addr, sizeof(player_addr));
                            }
                        }
                    }
                }

                pthread_mutex_unlock(&rooms_mutex);
                pthread_mutex_unlock(&game_states_mutex);
            }
        }
    }

    closesocket(sync_socket);
    return nullptr;
}
// Thêm hàm xử lý UDP
void *handleUDPMessages(void *arg)
{
    SOCKET udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in udp_addr;
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_port = htons(8889);
    udp_addr.sin_addr.s_addr = INADDR_ANY;

    bind(udp_socket, (struct sockaddr *)&udp_addr, sizeof(udp_addr));

    while (true)
    {
        char buffer[1024];
        struct sockaddr_in client_addr;
        int client_len = sizeof(client_addr);

        int recv_len = recvfrom(udp_socket, buffer, sizeof(buffer), 0,
                                (struct sockaddr *)&client_addr, &client_len);

        if (recv_len > 0)
        {
            buffer[recv_len] = '\0';
            std::string data(buffer);

            // Parse room code
            size_t pos = data.find(':');
            if (pos != std::string::npos)
            {
                std::string room_code = data.substr(0, pos);

                pthread_mutex_lock(&rooms_mutex);
                if (rooms.find(room_code) != rooms.end())
                {
                    auto &room = rooms[room_code];
                    // Gửi dữ liệu cho tất cả players trong phòng
                    for (auto &addr : room.player_addrs)
                    {
                        if (addr.sin_addr.s_addr != client_addr.sin_addr.s_addr ||
                            addr.sin_port != client_addr.sin_port)
                        {
                            sendto(udp_socket, buffer, recv_len, 0,
                                   (struct sockaddr *)&addr, sizeof(addr));
                        }
                    }
                }
                pthread_mutex_unlock(&rooms_mutex);
            }
        }
    }

    closesocket(udp_socket);
    return nullptr;
}

// Hàm lấy danh sách người chơi trong phòng
std::string handleGetPlayers(const std::string &room_code)
{
    pthread_mutex_lock(&rooms_mutex);

    // Kiểm tra phòng tồn tại
    if (rooms.find(room_code) == rooms.end())
    {
        pthread_mutex_unlock(&rooms_mutex);
        return "ROOM_NOT_FOUND";
    }

    // Lấy danh sách người chơi và ghép thành chuỗi, phân cách bằng dấu phẩy
    const auto &room = rooms[room_code];
    std::string player_list;
    for (size_t i = 0; i < room.players.size(); ++i)
    {
        player_list += room.players[i];
        if (i < room.players.size() - 1)
        {
            player_list += ",";
        }
    }

    pthread_mutex_unlock(&rooms_mutex);
    return player_list;
}

void handleGameEnd(const std::string &room_code)
{
    pthread_mutex_lock(&rooms_mutex);
    if (rooms.find(room_code) != rooms.end())
    {
        rooms[room_code].is_playing = false;

        // Xóa game state
        pthread_mutex_lock(&game_states_mutex);
        if (game_states.find(room_code) != game_states.end())
        {
            game_states.erase(room_code);
        }
        pthread_mutex_unlock(&game_states_mutex);
    }
    pthread_mutex_unlock(&rooms_mutex);
}

// Hàm xử lý rời phòng
std::string handleLeaveRoom(const std::string &username, const std::string &room_code)
{
    pthread_mutex_lock(&rooms_mutex);

    if (rooms.find(room_code) == rooms.end())
    {
        pthread_mutex_unlock(&rooms_mutex);
        return "ROOM_NOT_FOUND";
    }

    auto &room = rooms[room_code];

    // Tìm và xóa người chơi khỏi danh sách
    auto it = std::find(room.players.begin(), room.players.end(), username);
    if (it != room.players.end())
    {
        room.players.erase(it);

        // Nếu là host rời phòng hoặc phòng không còn ai
        if (username == room.host || room.players.empty())
        {
            // Xóa file phòng
            std::string filename = "rooms/room_" + room_code + ".txt";
            remove(filename.c_str());

            // Xóa phòng khỏi danh sách
            rooms.erase(room_code);

            pthread_mutex_unlock(&rooms_mutex);
            return "ROOM_DELETED";
        }
        else
        {
            // Cập nhật file phòng
            writeRoomFile(room_code, room.host, room.players);
        }

        pthread_mutex_lock(&players_mutex);
        if (players.find(username) != players.end())
        {
            players[username].current_room = "";
            players[username].is_host = false;
        }
        pthread_mutex_unlock(&players_mutex);

        pthread_mutex_unlock(&rooms_mutex);
        return "OK";
    }

    pthread_mutex_unlock(&rooms_mutex);
    return "PLAYER_NOT_FOUND";
}

// Hàm kiểm tra đăng nhập
bool checkLogin(const std::string &username, const std::string &password)
{
    std::ifstream file("user.txt");
    std::string line;

    while (std::getline(file, line))
    {
        size_t pos = line.find(':');
        if (pos != std::string::npos)
        {
            std::string stored_username = line.substr(0, pos);
            std::string stored_password = line.substr(pos + 1);
            if (username == stored_username && password == stored_password)
            {
                file.close();
                return true;
            }
        }
    }
    file.close();
    return false;
}

// Hàm xử lý đăng ký
std::string handleSignup(const std::string &username, const std::string &password)
{
    // Kiểm tra tài khoản tồn tại
    std::ifstream check_file("user.txt");
    std::string line;
    while (std::getline(check_file, line))
    {
        if (line.find(username + ":") == 0)
        {
            check_file.close();
            return "EXISTS";
        }
    }
    check_file.close();

    // Ghi tài khoản mới
    std::ofstream user_file("user.txt", std::ios::app);
    if (user_file.is_open())
    {
        user_file << username << ":" << password << std::endl;
        user_file.close();
        return "OK";
    }
    return "ERROR";
}

// Hàm xử lý tạo phòng
std::string handleCreateRoom(const std::string &username, const std::string &room_code)
{
    pthread_mutex_lock(&rooms_mutex);

    if (rooms.find(room_code) != rooms.end())
    {
        pthread_mutex_unlock(&rooms_mutex);
        return "ROOM_EXISTS";
    }

    RoomInfo room;
    room.room_code = room_code;
    room.players.push_back(username);
    room.host = username;
    room.is_playing = false;
    rooms[room_code] = room;

    // Tạo file room mới
    writeRoomFile(room_code, username, room.players);

    pthread_mutex_lock(&players_mutex);
    if (players.find(username) != players.end())
    {
        players[username].current_room = room_code;
        players[username].is_host = true;
    }
    pthread_mutex_unlock(&players_mutex);

    pthread_mutex_unlock(&rooms_mutex);
    return "OK";
}

// Hàm xử lý tham gia phòng
std::string handleJoinRoom(const std::string &username, const std::string &room_code)
{
    pthread_mutex_lock(&rooms_mutex);

    if (rooms.find(room_code) == rooms.end())
    {
        pthread_mutex_unlock(&rooms_mutex);
        return "ROOM_NOT_FOUND";
    }

    auto &room = rooms[room_code];

    for (const auto &player : room.players)
    {
        if (player == username)
        {
            pthread_mutex_unlock(&rooms_mutex);
            return "ALREADY_IN_ROOM";
        }
    }

    if (room.players.size() >= 2)
    {
        pthread_mutex_unlock(&rooms_mutex);
        return "ROOM_FULL";
    }

    room.players.push_back(username);
    writeRoomFile(room_code, room.host, room.players);

    pthread_mutex_lock(&players_mutex);
    if (players.find(username) != players.end())
    {
        players[username].current_room = room_code;
        players[username].is_host = false;
    }
    pthread_mutex_unlock(&players_mutex);

    pthread_mutex_unlock(&rooms_mutex);
    return "OK";
}

// Hàm xử lý TCP client
void *handleTCPClient(void *arg)
{
    ClientData *client_data = static_cast<ClientData *>(arg);
    SOCKET client_socket = client_data->client_socket;
    delete client_data;

    char buffer[BUFFER_SIZE] = {0};
    recv(client_socket, buffer, sizeof(buffer), 0);
    std::string request(buffer);
    std::string response;

    if (request.find("signin:") == 0)
    {
        // Format: signin:username&password
        std::string credentials = request.substr(7);
        size_t separator = credentials.find('&');
        std::string username = credentials.substr(0, separator);
        std::string password = credentials.substr(separator + 1);

        if (checkLogin(username, password))
        {
            pthread_mutex_lock(&players_mutex);
            PlayerInfo player;
            player.username = username;
            player.is_authenticated = true;
            players[username] = player;
            pthread_mutex_unlock(&players_mutex);
            response = "OK";
        }
        else
        {
            response = "FAILED";
        }
    }
    else if (request.find("leave_room:") == 0)
    {
        // Format: leave_room:room_code:username
        size_t first_colon = request.find(':');
        size_t second_colon = request.find(':', first_colon + 1);
        std::string room_code = request.substr(first_colon + 1, second_colon - first_colon - 1);
        std::string username = request.substr(second_colon + 1);
        response = handleLeaveRoom(username, room_code);
    }
    else if (request.find("create_room:") == 0)
    {
        // Format: create_room:room_code:username
        size_t first_colon = request.find(':');
        size_t second_colon = request.find(':', first_colon + 1);
        std::string room_code = request.substr(first_colon + 1, second_colon - first_colon - 1);
        std::string username = request.substr(second_colon + 1);
        response = handleCreateRoom(username, room_code);
    }
    else if (request.find("join_room:") == 0)
    {
        // Format: join_room:room_code:username
        size_t first_colon = request.find(':');
        size_t second_colon = request.find(':', first_colon + 1);
        std::string room_code = request.substr(first_colon + 1, second_colon - first_colon - 1);
        std::string username = request.substr(second_colon + 1);
        response = handleJoinRoom(username, room_code);
    }
    else if (request.find("signup:") == 0)
    {
        // Xử lý đăng ký
        std::string data = request.substr(7);
        std::string username = data.substr(data.find("username=") + 9,
                                           data.find("&password=") - (data.find("username=") + 9));
        std::string password = data.substr(data.find("&password=") + 10);
        response = handleSignup(username, password);
    }
    else if (request.find("get_players:") == 0)
    {
        // Format: get_players:room_code
        std::string room_code = request.substr(12); // Bỏ "get_players:"
        response = handleGetPlayers(room_code);
    }
    // Thêm case xử lý trong handleTCPClient
    else if (request.find("start_game:") == 0)
    {
        printf("is start_game\n");
        std::string room_code = request.substr(11); // Bỏ "start_game:"
        response = handleStartGame(room_code);
    }
    else if (request.find("check_game:") == 0)
    {
        printf("is check_game\n");
        std::string room_code = request.substr(11); // Bỏ "check_game:"
        response = handleCheckGame(room_code);
    }

    send(client_socket, response.c_str(), response.length(), 0);
    closesocket(client_socket);
    return nullptr;
}

// Main function
int main()
{
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    SOCKET tcp_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in tcp_addr;
    tcp_addr.sin_family = AF_INET;
    tcp_addr.sin_port = htons(TCP_PORT);
    tcp_addr.sin_addr.s_addr = INADDR_ANY;

    // Tạo UDP thread
    pthread_t udp_thread;
    pthread_create(&udp_thread, nullptr, handleUDPMessages, nullptr);

    // Thêm game sync thread
    pthread_t game_sync_thread;
    pthread_create(&game_sync_thread, nullptr, handleGameSync, nullptr);

    if (bind(tcp_socket, (struct sockaddr *)&tcp_addr, sizeof(tcp_addr)) == SOCKET_ERROR)
    {
        std::cout << "TCP bind failed" << std::endl;
        return 1;
    }
    listen(tcp_socket, 5);

    std::cout << "Server running on TCP port " << TCP_PORT << std::endl;

    while (true)
    {
        ClientData *client_data = new ClientData;
        int client_size = sizeof(client_data->client_addr);
        client_data->client_socket = accept(tcp_socket,
                                            (struct sockaddr *)&client_data->client_addr, &client_size);

        pthread_t tcp_thread;
        pthread_create(&tcp_thread, nullptr, handleTCPClient, client_data);
        pthread_detach(tcp_thread);
    }

    closesocket(tcp_socket);
    WSACleanup();
    return 0;
}