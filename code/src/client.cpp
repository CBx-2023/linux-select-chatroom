#include "client.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>

namespace chatroom {
namespace {

bool starts_with(const std::string& value, const std::string& prefix)
{
    return value.compare(0, prefix.size(), prefix) == 0;
}

std::string strip_line_end(std::string value)
{
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) {
        value.pop_back();
    }
    return value;
}

bool pop_line(std::string& buffer, std::string& line)
{
    std::size_t newline = buffer.find('\n');
    if (newline == std::string::npos) {
        return false;
    }

    line = buffer.substr(0, newline + 1);
    buffer.erase(0, newline + 1);
    return true;
}

UserInputResult pass_through_input(const std::string& line)
{
    if (line.empty()) {
        return {};
    }
    return {line + "\n", "", false};
}

}  // namespace

bool parse_client_options(int argc, char* argv[], ClientOptions& options, std::ostream& err)
{
    if (argc != 4) {
        err << "Usage: " << (argc > 0 ? argv[0] : "client")
            << " <server_ip> <port> <nickname>\n";
        return false;
    }

    char* end = nullptr;
    errno = 0;
    long parsed_port = std::strtol(argv[2], &end, 10);
    if (errno != 0 || end == argv[2] || *end != '\0' || parsed_port < 1 ||
        parsed_port > std::numeric_limits<unsigned short>::max()) {
        err << "Invalid port: " << argv[2] << "\n";
        return false;
    }

    if (argv[3][0] == '\0') {
        err << "Invalid nickname: nickname is required\n";
        return false;
    }

    options.server_ip = argv[1];
    options.port = static_cast<int>(parsed_port);
    options.nickname = argv[3];
    return true;
}

ChatClient::ChatClient(std::ostream& out, std::ostream& err) : out_(out), err_(err) {}

ChatClient::~ChatClient()
{
    close_connection();
}

bool ChatClient::connect_and_login(const ClientOptions& options)
{
    close_connection();

    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
        err_ << "Connection failed to " << options.server_ip << ":" << options.port
             << ": " << std::strerror(errno) << "\n";
        return false;
    }

    sockaddr_in server_addr {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(static_cast<uint16_t>(options.port));
    if (inet_pton(AF_INET, options.server_ip.c_str(), &server_addr.sin_addr) != 1) {
        err_ << "Connection failed to " << options.server_ip << ":" << options.port
             << ": invalid IP address\n";
        close_connection();
        return false;
    }

    if (connect(socket_fd_, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
        err_ << "Connection failed to " << options.server_ip << ":" << options.port
             << ": " << std::strerror(errno) << "\n";
        close_connection();
        return false;
    }

    if (!send_all("LOGIN " + options.nickname + "\n")) {
        close_connection();
        return false;
    }

    std::string response = receive_line();
    if (response.empty()) {
        err_ << "Login failed: server closed the connection\n";
        close_connection();
        return false;
    }

    std::string printable = strip_line_end(response);
    if (starts_with(printable, "OK")) {
        out_ << "Connected to " << options.server_ip << ":" << options.port << " as "
             << options.nickname << "\n";
        return true;
    }

    err_ << "Login failed: " << printable << "\n";
    close_connection();
    return false;
}

bool ChatClient::run_event_loop(int input_fd, const UserInputProcessor& process_input)
{
    if (socket_fd_ < 0) {
        err_ << "Client is not connected\n";
        return false;
    }

    UserInputProcessor processor = process_input ? process_input : pass_through_input;
    std::string input_buffer;
    std::string server_buffer;

    while (true) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(socket_fd_, &read_fds);
        FD_SET(input_fd, &read_fds);

        int max_fd = socket_fd_ > input_fd ? socket_fd_ : input_fd;
        int ready = select(max_fd + 1, &read_fds, nullptr, nullptr, nullptr);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            err_ << "select failed: " << std::strerror(errno) << "\n";
            return false;
        }

        if (FD_ISSET(socket_fd_, &read_fds)) {
            char buffer[512];
            ssize_t count = recv(socket_fd_, buffer, sizeof(buffer), 0);
            if (count < 0) {
                if (errno == EINTR) {
                    continue;
                }
                err_ << "Receive failed: " << std::strerror(errno) << "\n";
                close_connection();
                return false;
            }
            if (count == 0) {
                if (!server_buffer.empty()) {
                    out_ << strip_line_end(server_buffer) << "\n";
                }
                out_ << "Server connection closed\n";
                close_connection();
                return false;
            }

            server_buffer.append(buffer, static_cast<std::size_t>(count));
            std::string line;
            while (pop_line(server_buffer, line)) {
                out_ << strip_line_end(line) << "\n";
            }
        }

        if (FD_ISSET(input_fd, &read_fds)) {
            char buffer[512];
            ssize_t count = read(input_fd, buffer, sizeof(buffer));
            if (count < 0) {
                if (errno == EINTR) {
                    continue;
                }
                err_ << "Input read failed: " << std::strerror(errno) << "\n";
                return false;
            }
            if (count == 0) {
                return true;
            }

            input_buffer.append(buffer, static_cast<std::size_t>(count));
            std::string line;
            while (pop_line(input_buffer, line)) {
                UserInputResult result = processor(strip_line_end(line));
                if (!result.local_error.empty()) {
                    err_ << result.local_error << "\n";
                }
                if (!result.command.empty() && !send_all(result.command)) {
                    close_connection();
                    return false;
                }
                if (result.should_exit) {
                    return true;
                }
            }
        }
    }
}

void ChatClient::close_connection()
{
    if (socket_fd_ >= 0) {
        close(socket_fd_);
        socket_fd_ = -1;
    }
}

bool ChatClient::send_all(const std::string& data)
{
    std::size_t sent = 0;
    while (sent < data.size()) {
        ssize_t count = send(socket_fd_, data.data() + sent, data.size() - sent, 0);
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            err_ << "Send failed: " << std::strerror(errno) << "\n";
            return false;
        }
        if (count == 0) {
            err_ << "Send failed: connection closed\n";
            return false;
        }
        sent += static_cast<std::size_t>(count);
    }
    return true;
}

std::string ChatClient::receive_line()
{
    std::string line;
    char ch = '\0';
    while (true) {
        ssize_t count = recv(socket_fd_, &ch, 1, 0);
        if (count > 0) {
            line.push_back(ch);
            if (ch == '\n') {
                return line;
            }
            continue;
        }
        if (count == 0) {
            return line;
        }
        if (errno == EINTR) {
            continue;
        }
        err_ << "Receive failed: " << std::strerror(errno) << "\n";
        return "";
    }
}

}  // namespace chatroom

#ifndef CHATROOM_CLIENT_NO_MAIN
int main(int argc, char* argv[])
{
    chatroom::ClientOptions options;
    if (!chatroom::parse_client_options(argc, argv, options, std::cerr)) {
        return 1;
    }

    chatroom::ChatClient client(std::cout, std::cerr);
    if (!client.connect_and_login(options)) {
        return 1;
    }
    return client.run_event_loop(STDIN_FILENO, {}) ? 0 : 1;
}
#endif
