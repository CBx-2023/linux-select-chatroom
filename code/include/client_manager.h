#ifndef CHATROOM_CLIENT_MANAGER_H
#define CHATROOM_CLIENT_MANAGER_H

#include "client_info.h"

#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace chatroom {

class ClientManager {
public:
    bool add_client(int fd);
    bool remove_client(int fd);
    bool login(int fd, const std::string& nickname);

    ClientInfo* find_by_fd(int fd);
    const ClientInfo* find_by_fd(int fd) const;
    ClientInfo* find_by_nickname(const std::string& nickname);
    const ClientInfo* find_by_nickname(const std::string& nickname) const;

    bool nickname_in_use(const std::string& nickname) const;
    std::vector<std::string> online_users() const;
    std::vector<int> fds() const;
    std::size_t client_count() const;

private:
    std::map<int, ClientInfo> clients_by_fd_;
    std::map<std::string, int> fd_by_nickname_;
};

}  // namespace chatroom

#endif  // CHATROOM_CLIENT_MANAGER_H
