# Chatroom Validation Checklist

Use this checklist for manual end-to-end validation after `make` builds the
server and client binaries. Fill the result and evidence columns during the
actual run; do not pre-mark any scenario as passed.

## Environment

| Item | Result | Evidence |
| --- | --- | --- |
| Build with `make` | Pending | |
| Start server with `./server 8888` | Pending | |
| Start three clients with unique nicknames | Pending | |
| Confirm `server.log` is created or appended | Pending | |

## Functional Tests

| Scenario | Steps | Expected Result | Result | Evidence |
| --- | --- | --- | --- | --- |
| Single-client login | Start server, run `./client 127.0.0.1 8888 alice` | Client connects, sends `LOGIN alice`, and receives login confirmation | Pending | |
| Multi-client group chat | Start clients `alice`, `bob`, and `carol`; type normal text in one client | Other online clients receive `CHAT <sender> <content>`; sender is not echoed unless explicitly documented | Pending | |
| Private chat | In `alice`, run `/w bob hello` | Only `bob` receives the private message; unrelated clients do not receive it | Pending | |
| Online user list | In a logged-in client, run `/list` | Requester receives a `USERLIST` containing current online nicknames | Pending | |
| Normal quit | In a client, run `/quit` | Client exits cleanly; other users receive an offline system notice; server removes the fd/nickname | Pending | |

## Abnormal and Error Tests

| Scenario | Steps | Expected Result | Result | Evidence |
| --- | --- | --- | --- | --- |
| Duplicate nickname | Start one `alice`, then start another `alice` | Second login is rejected with `ERROR nickname already used` | Pending | |
| Not-logged-in command | Connect to server and send `MSG hello` before `LOGIN` using a raw TCP tool | Server returns `ERROR please login first` | Pending | |
| Invalid command | Send an unsupported command line | Server returns `ERROR invalid command` and keeps running | Pending | |
| Private target missing | Send `/w nobody hello` from a logged-in client | Sender receives `ERROR user not online` | Pending | |
| Long message | Send a protocol line longer than 1024 bytes | Server returns `ERROR message too long` and keeps unrelated clients connected | Pending | |
| Abrupt client close | Kill or close a client terminal without `/quit` | Server detects disconnect, cleans state, logs abnormal disconnect, and other clients remain usable | Pending | |
| Send/recv failure resilience | Close a client while other clients continue chatting | Affected fd is cleaned without stopping unrelated clients | Pending | |

## Terminal Output Checks

| Output Type | Expected Terminal Meaning | Result | Evidence |
| --- | --- | --- | --- |
| `SYSTEM` | Displayed as a system notice | Pending | |
| `CHAT` | Displayed with sender nickname and group-chat content | Pending | |
| `PRIVATE` | Displayed as a private message with sender nickname | Pending | |
| `USERLIST` | Displayed as the online user list | Pending | |
| `OK` | Displayed as a confirmation | Pending | |
| `ERROR` | Displayed as an error without being hidden | Pending | |

## Server Log Checks

| Event | Expected Log Evidence | Result | Evidence |
| --- | --- | --- | --- |
| Startup | Port/startup line is written | Pending | |
| Login | Successful nickname login is written | Pending | |
| Quit | Normal `/quit` event is written | Pending | |
| Abnormal disconnect | Unexpected disconnect is written | Pending | |
| Group chat | Sender and group-chat event are written | Pending | |
| Private chat | Sender, target, and private-chat event are written | Pending | |
| Error | Important validation/socket errors are written | Pending | |

## Final Evidence Package

Collect these artifacts for the course paper after validation:

- Screenshot of successful `make`.
- Screenshot of server startup.
- Screenshots of at least three client terminals during group chat.
- Screenshot showing private chat visible only to the target.
- Screenshot showing `/list` output.
- Screenshot showing duplicate nickname rejection.
- Screenshot showing invalid/long-message errors.
- Screenshot or excerpt of `server.log` with the required event types.
