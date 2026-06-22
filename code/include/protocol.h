#ifndef CHATROOM_PROTOCOL_H
#define CHATROOM_PROTOCOL_H

#include <cstddef>
#include <string>
#include <vector>

namespace chatroom {

constexpr std::size_t kMaxMessageBytes = 1024;
constexpr std::size_t kMaxNicknameLength = 20;

enum class CommandType {
    Login,
    Message,
    PrivateMessage,
    List,
    Quit,
    Invalid
};

enum class ResponseType {
    Ok,
    Error,
    System,
    Chat,
    PrivateMessage,
    UserList
};

struct Command {
    CommandType type = CommandType::Invalid;
    std::string nickname;
    std::string target;
    std::string content;
    std::string error;
};

class LineBuffer {
public:
    std::vector<std::string> append(const std::string& bytes);
    const std::string& pending() const;
    void clear();

private:
    std::string buffer_;
};

bool is_valid_nickname(const std::string& nickname);
Command parse_client_command(const std::string& line);

std::string make_ok(const std::string& message);
std::string make_error(const std::string& reason);
std::string make_system(const std::string& message);
std::string make_chat(const std::string& nickname, const std::string& content);
std::string make_private(const std::string& from, const std::string& content);
std::string make_userlist(const std::vector<std::string>& nicknames);

}  // namespace chatroom

#endif  // CHATROOM_PROTOCOL_H
