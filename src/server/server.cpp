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

struct InputData
{
    bool up;
    bool down;
    bool left;
    bool right;
    bool fire;
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
    int selected_level;

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
    InputData client_input;
    std::string game_state;
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

std::string handleUpdateLevel(const std::string &room_code, int level)
{
    pthread_mutex_lock(&rooms_mutex);

    if (rooms.find(room_code) == rooms.end())
    {
        pthread_mutex_unlock(&rooms_mutex);
        return "ROOM_NOT_FOUND";
    }

    auto &room = rooms[room_code];
    room.selected_level = level;

    pthread_mutex_unlock(&rooms_mutex);
    return "OK";
}

std::string handleGetLevel(const std::string &room_code)
{
    pthread_mutex_lock(&rooms_mutex);

    if (rooms.find(room_code) == rooms.end())
    {
        pthread_mutex_unlock(&rooms_mutex);
        return "ROOM_NOT_FOUND";
    }

    auto &room = rooms[room_code];
    std::string response = std::to_string(room.selected_level);

    pthread_mutex_unlock(&rooms_mutex);
    return response;
}

// Truyền dữ liệu rặng game đã bắt đầu
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

// Kiểm tra xem có bắt đầu game chưa
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

std::string handleHostData(const std::string &data)
{
    std::stringstream ss(data);
    std::string head, room_code;

    std::getline(ss, head, ':');
    std::getline(ss, room_code, ':');

    // Create game state if not exists
    if (game_states.find(room_code) == game_states.end())
    {
        game_states[room_code] = GameState();
    }

    game_states[room_code].game_state = data;

    return room_code;
}

std::string handleClientData(const std::string &data)
{
    std::stringstream ss(data);
    std::string room_code, input_data;

    // std::cout << "Raw data: " << data << std::endl;

    // Parse format: "client_input:room_code:up,down,left,right,fire"
    std::getline(ss, room_code, ':');
    std::getline(ss, input_data);

    // std::cout << "Input data string: " << input_data << std::endl;

    std::vector<bool> inputs;
    std::stringstream input_ss(input_data);
    std::string value;

    // Tách các giá trị input
    while (std::getline(input_ss, value, ','))
    {
        inputs.push_back(value == "1");
    }

    // std::cout<<inputs.size()<<std::endl;

    if (inputs.size() >= 5)
    {
        pthread_mutex_lock(&game_states_mutex);

        // Tìm và cập nhật game state
        if (game_states.find(room_code) != game_states.end())
        {
            auto &state = game_states[room_code];
            state.client_input.up = inputs[0];
            state.client_input.down = inputs[1];
            state.client_input.left = inputs[2];
            state.client_input.right = inputs[3];
            state.client_input.fire = inputs[4];

            std::cout << "Updated input for room " << room_code << std::endl;
            std::cout << "Input state: "
                      << "Up=" << inputs[0]
                      << " Down=" << inputs[1]
                      << " Left=" << inputs[2]
                      << " Right=" << inputs[3]
                      << " Fire=" << inputs[4] << std::endl;
        }

        pthread_mutex_unlock(&game_states_mutex);
    }

    return room_code;
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
        std::string room_code = request.substr(11); // Bỏ "check_game:"
        response = handleCheckGame(room_code);
    }
    else if (request.find("game_state:") == 0)
    {
        // Handle Data
        auto room_code = handleHostData(request);

        // response
        response = "host_response:";

        if (game_states.find(room_code) != game_states.end())
        {
            auto &state = game_states[room_code];

            response += std::to_string(state.client_input.up) + "," + std::to_string(state.client_input.down) + "," + std::to_string(state.client_input.left) + "," + std::to_string(state.client_input.right) + "," + std::to_string(state.client_input.fire);
        }
        else
        {
            response += "0,0,0,0,0"; // Default no input
        }
    }
    else if (request.find("client_data:") == 0)
    {
        std::string clientData = request.substr(12);
        auto room_code = handleClientData(clientData);

        if (game_states.find(room_code) != game_states.end())
        {
            auto &state = game_states[room_code];
            response = state.game_state;
        }
        else
        {
            response = "game_state:" + room_code + ":";
        }
    }
    else if (request.find("update_level:") == 0)
    {
        // Format: update_level:room_code:level
        size_t first_colon = request.find(':');
        size_t second_colon = request.find(':', first_colon + 1);
        std::string room_code = request.substr(first_colon + 1,
                                               second_colon - first_colon - 1);
        int level = std::stoi(request.substr(second_colon + 1));
        response = handleUpdateLevel(room_code, level);
    }
    else if (request.find("get_level:") == 0)
    {
        // Format: get_level:room_code
        std::string room_code = request.substr(10);
        response = handleGetLevel(room_code);
    }
    else if (request.find("save_score:") == 0)
    {
        // Format: save_score:room_code:level:host_name:host_score:client_name:client_score
        std::stringstream ss(request.substr(11));
        std::string room_code, level, main_name, main_score, coop_name, coop_score;

        std::getline(ss, room_code, ':');
        std::getline(ss, level, ':');
        std::getline(ss, main_name, ':');
        std::getline(ss, main_score, ':');
        std::getline(ss, coop_name, ':');
        std::getline(ss, coop_score);

        // Save to file
        std::string filename = "scores/" + main_name + ".txt";
        std::ofstream file(filename, std::ios::app);
        if (file.is_open())
        {
            time_t now = time(0);
            tm *ltm = localtime(&now);

            file << " Room: " << room_code << " Level:" << level
                 << " Host:" << main_name << " Score:" << main_score
                 << " Client:" << coop_name << " Score:" << coop_score
                 << " Date:" << ltm->tm_mday << "/" << ltm->tm_mon + 1 << "/" << ltm->tm_year + 1900 << "\n";

            file.close();
            response = "OK";
        }
    }
    else
    {
        std::cout << "Unknown: " << request << std::endl;
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