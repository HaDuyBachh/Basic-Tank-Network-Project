#ifndef SETTINGS_H
#define SETTINGS_H

#include <string>

class Settings {
private:
    static std::string s_server_ip;
    static bool s_initialized;

public:
    static void setServerIP(const std::string& ip) {
        s_server_ip = ip;
        s_initialized = true;
    }
    
    static std::string getServerIP() {
        return s_initialized ? s_server_ip : "127.0.0.1";
    }

    static bool isInitialized() {
        return s_initialized;
    }
};

#endif