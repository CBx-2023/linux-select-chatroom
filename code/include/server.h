#ifndef CHATROOM_SERVER_H
#define CHATROOM_SERVER_H

#include "client_manager.h"
#include "logger.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <sys/types.h>

namespace chatroom {

using SendFunction = std::function<ssize_t(int, const char*, std::size_t)>;
using ReceiveFunction = std::function<ssize_t(int, char*, std::size_t)>;

class ChatServer {
public:
    explicit ChatServer(std::uint16_t port,
                        std::string log_path = "server.log",
                        SendFunction send_function = SendFunction(),
                        ReceiveFunction receive_function = ReceiveFunction());
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
    void handle_group_message(int fd, const std::string& content);
    void handle_private_message(int fd, const std::string& target, const std::string& content);
    void handle_list(int fd);
    void handle_disconnect(int fd, bool normal_quit);
    void broadcast_system(const std::string& message, int excluded_fd);
    bool send_response(int fd, const std::string& response);
    void close_client(int fd);

    std::uint16_t port_;
    int listen_fd_ = -1;
    bool running_ = false;
    ClientManager clients_;
    Logger logger_;
    SendFunction send_function_;
    ReceiveFunction receive_function_;
};

}  // namespace chatroom

#endif  // CHATROOM_SERVER_H
