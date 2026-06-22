#include "input_handler.h"

#include <sstream>

namespace chatroom {
namespace {

std::string trim(const std::string& value)
{
    const std::string whitespace = " \t\r\n";
    std::size_t first = value.find_first_not_of(whitespace);
    if (first == std::string::npos) {
        return "";
    }

    std::size_t last = value.find_last_not_of(whitespace);
    return value.substr(first, last - first + 1);
}

UserInputResult local_error(const std::string& message)
{
    return {"", message, false};
}

}  // namespace

UserInputResult InputHandler::parse(const std::string& line) const
{
    std::string input = trim(line);
    if (input.empty()) {
        return {};
    }

    if (input[0] != '/') {
        return {"MSG " + input + "\n", "", false};
    }

    std::istringstream stream(input);
    std::string command;
    stream >> command;

    if (command == "/list") {
        std::string extra;
        if (stream >> extra) {
            return local_error("Usage: /list");
        }
        return {"LIST\n", "", false};
    }

    if (command == "/quit") {
        std::string extra;
        if (stream >> extra) {
            return local_error("Usage: /quit");
        }
        return {"QUIT\n", "", true};
    }

    if (command == "/w") {
        std::string target;
        if (!(stream >> target)) {
            return local_error("Usage: /w <nickname> <message>");
        }

        std::string content;
        std::getline(stream, content);
        content = trim(content);
        if (content.empty()) {
            return local_error("Usage: /w <nickname> <message>");
        }
        return {"PRIVATE " + target + " " + content + "\n", "", false};
    }

    return local_error("Unknown command: " + command);
}

}  // namespace chatroom
