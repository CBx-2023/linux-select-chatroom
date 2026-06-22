#include "protocol.h"
#include "server.h"

#include <arpa/inet.h>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <functional>
#include <iostream>
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

void test_invalid_and_oversized_input_keep_server_usable()
{
    chatroom::ChatServer server(0, "/tmp/chatroom_server_hardening_invalid.log");
    assert(server.start());

    auto alice = connect_to_server(server.port());
    accept_client(server);
    login_client(server, alice.get(), "alice");

    send_line(alice.get(), "BOGUS command");
    process_client(server);
    assert(read_available(alice.get()) == "ERROR invalid command\n");

    const std::string too_long(chatroom::kMaxMessageBytes + 1, 'x');
    assert(send(alice.get(), too_long.data(), too_long.size(), 0) == static_cast<ssize_t>(too_long.size()));
    std::string oversized_response;
    for (int i = 0; i < 4 && oversized_response.empty(); ++i) {
        process_client(server);
        oversized_response = read_available(alice.get(), 50);
    }
    assert(oversized_response == "ERROR message too long\n");

    send_line(alice.get(), "LIST");
    process_client(server);
    assert(read_available(alice.get()) == "USERLIST alice\n");

    server.stop();
}

void test_send_failure_removes_only_failed_recipient_and_continues_delivery()
{
    int failed_chat_sends = 0;
    chatroom::SendFunction sender = [&](int fd, const char* data, std::size_t length) -> ssize_t {
        const std::string payload(data, length);
        if (payload.find("CHAT alice send failure check") == 0 && failed_chat_sends == 0) {
            ++failed_chat_sends;
            errno = EPIPE;
            return -1;
        }
        return send(fd, data, length, MSG_NOSIGNAL);
    };

    chatroom::ChatServer server(0, "/tmp/chatroom_server_hardening_send.log", sender);
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

    send_line(alice.get(), "MSG send failure check");
    process_client(server);
    assert(failed_chat_sends == 1);
    assert(server.client_count() == 2);
    assert(read_available(alice.get()) == "SYSTEM bob offline\n");
    std::string carol_messages = read_available(carol.get());
    if (carol_messages.find("CHAT alice send failure check\n") == std::string::npos) {
        carol_messages += read_available(carol.get());
    }
    assert(carol_messages.find("SYSTEM bob offline\n") != std::string::npos);
    assert(carol_messages.find("CHAT alice send failure check\n") != std::string::npos);

    send_line(alice.get(), "LIST");
    process_client(server);
    assert(read_available(alice.get()) == "USERLIST alice,carol\n");

    server.stop();
}

void test_recv_failure_removes_only_failed_client_and_server_stays_usable()
{
    bool fail_next_recv = false;
    chatroom::ReceiveFunction receiver = [&](int fd, char* data, std::size_t length) -> ssize_t {
        if (fail_next_recv) {
            fail_next_recv = false;
            errno = ECONNRESET;
            return -1;
        }
        return recv(fd, data, length, 0);
    };

    chatroom::ChatServer server(
        0,
        "/tmp/chatroom_server_hardening_recv.log",
        chatroom::SendFunction(),
        receiver);
    assert(server.start());

    auto alice = connect_to_server(server.port());
    accept_client(server);
    login_client(server, alice.get(), "alice");

    auto bob = connect_to_server(server.port());
    accept_client(server);
    login_client(server, bob.get(), "bob");
    assert(read_available(alice.get()) == "SYSTEM bob online\n");

    fail_next_recv = true;
    send_line(bob.get(), "MSG this recv should fail");
    process_client(server);
    assert(server.client_count() == 1);
    assert(read_available(alice.get()) == "SYSTEM bob offline\n");

    send_line(alice.get(), "LIST");
    process_client(server);
    assert(read_available(alice.get()) == "USERLIST alice\n");

    server.stop();
}

}  // namespace

int main()
{
    test_invalid_and_oversized_input_keep_server_usable();
    test_send_failure_removes_only_failed_recipient_and_continues_delivery();
    test_recv_failure_removes_only_failed_client_and_server_stays_usable();
    std::cout << "server_hardening_tests passed\n";
    return 0;
}
