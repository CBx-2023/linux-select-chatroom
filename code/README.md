# TCP Select Chatroom

Linux C++ course project implementing a multi-user chatroom with TCP sockets
and `select`.

## Build

From this directory:

```sh
make
```

The build produces two executables:

- `server`
- `client`

To remove generated files:

```sh
make clean
```

## Run

Start the server on a port:

```sh
./server 8888
```

Start each client in a separate terminal:

```sh
./client 127.0.0.1 8888 alice
./client 127.0.0.1 8888 bob
./client 127.0.0.1 8888 carol
```

The client sends `LOGIN <nickname>` after connecting. Nicknames must be 1 to
20 characters and may contain only letters, digits, and underscores.

## Client Commands

| Input | Effect |
| --- | --- |
| `hello everyone` | Sends a group chat message as `MSG hello everyone` |
| `/w bob hello` | Sends a private message as `PRIVATE bob hello` |
| `/list` | Requests the online user list with `LIST` |
| `/quit` | Sends `QUIT` and exits the client |

Empty input is ignored. Invalid `/w` input is handled locally and is not sent
to the server.

## Expected Behavior

- The server uses one process with `select` to handle the listening socket and
  connected client sockets.
- A client must log in before sending chat commands.
- Duplicate nicknames are rejected.
- Group chat messages are broadcast to other online users.
- Private messages are delivered only to the target nickname.
- `/list` returns the current online nickname list to the requester.
- `/quit` and abnormal disconnects remove the user from server state.
- Protocol lines longer than 1024 bytes are rejected.
- Server events are written to `server.log`.

Out of scope for this project: account passwords, database storage, offline
messages, chat-history lookup, GUI, admin permissions, and kick commands.

## Manual Test Flow

1. Build with `make`.
2. Start `./server 8888`.
3. Start at least three clients with unique nicknames.
4. Type normal text in one client and confirm the other clients receive it.
5. Run `/w bob hello` and confirm only `bob` receives the private message.
6. Run `/list` and confirm the online users are shown.
7. Try a duplicate nickname and confirm it is rejected.
8. Send an invalid command with a raw TCP tool and confirm an error response.
9. Send a line longer than 1024 bytes and confirm the server stays running.
10. Run `/quit` from one client and confirm the user is removed.
11. Close a client terminal abruptly and confirm the server cleans up the fd.
12. Inspect `server.log` for startup, login, quit, abnormal disconnect, group
    chat, private chat, and error entries.

Use `validation_checklist.md` to record pass/fail results and evidence during
the final end-to-end run.
