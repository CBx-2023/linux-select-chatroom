#ifndef CHATROOM_DISPLAY_H
#define CHATROOM_DISPLAY_H

#include <string>

namespace chatroom {

class Display {
public:
    static std::string format_server_line(const std::string& line);
};

}  // namespace chatroom

#endif  // CHATROOM_DISPLAY_H
