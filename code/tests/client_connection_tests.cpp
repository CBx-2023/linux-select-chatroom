#include "client.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cassert>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

namespace {

class Fd {
public:
    explicit Fd(int fd = -1) : fd_(fd) {}
    ~Fd() { reset(); }

    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;

    int get() const { return fd_; }

    int release()
    {
        int fd = fd_;
        fd_ = -1;
        return fd;
    }

    void reset(int fd = -1)
    {
        if (fd_ >= 0) {
            close(fd_);
        }
        fd_ = fd;
    }

private:
    int fd_;
};

int create_loopback_listener()
{
    Fd listener(socket(AF_INET, SOCK_STREAM, 0));
    assert(listener.get() >= 0);

    int yes = 1;
    assert(setsockopt(listener.get(), SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == 0);

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;

    assert(bind(listener.get(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
    assert(listen(listener.get(), 1) == 0);
    return listener.release();
}

int listener_port(int listener)
{
    sockaddr_in addr {};
    socklen_t len = sizeof(addr);
    assert(getsockname(listener, reinterpret_cast<sockaddr*>(&addr), &len) == 0);
    return ntohs(addr.sin_port);
}

std::string recv_line(int fd)
{
    std::string line;
    char ch = '\0';
    while (recv(fd, &ch, 1, 0) == 1) {
        line.push_back(ch);
        if (ch == '\n') {
            break;
        }
    }
    return line;
}

struct ServerResult {
    std::string received;
};

ServerResult accept_one_login(int listener, const std::string& response)
{
    Fd client(accept(listener, nullptr, nullptr));
    assert(client.get() >= 0);
    ServerResult result;
    result.received = recv_line(client.get());
    assert(send(client.get(), response.data(), response.size(), 0) ==
           static_cast<ssize_t>(response.size()));
    return result;
}

void test_parse_options_accepts_ip_port_and_nickname()
{
    const char* argv[] = {"client", "127.0.0.1", "8888", "alice"};
    chatroom::ClientOptions options;
    std::ostringstream err;

    bool ok = chatroom::parse_client_options(4, const_cast<char**>(argv), options, err);

    assert(ok);
    assert(options.server_ip == "127.0.0.1");
    assert(options.port == 8888);
    assert(options.nickname == "alice");
    assert(err.str().empty());
}

void test_parse_options_rejects_missing_arguments()
{
    const char* argv[] = {"client", "127.0.0.1"};
    chatroom::ClientOptions options;
    std::ostringstream err;

    bool ok = chatroom::parse_client_options(2, const_cast<char**>(argv), options, err);

    assert(!ok);
    assert(err.str().find("Usage:") != std::string::npos);
}

void test_connect_and_login_sends_login_and_accepts_ok()
{
    Fd listener(create_loopback_listener());
    int port = listener_port(listener.get());

    std::thread server([&]() {
        ServerResult result = accept_one_login(listener.get(), "OK welcome\n");
        assert(result.received == "LOGIN alice\n");
    });

    std::ostringstream out;
    std::ostringstream err;
    chatroom::ChatClient client(out, err);

    bool ok = client.connect_and_login({"127.0.0.1", port, "alice"});

    server.join();
    assert(ok);
    assert(err.str().empty());
    assert(out.str().find("Connected") != std::string::npos);
}

void test_login_failure_prints_server_reason()
{
    Fd listener(create_loopback_listener());
    int port = listener_port(listener.get());

    std::thread server([&]() {
        ServerResult result = accept_one_login(listener.get(), "ERROR nickname already used\n");
        assert(result.received == "LOGIN alice\n");
    });

    std::ostringstream out;
    std::ostringstream err;
    chatroom::ChatClient client(out, err);

    bool ok = client.connect_and_login({"127.0.0.1", port, "alice"});

    server.join();
    assert(!ok);
    assert(out.str().empty());
    assert(err.str().find("Login failed") != std::string::npos);
    assert(err.str().find("nickname already used") != std::string::npos);
}

void test_connection_failure_prints_target()
{
    Fd listener(create_loopback_listener());
    int port = listener_port(listener.get());
    listener.reset();

    std::ostringstream out;
    std::ostringstream err;
    chatroom::ChatClient client(out, err);

    bool ok = client.connect_and_login({"127.0.0.1", port, "alice"});

    assert(!ok);
    assert(out.str().empty());
    assert(err.str().find("Connection failed") != std::string::npos);
    assert(err.str().find("127.0.0.1") != std::string::npos);
}

}  // namespace

int main()
{
    test_parse_options_accepts_ip_port_and_nickname();
    test_parse_options_rejects_missing_arguments();
    test_connect_and_login_sends_login_and_accepts_ok();
    test_login_failure_prints_server_reason();
    test_connection_failure_prints_target();

    std::cout << "client connection tests passed\n";
    return 0;
}
