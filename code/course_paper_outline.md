# Course Paper Outline

This outline is a writing and evidence checklist for the Linux TCP chatroom
course paper. It is not the final paper text.

## 1. Background and Goals

- Explain the course design topic: a Linux TCP socket multi-user chatroom.
- State the implemented goals: server, client, multi-client chat, private
  messages, online user list, exit handling, abnormal disconnect handling, and
  server-side logging.
- State non-goals: account passwords, database storage, offline messages,
  history lookup, GUI, admin permissions, and kick commands.

Evidence to capture:

- Screenshot of the final `code/` directory.
- Screenshot of successful compilation.

## 2. System Architecture

- Describe the two executable programs: `server` and `client`.
- Explain the shared text protocol module and server log utility.
- Show the high-level flow: client connects, sends `LOGIN`, server tracks fd
  and nickname, then routes commands.

Evidence to capture:

- Key code screenshot of the shared protocol declarations.
- Key code screenshot of the main server/client entry points.

## 3. TCP Socket Principle

- Explain server socket creation, `bind`, `listen`, and `accept`.
- Explain client socket creation and `connect`.
- Explain line-delimited TCP messages and why receive buffers are needed for
  partial packets and sticky packets.

Evidence to capture:

- Server socket setup code screenshot.
- Client connection code screenshot.

## 4. Select I/O Multiplexing Design

- Explain why the server uses a single-process `select` loop.
- Explain how the listening fd and client fds are added to `fd_set`.
- Explain how the client uses `select` to monitor stdin and the server socket.

Evidence to capture:

- Server `select` loop screenshot.
- Client `select` loop screenshot.
- Runtime screenshot with multiple clients connected.

## 5. Protocol Design

- Document client commands: `LOGIN`, `MSG`, `PRIVATE`, `LIST`, `QUIT`.
- Document server responses: `OK`, `ERROR`, `SYSTEM`, `CHAT`, `PRIVATE`,
  `USERLIST`.
- Explain nickname rules and the 1024-byte message limit.

Evidence to capture:

- Protocol parsing/generation code screenshot.
- Test evidence for valid and invalid command parsing.

## 6. Server Implementation

- Describe client state management by fd and nickname.
- Describe login, duplicate nickname rejection, normal quit, and abnormal
  disconnect cleanup.
- Describe group chat broadcast, private message delivery, and online list
  response.
- Describe error handling for invalid commands, not-logged-in commands, missing
  private targets, long messages, and send/recv errors.
- Describe server log records for startup, login, quit, abnormal disconnect,
  group chat, private chat, and errors.

Evidence to capture:

- Client manager code screenshot.
- Message routing code screenshot.
- Cleanup/error-handling code screenshot.
- `server.log` screenshot after manual tests.

## 7. Client Implementation

- Describe command-line arguments: server IP, port, nickname.
- Describe login command transmission and login failure display.
- Describe stdin/server-socket `select` loop.
- Describe input mapping: normal text, `/w`, `/list`, and `/quit`.
- Describe terminal display formatting for system notices, group chat, private
  chat, user list, confirmations, and errors.

Evidence to capture:

- Client startup/login code screenshot.
- Input handler code screenshot.
- Display formatting code screenshot.

## 8. Key Code Screenshots

Capture these code sections for the final paper:

- `protocol.h` and protocol parsing/generation implementation.
- Server socket setup and `select` event loop.
- ClientManager add, login, lookup, list, and remove behavior.
- Server routing for `MSG`, `PRIVATE`, `LIST`, `QUIT`, and disconnect cleanup.
- Logger event-writing implementation.
- Client connection/login code.
- Client input mapping and display formatting.
- Makefile build targets.

## 9. Test Process and Runtime Screenshots

Capture the following runtime tests:

- `make` succeeds.
- Server starts with `./server 8888`.
- Client A logs in.
- Client B and Client C log in.
- Group chat from one client appears on the other online clients.
- Private chat is received only by the target client.
- `/list` shows current online users.
- Duplicate nickname is rejected.
- Invalid command receives an error.
- Long message receives `ERROR message too long`.
- `/quit` cleans up the user and broadcasts offline notice.
- Abrupt client close is detected and logged.
- `server.log` contains the expected event lines.

## 10. Summary and Improvement Direction

- Summarize how Linux TCP socket programming and `select` were applied.
- Summarize how the protocol, client state, routing, and logging cooperate.
- Mention reasonable future improvements outside the approved scope, such as
  authentication, persistence, history lookup, GUI, or admin tools.
