#include "client_manager.h"

namespace chatroom {

bool ClientManager::add_client(int fd)
{
    if (fd < 0 || clients_by_fd_.find(fd) != clients_by_fd_.end()) {
        return false;
    }

    ClientInfo client;
    client.fd = fd;
    clients_by_fd_.emplace(fd, client);
    return true;
}

bool ClientManager::remove_client(int fd)
{
    auto it = clients_by_fd_.find(fd);
    if (it == clients_by_fd_.end()) {
        return false;
    }

    if (it->second.logged_in) {
        fd_by_nickname_.erase(it->second.nickname);
    }
    clients_by_fd_.erase(it);
    return true;
}

bool ClientManager::login(int fd, const std::string& nickname)
{
    if (nickname.empty()) {
        return false;
    }

    auto client_it = clients_by_fd_.find(fd);
    if (client_it == clients_by_fd_.end()) {
        return false;
    }

    auto nickname_it = fd_by_nickname_.find(nickname);
    if (nickname_it != fd_by_nickname_.end() && nickname_it->second != fd) {
        return false;
    }

    if (client_it->second.logged_in) {
        fd_by_nickname_.erase(client_it->second.nickname);
    }

    client_it->second.nickname = nickname;
    client_it->second.logged_in = true;
    fd_by_nickname_[nickname] = fd;
    return true;
}

ClientInfo* ClientManager::find_by_fd(int fd)
{
    auto it = clients_by_fd_.find(fd);
    if (it == clients_by_fd_.end()) {
        return nullptr;
    }
    return &it->second;
}

const ClientInfo* ClientManager::find_by_fd(int fd) const
{
    auto it = clients_by_fd_.find(fd);
    if (it == clients_by_fd_.end()) {
        return nullptr;
    }
    return &it->second;
}

ClientInfo* ClientManager::find_by_nickname(const std::string& nickname)
{
    auto nickname_it = fd_by_nickname_.find(nickname);
    if (nickname_it == fd_by_nickname_.end()) {
        return nullptr;
    }
    return find_by_fd(nickname_it->second);
}

const ClientInfo* ClientManager::find_by_nickname(const std::string& nickname) const
{
    auto nickname_it = fd_by_nickname_.find(nickname);
    if (nickname_it == fd_by_nickname_.end()) {
        return nullptr;
    }
    return find_by_fd(nickname_it->second);
}

bool ClientManager::nickname_in_use(const std::string& nickname) const
{
    return fd_by_nickname_.find(nickname) != fd_by_nickname_.end();
}

std::vector<std::string> ClientManager::online_users() const
{
    std::vector<std::string> users;
    for (const auto& entry : clients_by_fd_) {
        const ClientInfo& client = entry.second;
        if (client.logged_in) {
            users.push_back(client.nickname);
        }
    }
    return users;
}

std::vector<int> ClientManager::fds() const
{
    std::vector<int> fds;
    for (const auto& entry : clients_by_fd_) {
        fds.push_back(entry.first);
    }
    return fds;
}

std::size_t ClientManager::client_count() const
{
    return clients_by_fd_.size();
}

}  // namespace chatroom
