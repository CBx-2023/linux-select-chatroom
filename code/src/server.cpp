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
#include <vector>

namespace chatroom {

namespace {

constexpr int kListenBacklog = 16;

void print_errno(const std::string& action)
{
    std::cerr << action << " failed: " << std::strerror(errno) << '\n';
}

}  // namespace

ChatServer::ChatServer(std::uint16_t port) : port_(port) {}

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
    char buffer[256];
    ssize_t bytes_read = recv(fd, buffer, sizeof(buffer), 0);
    if (bytes_read <= 0) {
        close_client(fd);
    }
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
