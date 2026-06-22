#ifndef CHATROOM_CLIENT_H
#define CHATROOM_CLIENT_H

#include <iosfwd>
#include <functional>
#include <string>

namespace chatroom {

struct ClientOptions {
    std::string server_ip;
    int port = 0;
    std::string nickname;
};

struct UserInputResult {
    std::string command;
    std::string local_error;
    bool should_exit = false;
};

using UserInputProcessor = std::function<UserInputResult(const std::string&)>;

bool parse_client_options(int argc, char* argv[], ClientOptions& options, std::ostream& err);

class ChatClient {
public:
    ChatClient(std::ostream& out, std::ostream& err);
    ~ChatClient();

    ChatClient(const ChatClient&) = delete;
    ChatClient& operator=(const ChatClient&) = delete;

    bool connect_and_login(const ClientOptions& options);
    bool run_event_loop(int input_fd, const UserInputProcessor& process_input);
    void close_connection();

private:
    bool send_all(const std::string& data);
    std::string receive_line();

    std::ostream& out_;
    std::ostream& err_;
    int socket_fd_ = -1;
};

}  // namespace chatroom

#endif  // CHATROOM_CLIENT_H
