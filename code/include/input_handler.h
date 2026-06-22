#ifndef CHATROOM_INPUT_HANDLER_H
#define CHATROOM_INPUT_HANDLER_H

#include "client.h"

#include <string>

namespace chatroom {

class InputHandler {
public:
    UserInputResult parse(const std::string& line) const;
};

}  // namespace chatroom

#endif  // CHATROOM_INPUT_HANDLER_H
