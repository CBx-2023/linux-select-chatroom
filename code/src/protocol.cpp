#include "protocol.h"

#include <cstddef>
#include <string>
#include <vector>

namespace chatroom {

namespace {

Command invalid_command(const std::string& error) {
    Command command;
    command.type = CommandType::Invalid;
    command.error = error;
    return command;
}

bool is_ascii_letter_or_digit(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9');
}

std::string without_trailing_cr(const std::string& line) {
    if (!line.empty() && line.back() == '\r') {
        return line.substr(0, line.size() - 1);
    }
    return line;
}

std::string join_nicknames(const std::vector<std::string>& nicknames) {
    std::string joined;
    for (std::size_t i = 0; i < nicknames.size(); ++i) {
        if (i != 0) {
            joined += ',';
        }
        joined += nicknames[i];
    }
    return joined;
}

}  // namespace

std::vector<std::string> LineBuffer::append(const std::string& bytes) {
    buffer_ += bytes;

    std::vector<std::string> lines;
    std::size_t start = 0;
    for (;;) {
        const std::size_t newline = buffer_.find('\n', start);
        if (newline == std::string::npos) {
            break;
        }

        lines.push_back(buffer_.substr(start, newline - start));
        start = newline + 1;
    }

    if (start != 0) {
        buffer_.erase(0, start);
    }

    return lines;
}

const std::string& LineBuffer::pending() const {
    return buffer_;
}

void LineBuffer::clear() {
    buffer_.clear();
}

bool is_valid_nickname(const std::string& nickname) {
    if (nickname.empty() || nickname.size() > kMaxNicknameLength) {
        return false;
    }

    for (char c : nickname) {
        if (c != '_' && !is_ascii_letter_or_digit(c)) {
            return false;
        }
    }

    return true;
}

Command parse_client_command(const std::string& raw_line) {
    if (raw_line.size() > kMaxMessageBytes) {
        return invalid_command("message too long");
    }

    const std::string line = without_trailing_cr(raw_line);
    const std::size_t first_space = line.find(' ');
    const std::string verb =
        first_space == std::string::npos ? line : line.substr(0, first_space);
    const std::string rest =
        first_space == std::string::npos ? "" : line.substr(first_space + 1);

    if (verb == "LOGIN") {
        if (rest.empty()) {
            return invalid_command("nickname required");
        }
        if (rest.find(' ') != std::string::npos) {
            return invalid_command("invalid command");
        }
        if (!is_valid_nickname(rest)) {
            return invalid_command("invalid nickname");
        }

        Command command;
        command.type = CommandType::Login;
        command.nickname = rest;
        return command;
    }

    if (verb == "MSG") {
        if (rest.empty()) {
            return invalid_command("invalid command");
        }

        Command command;
        command.type = CommandType::Message;
        command.content = rest;
        return command;
    }

    if (verb == "PRIVATE") {
        const std::size_t target_end = rest.find(' ');
        if (rest.empty() || target_end == std::string::npos ||
            target_end + 1 >= rest.size()) {
            return invalid_command("invalid command");
        }

        const std::string target = rest.substr(0, target_end);
        if (!is_valid_nickname(target)) {
            return invalid_command("invalid nickname");
        }

        Command command;
        command.type = CommandType::PrivateMessage;
        command.target = target;
        command.content = rest.substr(target_end + 1);
        return command;
    }

    if (verb == "LIST") {
        if (first_space != std::string::npos) {
            return invalid_command("invalid command");
        }

        Command command;
        command.type = CommandType::List;
        return command;
    }

    if (verb == "QUIT") {
        if (first_space != std::string::npos) {
            return invalid_command("invalid command");
        }

        Command command;
        command.type = CommandType::Quit;
        return command;
    }

    return invalid_command("invalid command");
}

std::string make_ok(const std::string& message) {
    return "OK " + message + '\n';
}

std::string make_error(const std::string& reason) {
    return "ERROR " + reason + '\n';
}

std::string make_system(const std::string& message) {
    return "SYSTEM " + message + '\n';
}

std::string make_chat(const std::string& nickname, const std::string& content) {
    return "CHAT " + nickname + ' ' + content + '\n';
}

std::string make_private(const std::string& from, const std::string& content) {
    return "PRIVATE " + from + ' ' + content + '\n';
}

std::string make_userlist(const std::vector<std::string>& nicknames) {
    return "USERLIST " + join_nicknames(nicknames) + '\n';
}

}  // namespace chatroom
