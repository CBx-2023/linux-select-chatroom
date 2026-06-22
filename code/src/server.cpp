#include "server.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace chatroom {

namespace {

constexpr int kListenBacklog = 16;
constexpr std::size_t kRecvBufferBytes = 512;

void print_errno(const std::string& action)
{
    std::cerr << action << " failed: " << std::strerror(errno) << '\n';
}

}  // namespace

ChatServer::ChatServer(std::uint16_t port, std::string log_path)
    : port_(port), logger_(std::move(log_path))
{
}

ChatServer::~ChatServer()
{
    stop();
}

bool ChatServer::start()
{
    if (listen_fd_ >= 0) {
        return true;
    }

    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        print_errno("socket");
        return false;
    }

    int reuse = 1;
    if (setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) != 0) {
        print_errno("setsockopt");
        stop();
        return false;
    }

    sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port_);

    if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        print_errno("bind");
        stop();
        return false;
    }

    if (listen(listen_fd_, kListenBacklog) != 0) {
        print_errno("listen");
        stop();
        return false;
    }

    socklen_t address_length = sizeof(address);
    if (getsockname(listen_fd_, reinterpret_cast<sockaddr*>(&address), &address_length) != 0) {
        print_errno("getsockname");
        stop();
        return false;
    }

    port_ = ntohs(address.sin_port);
    running_ = true;
    logger_.log_startup(port_);
    return true;
}

void ChatServer::stop()
{
    running_ = false;

    std::vector<int> fds = clients_.fds();
    for (int fd : fds) {
        close_client(fd);
    }

    if (listen_fd_ >= 0) {
        close(listen_fd_);
        listen_fd_ = -1;
    }
}

bool ChatServer::run_once(int timeout_ms)
{
    if (listen_fd_ < 0) {
        return false;
    }

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(listen_fd_, &read_fds);
    int max_fd = listen_fd_;

    for (int fd : clients_.fds()) {
        FD_SET(fd, &read_fds);
        if (fd > max_fd) {
            max_fd = fd;
        }
    }

    timeval timeout {};
    timeval* timeout_ptr = nullptr;
    if (timeout_ms >= 0) {
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;
        timeout_ptr = &timeout;
    }

    int ready = select(max_fd + 1, &read_fds, nullptr, nullptr, timeout_ptr);
    if (ready < 0) {
        if (errno == EINTR) {
            return true;
        }
        print_errno("select");
        return false;
    }
    if (ready == 0) {
        return false;
    }

    if (FD_ISSET(listen_fd_, &read_fds)) {
        if (!handle_new_client()) {
            return false;
        }
    }

    std::vector<int> fds = clients_.fds();
    for (int fd : fds) {
        if (FD_ISSET(fd, &read_fds)) {
            handle_client_readable(fd);
        }
    }

    return true;
}

void ChatServer::run_forever()
{
    while (running_) {
        run_once(-1);
    }
}

std::uint16_t ChatServer::port() const
{
    return port_;
}

std::size_t ChatServer::client_count() const
{
    return clients_.client_count();
}

bool ChatServer::handle_new_client()
{
    sockaddr_in client_address {};
    socklen_t address_length = sizeof(client_address);
    int client_fd = accept(listen_fd_, reinterpret_cast<sockaddr*>(&client_address), &address_length);
    if (client_fd < 0) {
        if (errno == EINTR) {
            return true;
        }
        print_errno("accept");
        return false;
    }

    if (!clients_.add_client(client_fd)) {
        close(client_fd);
        return false;
    }
    return true;
}

void ChatServer::handle_client_readable(int fd)
{
    ClientInfo* client = clients_.find_by_fd(fd);
    if (client == nullptr) {
        close_client(fd);
        return;
    }

    char buffer[kRecvBufferBytes];
    ssize_t bytes_read = recv(fd, buffer, sizeof(buffer), 0);
    if (bytes_read == 0) {
        handle_disconnect(fd, false);
        return;
    }
    if (bytes_read < 0) {
        if (errno == EINTR) {
            return;
        }
        logger_.log_error("recv failed on fd " + std::to_string(fd) + ": " + std::strerror(errno));
        handle_disconnect(fd, false);
        return;
    }

    std::string bytes(buffer, static_cast<std::size_t>(bytes_read));
    std::vector<std::string> lines = client->receive_buffer.append(bytes);
    for (const std::string& line : lines) {
        process_client_line(fd, line);
        if (clients_.find_by_fd(fd) == nullptr) {
            return;
        }
    }

    ClientInfo* current = clients_.find_by_fd(fd);
    if (current != nullptr && current->receive_buffer.pending().size() > kMaxMessageBytes) {
        if (!send_response(fd, make_error("message too long"))) {
            handle_disconnect(fd, false);
            return;
        }
        current->receive_buffer.clear();
    }
}

void ChatServer::process_client_line(int fd, const std::string& line)
{
    Command command = parse_client_command(line);
    if (command.type == CommandType::Invalid) {
        if (!send_response(fd, make_error(command.error))) {
            handle_disconnect(fd, false);
        }
        logger_.log_error("invalid command from fd " + std::to_string(fd) + ": " + command.error);
        return;
    }

    ClientInfo* client = clients_.find_by_fd(fd);
    if (client == nullptr) {
        return;
    }

    if (!client->logged_in) {
        if (command.type == CommandType::Login) {
            handle_login(fd, command.nickname);
            return;
        }
        if (command.type == CommandType::Quit) {
            handle_disconnect(fd, true);
            return;
        }
        if (!send_response(fd, make_error("please login first"))) {
            handle_disconnect(fd, false);
        }
        return;
    }

    if (command.type == CommandType::Quit) {
        handle_disconnect(fd, true);
        return;
    }

    if (command.type == CommandType::Login) {
        if (!send_response(fd, make_error("invalid command"))) {
            handle_disconnect(fd, false);
        }
        return;
    }

    if (command.type == CommandType::Message) {
        handle_group_message(fd, command.content);
        return;
    }

    if (command.type == CommandType::PrivateMessage) {
        handle_private_message(fd, command.target, command.content);
        return;
    }

    if (command.type == CommandType::List) {
        handle_list(fd);
    }
}

void ChatServer::handle_login(int fd, const std::string& nickname)
{
    if (clients_.nickname_in_use(nickname)) {
        if (!send_response(fd, make_error("nickname already used"))) {
            handle_disconnect(fd, false);
        }
        return;
    }

    if (!clients_.login(fd, nickname)) {
        if (!send_response(fd, make_error("invalid command"))) {
            handle_disconnect(fd, false);
        }
        return;
    }

    if (!send_response(fd, make_ok("login successful"))) {
        handle_disconnect(fd, false);
        return;
    }

    logger_.log_login(nickname);
    broadcast_system(nickname + " online", fd);
}

void ChatServer::handle_group_message(int fd, const std::string& content)
{
    const ClientInfo* sender = clients_.find_by_fd(fd);
    if (sender == nullptr || !sender->logged_in) {
        return;
    }

    const std::string response = make_chat(sender->nickname, content);
    std::vector<int> fds = clients_.fds();
    for (int target_fd : fds) {
        if (target_fd == fd) {
            continue;
        }

        const ClientInfo* target = clients_.find_by_fd(target_fd);
        if (target == nullptr || !target->logged_in) {
            continue;
        }

        if (!send_response(target_fd, response)) {
            handle_disconnect(target_fd, false);
        }
    }

    logger_.log_group_chat(sender->nickname, content);
}

void ChatServer::handle_private_message(int fd, const std::string& target, const std::string& content)
{
    const ClientInfo* sender = clients_.find_by_fd(fd);
    if (sender == nullptr || !sender->logged_in) {
        return;
    }

    const ClientInfo* target_client = clients_.find_by_nickname(target);
    if (target_client == nullptr || !target_client->logged_in) {
        if (!send_response(fd, make_error("user not online"))) {
            handle_disconnect(fd, false);
        }
        return;
    }

    if (!send_response(target_client->fd, make_private(sender->nickname, content))) {
        handle_disconnect(target_client->fd, false);
        return;
    }

    logger_.log_private_chat(sender->nickname, target, content);
}

void ChatServer::handle_list(int fd)
{
    if (!send_response(fd, make_userlist(clients_.online_users()))) {
        handle_disconnect(fd, false);
    }
}

void ChatServer::handle_disconnect(int fd, bool normal_quit)
{
    ClientInfo* client = clients_.find_by_fd(fd);
    if (client == nullptr) {
        return;
    }

    const bool was_logged_in = client->logged_in;
    const std::string nickname = client->nickname;

    close_client(fd);

    if (!was_logged_in) {
        return;
    }

    if (normal_quit) {
        logger_.log_quit(nickname);
    } else {
        logger_.log_abnormal_disconnect(nickname);
    }
    broadcast_system(nickname + " offline", fd);
}

void ChatServer::broadcast_system(const std::string& message, int excluded_fd)
{
    std::vector<int> fds = clients_.fds();
    for (int fd : fds) {
        if (fd == excluded_fd) {
            continue;
        }

        const ClientInfo* client = clients_.find_by_fd(fd);
        if (client == nullptr || !client->logged_in) {
            continue;
        }

        if (!send_response(fd, make_system(message))) {
            handle_disconnect(fd, false);
        }
    }
}

bool ChatServer::send_response(int fd, const std::string& response)
{
    std::size_t sent_total = 0;
    while (sent_total < response.size()) {
        ssize_t sent = send(fd, response.data() + sent_total, response.size() - sent_total, MSG_NOSIGNAL);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            logger_.log_error("send failed on fd " + std::to_string(fd) + ": " + std::strerror(errno));
            return false;
        }
        if (sent == 0) {
            logger_.log_error("send returned 0 on fd " + std::to_string(fd));
            return false;
        }
        sent_total += static_cast<std::size_t>(sent);
    }
    return true;
}

void ChatServer::close_client(int fd)
{
    if (clients_.remove_client(fd)) {
        close(fd);
    }
}

}  // namespace chatroom

#ifndef CHATROOM_SERVER_TEST
int main(int argc, char* argv[])
{
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <port>\n";
        return 1;
    }

    int port = 0;
    try {
        port = std::stoi(argv[1]);
    } catch (const std::exception&) {
        std::cerr << "Invalid port\n";
        return 1;
    }

    if (port <= 0 || port > 65535) {
        std::cerr << "Invalid port\n";
        return 1;
    }

    chatroom::ChatServer server(static_cast<std::uint16_t>(port));
    if (!server.start()) {
        return 1;
    }

    std::cout << "server listening on port " << server.port() << '\n';
    server.run_forever();
    return 0;
}
#endif
