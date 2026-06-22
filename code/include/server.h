#ifndef CHATROOM_SERVER_H
#define CHATROOM_SERVER_H

#include "client_manager.h"

#include <cstddef>
#include <cstdint>

namespace chatroom {

class ChatServer {
public:
    explicit ChatServer(std::uint16_t port);
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
    void close_client(int fd);

    std::uint16_t port_;
    int listen_fd_ = -1;
    bool running_ = false;
    ClientManager clients_;
};

}  // namespace chatroom

#endif  // CHATROOM_SERVER_H
