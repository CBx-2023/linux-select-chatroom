#include "client_manager.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <vector>

namespace {

void test_client_manager_tracks_login_lookup_duplicates_and_removal()
{
    chatroom::ClientManager manager;

    assert(manager.add_client(10));
    assert(manager.add_client(11));
    assert(!manager.add_client(10));
    assert(manager.client_count() == 2);

    const chatroom::ClientInfo* fd_client = manager.find_by_fd(10);
    assert(fd_client != nullptr);
    assert(fd_client->fd == 10);
    assert(!fd_client->logged_in);
    assert(fd_client->nickname.empty());

    assert(manager.login(10, "alice"));
    assert(manager.login(11, "bob"));
    assert(!manager.login(11, "alice"));

    const chatroom::ClientInfo* alice = manager.find_by_nickname("alice");
    assert(alice != nullptr);
    assert(alice->fd == 10);
    assert(alice->logged_in);
    assert(manager.nickname_in_use("bob"));

    std::vector<std::string> users = manager.online_users();
    std::sort(users.begin(), users.end());
    assert((users == std::vector<std::string>{"alice", "bob"}));

    assert(manager.remove_client(10));
    assert(manager.find_by_fd(10) == nullptr);
    assert(manager.find_by_nickname("alice") == nullptr);
    assert(!manager.nickname_in_use("alice"));
    assert(manager.client_count() == 1);
    assert(!manager.remove_client(10));
}

}  // namespace

int main()
{
    test_client_manager_tracks_login_lookup_duplicates_and_removal();
    std::cout << "client_manager_tests passed\n";
    return 0;
}
