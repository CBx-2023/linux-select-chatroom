#include "display.h"

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

std::string payload_after_prefix(const std::string& line, const std::string& prefix)
{
    return trim(line.substr(prefix.size()));
}

std::string format_sender_message(const std::string& label, const std::string& payload)
{
    std::istringstream stream(payload);
    std::string sender;
    stream >> sender;

    std::string content;
    std::getline(stream, content);
    content = trim(content);

    if (sender.empty()) {
        return label + " " + payload;
    }
    if (content.empty()) {
        return label + " " + sender;
    }
    return label + " " + sender + ": " + content;
}

bool starts_with_prefix(const std::string& line, const std::string& prefix)
{
    return line == prefix || line.compare(0, prefix.size() + 1, prefix + " ") == 0;
}

}  // namespace

std::string Display::format_server_line(const std::string& line)
{
    std::string trimmed = trim(line);
    if (starts_with_prefix(trimmed, "SYSTEM")) {
        return "[system] " + payload_after_prefix(trimmed, "SYSTEM");
    }
    if (starts_with_prefix(trimmed, "CHAT")) {
        return format_sender_message("[group]", payload_after_prefix(trimmed, "CHAT"));
    }
    if (starts_with_prefix(trimmed, "PRIVATE")) {
        return format_sender_message("[private]", payload_after_prefix(trimmed, "PRIVATE"));
    }
    if (starts_with_prefix(trimmed, "USERLIST")) {
        std::string payload = payload_after_prefix(trimmed, "USERLIST");
        return payload.empty() ? "Online users: (none)" : "Online users: " + payload;
    }
    if (starts_with_prefix(trimmed, "OK")) {
        std::string payload = payload_after_prefix(trimmed, "OK");
        return payload.empty() ? "OK" : "OK: " + payload;
    }
    if (starts_with_prefix(trimmed, "ERROR")) {
        std::string payload = payload_after_prefix(trimmed, "ERROR");
        return payload.empty() ? "Error" : "Error: " + payload;
    }
    return trimmed;
}

}  // namespace chatroom
