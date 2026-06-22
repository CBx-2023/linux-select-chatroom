#ifndef CHATROOM_LOGGER_H
#define CHATROOM_LOGGER_H

#include <string>

namespace chatroom {

class Logger {
public:
    explicit Logger(std::string path = "server.log");

    void log_startup(int port) const;
    void log_login(const std::string& nickname,
                   const std::string& address = "") const;
    void log_quit(const std::string& nickname) const;
    void log_abnormal_disconnect(const std::string& nickname) const;
    void log_group_chat(const std::string& from,
                        const std::string& content) const;
    void log_private_chat(const std::string& from,
                          const std::string& to,
                          const std::string& content) const;
    void log_error(const std::string& message) const;

private:
    std::string path_;

    void write_line(const std::string& line) const;
};

}  // namespace chatroom

#endif  // CHATROOM_LOGGER_H
