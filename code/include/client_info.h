#ifndef CHATROOM_CLIENT_INFO_H
#define CHATROOM_CLIENT_INFO_H

#include <string>

namespace chatroom {

struct ClientInfo {
    int fd = -1;
    std::string nickname;
    bool logged_in = false;
};

}  // namespace chatroom

#endif  // CHATROOM_CLIENT_INFO_H
