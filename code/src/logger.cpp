#include "logger.h"

#include <fstream>
#include <iostream>
#include <string>
#include <utility>

namespace chatroom {

namespace {

std::string sanitize_field(const std::string& value) {
    std::string sanitized = value;
    for (char& c : sanitized) {
        if (c == '\n' || c == '\r') {
            c = ' ';
        }
    }
    return sanitized;
}

}  // namespace

Logger::Logger(std::string path) : path_(std::move(path)) {}

void Logger::log_startup(int port) const {
    write_line("STARTUP port=" + std::to_string(port));
}

void Logger::log_login(const std::string& nickname,
                       const std::string& address) const {
    std::string line = "LOGIN nickname=" + sanitize_field(nickname);
    if (!address.empty()) {
        line += " address=" + sanitize_field(address);
    }
    write_line(line);
}

void Logger::log_quit(const std::string& nickname) const {
    write_line("QUIT nickname=" + sanitize_field(nickname));
}

void Logger::log_abnormal_disconnect(const std::string& nickname) const {
    write_line("ABNORMAL_DISCONNECT nickname=" + sanitize_field(nickname));
}

void Logger::log_group_chat(const std::string& from,
                            const std::string& content) const {
    write_line("GROUP_CHAT from=" + sanitize_field(from) +
               " content=" + sanitize_field(content));
}

void Logger::log_private_chat(const std::string& from,
                              const std::string& to,
                              const std::string& content) const {
    write_line("PRIVATE_CHAT from=" + sanitize_field(from) +
               " to=" + sanitize_field(to) +
               " content=" + sanitize_field(content));
}

void Logger::log_error(const std::string& message) const {
    write_line("ERROR message=" + sanitize_field(message));
}

void Logger::write_line(const std::string& line) const {
    std::ofstream out(path_, std::ios::app);
    if (!out.is_open()) {
        std::cerr << "logger error: unable to open " << path_
                  << " for append; event=" << line << '\n';
        return;
    }

    out << line << '\n';
    out.flush();
    if (!out) {
        std::cerr << "logger error: unable to write " << path_
                  << "; event=" << line << '\n';
    }
}

}  // namespace chatroom
