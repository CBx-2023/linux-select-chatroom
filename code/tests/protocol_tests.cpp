#include "protocol.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

void require_equal(const std::string& actual,
                   const std::string& expected,
                   const std::string& message) {
    if (actual != expected) {
        std::cerr << "FAIL: " << message << "\nexpected: [" << expected
                  << "]\nactual:   [" << actual << "]\n";
        std::exit(1);
    }
}

void require_lines(const std::vector<std::string>& actual,
                   const std::vector<std::string>& expected,
                   const std::string& message) {
    if (actual != expected) {
        std::cerr << "FAIL: " << message << "\nexpected lines: "
                  << expected.size() << "\nactual lines:   " << actual.size()
                  << '\n';
        std::exit(1);
    }
}

void require_invalid(const chatroom::Command& command,
                     const std::string& expected_error,
                     const std::string& message) {
    require(command.type == chatroom::CommandType::Invalid, message);
    require_equal(command.error, expected_error, message + " error");
}

void test_line_buffer_emits_complete_lines_and_retains_partial() {
    chatroom::LineBuffer buffer;

    auto lines = buffer.append("LOGIN alice\nMSG hello");
    require_lines(lines, {"LOGIN alice"}, "first complete line emitted");
    require_equal(buffer.pending(), "MSG hello", "partial line retained");

    lines = buffer.append(" world\r\nLIST\nQUIT");
    require_lines(lines, {"MSG hello world\r", "LIST"},
                  "sticky packet emits all complete lines");
    require_equal(buffer.pending(), "QUIT", "new partial line retained");

    lines = buffer.append("\n");
    require_lines(lines, {"QUIT"}, "final partial line emitted after newline");
    require(buffer.pending().empty(), "pending buffer cleared after newline");
}

void test_nickname_rules() {
    require(chatroom::is_valid_nickname("alice_01"), "valid nickname accepted");
    require(!chatroom::is_valid_nickname(""), "empty nickname rejected");
    require(!chatroom::is_valid_nickname(std::string(21, 'a')),
            "too-long nickname rejected");
    require(!chatroom::is_valid_nickname("bad-name"),
            "non-alnum underscore nickname rejected");
}

void test_parse_valid_commands() {
    auto login = chatroom::parse_client_command("LOGIN alice\r");
    require(login.type == chatroom::CommandType::Login, "LOGIN parsed");
    require_equal(login.nickname, "alice", "LOGIN nickname parsed");

    auto message = chatroom::parse_client_command("MSG hello world");
    require(message.type == chatroom::CommandType::Message, "MSG parsed");
    require_equal(message.content, "hello world", "MSG content parsed");

    auto private_message =
        chatroom::parse_client_command("PRIVATE bob private note");
    require(private_message.type == chatroom::CommandType::PrivateMessage,
            "PRIVATE parsed");
    require_equal(private_message.target, "bob", "PRIVATE target parsed");
    require_equal(private_message.content, "private note",
                  "PRIVATE content parsed");

    require(chatroom::parse_client_command("LIST").type ==
                chatroom::CommandType::List,
            "LIST parsed");
    require(chatroom::parse_client_command("QUIT").type ==
                chatroom::CommandType::Quit,
            "QUIT parsed");
}

void test_parse_invalid_commands_return_structured_errors() {
    require_invalid(chatroom::parse_client_command("LOGIN"), "nickname required",
                    "LOGIN without nickname rejected");
    require_invalid(chatroom::parse_client_command("LOGIN bad-name"),
                    "invalid nickname", "invalid LOGIN nickname rejected");
    require_invalid(chatroom::parse_client_command("LOGIN alice extra"),
                    "invalid command", "extra LOGIN argument rejected");
    require_invalid(chatroom::parse_client_command("MSG"), "invalid command",
                    "MSG without content rejected");
    require_invalid(chatroom::parse_client_command("PRIVATE bob"),
                    "invalid command", "PRIVATE without content rejected");
    require_invalid(chatroom::parse_client_command("PRIVATE bad-name hello"),
                    "invalid nickname", "invalid PRIVATE target rejected");
    require_invalid(chatroom::parse_client_command("LIST now"),
                    "invalid command", "extra LIST argument rejected");
    require_invalid(chatroom::parse_client_command("QUIT now"),
                    "invalid command", "extra QUIT argument rejected");
    require_invalid(chatroom::parse_client_command("UNKNOWN"),
                    "invalid command", "unknown command rejected");
}

void test_parse_enforces_1024_byte_limit() {
    const std::string largest_allowed = "MSG " + std::string(1020, 'x');
    require(largest_allowed.size() == chatroom::kMaxMessageBytes,
            "test fixture hits exact max size");
    require(chatroom::parse_client_command(largest_allowed).type ==
                chatroom::CommandType::Message,
            "1024-byte command accepted");

    const std::string too_large = "MSG " + std::string(1021, 'x');
    require_invalid(chatroom::parse_client_command(too_large),
                    "message too long", "1025-byte command rejected");
}

void test_parse_ignores_optional_cr_before_1024_byte_limit() {
    const std::string largest_allowed_with_cr =
        "MSG " + std::string(1020, 'x') + "\r";
    require(largest_allowed_with_cr.size() == chatroom::kMaxMessageBytes + 1,
            "test fixture includes exact max payload plus CR");
    require(chatroom::parse_client_command(largest_allowed_with_cr).type ==
                chatroom::CommandType::Message,
            "1024-byte command with trailing CR accepted");

    const std::string too_large_with_cr =
        "MSG " + std::string(1021, 'x') + "\r";
    require_invalid(chatroom::parse_client_command(too_large_with_cr),
                    "message too long",
                    "1025-byte command with trailing CR rejected");
}

void test_response_helpers_are_newline_terminated() {
    require_equal(chatroom::make_ok("welcome"), "OK welcome\n", "OK response");
    require_equal(chatroom::make_error("bad"), "ERROR bad\n",
                  "ERROR response");
    require_equal(chatroom::make_system("alice joined"), "SYSTEM alice joined\n",
                  "SYSTEM response");
    require_equal(chatroom::make_chat("alice", "hello"), "CHAT alice hello\n",
                  "CHAT response");
    require_equal(chatroom::make_private("alice", "private note"),
                  "PRIVATE alice private note\n", "PRIVATE response");
    require_equal(chatroom::make_userlist({"alice", "bob"}),
                  "USERLIST alice,bob\n", "USERLIST response");
}

}  // namespace

int main() {
    test_line_buffer_emits_complete_lines_and_retains_partial();
    test_nickname_rules();
    test_parse_valid_commands();
    test_parse_invalid_commands_return_structured_errors();
    test_parse_enforces_1024_byte_limit();
    test_parse_ignores_optional_cr_before_1024_byte_limit();
    test_response_helpers_are_newline_terminated();
    return 0;
}
