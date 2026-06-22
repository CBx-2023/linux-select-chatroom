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
        reset();
    }

    FdGuard(const FdGuard&) = delete;
    FdGuard& operator=(const FdGuard&) = delete;

    int get() const { return fd_; }

    void reset()
    {
        if (fd_ >= 0) {
            close(fd_);
            fd_ = -1;
        }
    }

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

std::string read_available(int fd)
{
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);

    timeval timeout {};
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

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

void test_login_quit_abnormal_disconnect_and_rejections()
{
    const std::string log_path = "/tmp/chatroom_server_lifecycle_test.log";
    unlink(log_path.c_str());

    chatroom::ChatServer server(0, log_path);
    assert(server.start());

    auto alice = connect_to_server(server.port());
    accept_client(server);
    send_line(alice.get(), "LOGIN alice");
    process_client(server);
    assert(read_available(alice.get()) == "OK login successful\n");

    auto bob = connect_to_server(server.port());
    accept_client(server);
    send_line(bob.get(), "LOGIN bob");
    process_client(server);
    assert(read_available(bob.get()) == "OK login successful\n");
    assert(read_available(alice.get()) == "SYSTEM bob online\n");

    auto duplicate = connect_to_server(server.port());
    accept_client(server);
    send_line(duplicate.get(), "LOGIN alice");
    process_client(server);
    assert(read_available(duplicate.get()) == "ERROR nickname already used\n");

    auto invalid = connect_to_server(server.port());
    accept_client(server);
    send_line(invalid.get(), "LOGIN bad-name");
    process_client(server);
    assert(read_available(invalid.get()) == "ERROR invalid nickname\n");

    auto anonymous = connect_to_server(server.port());
    accept_client(server);
    send_line(anonymous.get(), "MSG hello before login");
    process_client(server);
    assert(read_available(anonymous.get()) == "ERROR please login first\n");

    send_line(bob.get(), "QUIT");
    process_client(server);
    assert(read_available(alice.get()) == "SYSTEM bob offline\n");
    assert(server.client_count() == 4);

    auto charlie = connect_to_server(server.port());
    accept_client(server);
    send_line(charlie.get(), "LOGIN charlie");
    process_client(server);
    assert(read_available(charlie.get()) == "OK login successful\n");
    assert(read_available(alice.get()) == "SYSTEM charlie online\n");

    charlie.reset();
    process_client(server);
    assert(read_available(alice.get()) == "SYSTEM charlie offline\n");

    const std::string log = read_file(log_path);
    assert(log.find("STARTUP port=") != std::string::npos);
    assert(log.find("LOGIN nickname=alice") != std::string::npos);
    assert(log.find("LOGIN nickname=bob") != std::string::npos);
    assert(log.find("QUIT nickname=bob") != std::string::npos);
    assert(log.find("ABNORMAL_DISCONNECT nickname=charlie") != std::string::npos);

    server.stop();
    unlink(log_path.c_str());
}

}  // namespace

int main()
{
    test_login_quit_abnormal_disconnect_and_rejections();
    std::cout << "server_lifecycle_tests passed\n";
    return 0;
}
