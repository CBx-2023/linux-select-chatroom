#ifndef CHATROOM_CLIENT_INFO_H
#define CHATROOM_CLIENT_INFO_H

#include "protocol.h"

#include <string>

namespace chatroom {

struct ClientInfo {
    int fd = -1;
    std::string nickname;
    bool logged_in = false;
    LineBuffer receive_buffer;
};

}  // namespace chatroom

#endif  // CHATROOM_CLIENT_INFO_H
