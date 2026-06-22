#include "display.h"

#include <cassert>
#include <iostream>

namespace {

void test_system_message_is_readable()
{
    assert(chatroom::Display::format_server_line("SYSTEM alice joined") ==
           "[system] alice joined");
}

void test_chat_message_shows_sender_and_content()
{
    assert(chatroom::Display::format_server_line("CHAT alice hello everyone") ==
           "[group] alice: hello everyone");
}

void test_private_message_shows_sender_and_content()
{
    assert(chatroom::Display::format_server_line("PRIVATE bob quiet hello") ==
           "[private] bob: quiet hello");
}

void test_userlist_ok_and_error_have_meaningful_labels()
{
    assert(chatroom::Display::format_server_line("USERLIST alice bob") ==
           "Online users: alice bob");
    assert(chatroom::Display::format_server_line("OK welcome") == "OK: welcome");
    assert(chatroom::Display::format_server_line("ERROR nickname required") ==
           "Error: nickname required");
}

}  // namespace

int main()
{
    test_system_message_is_readable();
    test_chat_message_shows_sender_and_content();
    test_private_message_shows_sender_and_content();
    test_userlist_ok_and_error_have_meaningful_labels();

    std::cout << "display tests passed\n";
    return 0;
}
