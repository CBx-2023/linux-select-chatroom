#include "client.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cassert>
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

class Pipe {
public:
    Pipe()
    {
        int fds[2] = {-1, -1};
        assert(pipe(fds) == 0);
        read_end_.reset(fds[0]);
        write_end_.reset(fds[1]);
    }

    int read_fd() const { return read_end_.get(); }

    void write_text(const std::string& text)
    {
        assert(write(write_end_.get(), text.data(), text.size()) ==
               static_cast<ssize_t>(text.size()));
    }

private:
    Fd read_end_;
    Fd write_end_;
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

void send_all(int fd, const std::string& text)
{
    assert(send(fd, text.data(), text.size(), 0) == static_cast<ssize_t>(text.size()));
}

struct LoopServerResult {
    std::string login;
    std::string command;
};

chatroom::UserInputResult echo_input_as_command(const std::string& line)
{
    return {line + "\n", "", false};
}

void test_select_loop_processes_input_and_server_messages_without_blocking()
{
    Fd listener(create_loopback_listener());
    int port = listener_port(listener.get());
    LoopServerResult result;

    std::thread server([&]() {
        Fd client(accept(listener.get(), nullptr, nullptr));
        assert(client.get() >= 0);
        result.login = recv_line(client.get());
        send_all(client.get(), "OK welcome\n");
        send_all(client.get(), "SYSTEM hello while typing\n");
        result.command = recv_line(client.get());
    });

    Pipe input;
    input.write_text("typed text\n");

    std::ostringstream out;
    std::ostringstream err;
    chatroom::ChatClient client(out, err);
    assert(client.connect_and_login({"127.0.0.1", port, "alice"}));

    bool ok = client.run_event_loop(input.read_fd(), echo_input_as_command);

    server.join();
    assert(!ok);
    assert(result.login == "LOGIN alice\n");
    assert(result.command == "typed text\n");
    assert(out.str().find("hello while typing") != std::string::npos);
    assert(out.str().find("Server connection closed") != std::string::npos);
    assert(err.str().empty());
}

void test_select_loop_prints_notice_when_server_closes()
{
    Fd listener(create_loopback_listener());
    int port = listener_port(listener.get());

    std::thread server([&]() {
        Fd client(accept(listener.get(), nullptr, nullptr));
        assert(client.get() >= 0);
        assert(recv_line(client.get()) == "LOGIN alice\n");
        send_all(client.get(), "OK welcome\n");
    });

    Pipe input;
    std::ostringstream out;
    std::ostringstream err;
    chatroom::ChatClient client(out, err);
    assert(client.connect_and_login({"127.0.0.1", port, "alice"}));

    bool ok = client.run_event_loop(input.read_fd(), echo_input_as_command);

    server.join();
    assert(!ok);
    assert(out.str().find("Server connection closed") != std::string::npos);
    assert(err.str().empty());
}

}  // namespace

int main()
{
    test_select_loop_processes_input_and_server_messages_without_blocking();
    test_select_loop_prints_notice_when_server_closes();

    std::cout << "client select loop tests passed\n";
    return 0;
}
