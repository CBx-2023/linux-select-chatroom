#include "input_handler.h"

#include <cassert>
#include <iostream>
#include <string>

namespace {

void assert_result(const chatroom::UserInputResult& result,
                   const std::string& command,
                   const std::string& local_error,
                   bool should_exit)
{
    assert(result.command == command);
    assert(result.local_error == local_error);
    assert(result.should_exit == should_exit);
}

void test_normal_text_maps_to_group_message()
{
    chatroom::InputHandler handler;

    assert_result(handler.parse("hello everyone"), "MSG hello everyone\n", "", false);
    assert_result(handler.parse("  hello with spaces  "), "MSG hello with spaces\n", "", false);
}

void test_empty_input_is_ignored()
{
    chatroom::InputHandler handler;

    assert_result(handler.parse(""), "", "", false);
    assert_result(handler.parse("   "), "", "", false);
}

void test_private_message_maps_target_and_content()
{
    chatroom::InputHandler handler;

    assert_result(handler.parse("/w bob hello"), "PRIVATE bob hello\n", "", false);
    assert_result(handler.parse(" /w   bob   hello there  "), "PRIVATE bob hello there\n", "", false);
}

void test_invalid_private_message_stays_local()
{
    chatroom::InputHandler handler;

    assert_result(handler.parse("/w"), "", "Usage: /w <nickname> <message>", false);
    assert_result(handler.parse("/w bob"), "", "Usage: /w <nickname> <message>", false);
    assert_result(handler.parse("/w   "), "", "Usage: /w <nickname> <message>", false);
}

void test_list_maps_to_list_command()
{
    chatroom::InputHandler handler;

    assert_result(handler.parse("/list"), "LIST\n", "", false);
    assert_result(handler.parse("  /list  "), "LIST\n", "", false);
}

void test_quit_maps_to_quit_and_local_exit()
{
    chatroom::InputHandler handler;

    assert_result(handler.parse("/quit"), "QUIT\n", "", true);
}

void test_unknown_slash_command_stays_local()
{
    chatroom::InputHandler handler;

    assert_result(handler.parse("/dance"), "", "Unknown command: /dance", false);
}

}  // namespace

int main()
{
    test_normal_text_maps_to_group_message();
    test_empty_input_is_ignored();
    test_private_message_maps_target_and_content();
    test_invalid_private_message_stays_local();
    test_list_maps_to_list_command();
    test_quit_maps_to_quit_and_local_exit();
    test_unknown_slash_command_stays_local();

    std::cout << "input handler tests passed\n";
    return 0;
}
