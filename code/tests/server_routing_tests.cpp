#include "server.h"

#include <arpa/inet.h>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/select.h>
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

void send_line(int fd, const std::string& line)
{
    const std::string wire = line + "\n";
    assert(send(fd, wire.data(), wire.size(), 0) == static_cast<ssize_t>(wire.size()));
}

std::string read_available(int fd, int timeout_ms = 1000)
{
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);

    timeval timeout {};
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;

    int ready = select(fd + 1, &read_fds, nullptr, nullptr, &timeout);
    assert(ready >= 0);
    if (ready == 0) {
        return "";
    }

    char buffer[4096];
    ssize_t bytes = recv(fd, buffer, sizeof(buffer), 0);
    if (bytes <= 0) {
        return "";
    }
    return std::string(buffer, static_cast<std::size_t>(bytes));
}

std::string read_file(const std::string& path)
{
    std::ifstream in(path);
    std::ostringstream contents;
    contents << in.rdbuf();
    return contents.str();
}

void accept_client(chatroom::ChatServer& server)
{
    assert(server.run_once(500));
}

void process_client(chatroom::ChatServer& server)
{
    assert(server.run_once(500));
}

void login_client(chatroom::ChatServer& server, int fd, const std::string& nickname)
{
    send_line(fd, "LOGIN " + nickname);
    process_client(server);
    assert(read_available(fd) == "OK login successful\n");
}

void test_group_private_and_list_routing()
{
    const std::string log_path = "/tmp/chatroom_server_routing_test.log";
    unlink(log_path.c_str());

    chatroom::ChatServer server(0, log_path);
    assert(server.start());

    auto alice = connect_to_server(server.port());
    accept_client(server);
    login_client(server, alice.get(), "alice");

    auto bob = connect_to_server(server.port());
    accept_client(server);
    login_client(server, bob.get(), "bob");
    assert(read_available(alice.get()) == "SYSTEM bob online\n");

    auto carol = connect_to_server(server.port());
    accept_client(server);
    login_client(server, carol.get(), "carol");
    assert(read_available(alice.get()) == "SYSTEM carol online\n");
    assert(read_available(bob.get()) == "SYSTEM carol online\n");

    send_line(alice.get(), "MSG hello everyone");
    process_client(server);
    assert(read_available(bob.get()) == "CHAT alice hello everyone\n");
    assert(read_available(carol.get()) == "CHAT alice hello everyone\n");
    assert(read_available(alice.get(), 100) == "");

    send_line(alice.get(), "PRIVATE bob direct note");
    process_client(server);
    assert(read_available(bob.get()) == "PRIVATE alice direct note\n");
    assert(read_available(carol.get(), 100) == "");
    assert(read_available(alice.get(), 100) == "");

    send_line(alice.get(), "PRIVATE nobody still there?");
    process_client(server);
    assert(read_available(alice.get()) == "ERROR user not online\n");
    assert(read_available(bob.get(), 100) == "");
    assert(read_available(carol.get(), 100) == "");

    send_line(alice.get(), "LIST");
    process_client(server);
    assert(read_available(alice.get()) == "USERLIST alice,bob,carol\n");
    assert(read_available(bob.get(), 100) == "");
    assert(read_available(carol.get(), 100) == "");

    const std::string log = read_file(log_path);
    assert(log.find("GROUP_CHAT from=alice content=hello everyone") != std::string::npos);
    assert(log.find("PRIVATE_CHAT from=alice to=bob content=direct note") != std::string::npos);

    server.stop();
    unlink(log_path.c_str());
}

}  // namespace

int main()
{
    test_group_private_and_list_routing();
    std::cout << "server_routing_tests passed\n";
    return 0;
}
