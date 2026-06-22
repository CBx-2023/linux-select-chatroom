#include "server.h"

#include <arpa/inet.h>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

namespace {

class FdGuard {
public:
    explicit FdGuard(int fd = -1) : fd_(fd) {}
    ~FdGuard()
    {
        if (fd_ >= 0) {
            close(fd_);
        }
    }

    FdGuard(const FdGuard&) = delete;
    FdGuard& operator=(const FdGuard&) = delete;

    int get() const { return fd_; }

private:
    int fd_;
};

FdGuard connect_to_server(std::uint16_t port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(fd >= 0);

    sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    assert(inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) == 1);

    int rc = connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address));
    if (rc != 0) {
        std::cerr << "connect failed: " << std::strerror(errno) << '\n';
    }
    assert(rc == 0);
    return FdGuard(fd);
}

void test_listener_accepts_and_tracks_multiple_clients()
{
    chatroom::ChatServer server(0);
    assert(server.start());
    assert(server.port() != 0);

    auto first = connect_to_server(server.port());
    auto second = connect_to_server(server.port());

    assert(server.run_once(500));
    assert(server.run_once(500));
    assert(server.client_count() == 2);

    const std::string ping = "x";
    assert(send(first.get(), ping.data(), ping.size(), 0) == 1);
    assert(send(second.get(), ping.data(), ping.size(), 0) == 1);

    assert(server.run_once(500));
    assert(server.client_count() == 2);

    server.stop();
}

}  // namespace

int main()
{
    test_listener_accepts_and_tracks_multiple_clients();
    std::cout << "server_listener_tests passed\n";
    return 0;
}
