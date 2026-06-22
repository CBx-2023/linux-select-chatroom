#include "logger.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

std::string read_file(const std::string& path) {
    std::ifstream in(path);
    require(in.is_open(), "log file opened for reading");
    return std::string((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
}

void require_contains(const std::string& haystack,
                      const std::string& needle,
                      const std::string& message) {
    require(haystack.find(needle) != std::string::npos, message);
}

void test_logger_writes_all_required_event_classes() {
    const std::string path = "/tmp/chatroom_logger_test.log";
    std::remove(path.c_str());

    chatroom::Logger logger(path);
    logger.log_startup(9000);
    logger.log_login("alice", "127.0.0.1");
    logger.log_quit("alice");
    logger.log_abnormal_disconnect("bob");
    logger.log_group_chat("alice", "hello everyone");
    logger.log_private_chat("alice", "bob", "private note");
    logger.log_error("send failed");

    const std::string contents = read_file(path);
    require_contains(contents, "STARTUP port=9000", "startup logged");
    require_contains(contents, "LOGIN nickname=alice address=127.0.0.1",
                     "login logged");
    require_contains(contents, "QUIT nickname=alice", "quit logged");
    require_contains(contents, "ABNORMAL_DISCONNECT nickname=bob",
                     "abnormal disconnect logged");
    require_contains(contents, "GROUP_CHAT from=alice content=hello everyone",
                     "group chat logged");
    require_contains(
        contents, "PRIVATE_CHAT from=alice to=bob content=private note",
        "private chat logged");
    require_contains(contents, "ERROR message=send failed", "error logged");

    std::remove(path.c_str());
}

void test_logger_reports_file_failures_to_stderr_without_throwing() {
    std::ostringstream captured;
    auto* original_stderr = std::cerr.rdbuf(captured.rdbuf());

    chatroom::Logger logger(
        "/tmp/chatroom_logger_missing_dir_for_test/server.log");
    logger.log_error("disk unavailable");

    std::cerr.rdbuf(original_stderr);

    const std::string output = captured.str();
    require_contains(output, "logger error:", "stderr failure prefix written");
    require_contains(output, "disk unavailable",
                     "stderr includes failed log event details");
}

}  // namespace

int main() {
    test_logger_writes_all_required_event_classes();
    test_logger_reports_file_failures_to_stderr_without_throwing();
    return 0;
}
