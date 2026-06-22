#ifndef CHATROOM_SERVER_H
#define CHATROOM_SERVER_H

#include "client_manager.h"
#include "logger.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace chatroom {

class ChatServer {
public:
    explicit ChatServer(std::uint16_t port, std::string log_path = "server.log");
    ~ChatServer();

    ChatServer(const ChatServer&) = delete;
    ChatServer& operator=(const ChatServer&) = delete;

    bool start();
    void stop();
    bool run_once(int timeout_ms);
    void run_forever();

    std::uint16_t port() const;
    std::size_t client_count() const;

private:
    bool handle_new_client();
    void handle_client_readable(int fd);
    void process_client_line(int fd, const std::string& line);
    void handle_login(int fd, const std::string& nickname);
    void handle_disconnect(int fd, bool normal_quit);
    void broadcast_system(const std::string& message, int excluded_fd);
    bool send_response(int fd, const std::string& response);
    void close_client(int fd);

    std::uint16_t port_;
    int listen_fd_ = -1;
    bool running_ = false;
    ClientManager clients_;
    Logger logger_;
};

}  // namespace chatroom

#endif  // CHATROOM_SERVER_H
