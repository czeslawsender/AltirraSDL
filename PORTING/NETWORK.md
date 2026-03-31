# Network Socket Emulation

## Current Architecture (Windows)

```
ATNetwork (emulated network stack)
    |  (clean interface, no Win32 types)
    v
ATNetworkSockets
    |
    +-- nativesockets.cpp  (WSAStartup, worker thread creation)
    +-- worker.cpp         (HWND message pump + WSAAsyncSelect)
    +-- socketworker.cpp   (socket state machines)
    |
    v
Winsock2 API
```

The `ATNetwork` library (emulated Atari network stack) is completely
platform-agnostic. The `ATNetworkSockets` library bridges to real OS
sockets. It is the **most deeply Win32-entangled** core module:

- `worker.cpp` creates a hidden `HWND` and uses `WSAAsyncSelect()` to
  receive socket events as Windows messages
- A custom `WndProc` dispatches `MYWM_TCP_SOCKET` / `MYWM_UDP_SOCKET`
  messages
- Uses `RegisterClass()`, `CreateWindowEx()`, `WNDCLASS`, `LRESULT`,
  `WPARAM`, `LPARAM`
- DNS resolution via Winsock functions

This architectural pattern (async sockets via window messages) has no
equivalent on other platforms. The entire async I/O model must be replaced.

## SDL3_net Architecture

SDL3_net provides cross-platform TCP/UDP networking with non-blocking I/O:

```
ATNetwork (emulated network stack, unchanged)
    |
    v
ATNetworkSocketsSDL3 (new implementation)
    |
    +-- nativesockets_sdl3.cpp  (SDL3_net initialization)
    +-- worker_sdl3.cpp         (polling-based socket management)
    |
    v
SDL3_net API (NET_CreateClient, NET_SendDatagram, etc.)
```

### Key SDL3_net Functions

| Purpose | SDL3_net API |
|---------|-------------|
| TCP client | `NET_CreateClient(address, port)` |
| TCP server | `NET_CreateServer(address, port)`, `NET_AcceptClient()` |
| UDP socket | `NET_CreateDatagramSocket(address, port)` |
| Send TCP | `NET_WriteToStreamSocket(socket, data, len)` |
| Receive TCP | `NET_ReadFromStreamSocket(socket, buf, len)` |
| Send UDP | `NET_SendDatagram(socket, address, port, data, len)` |
| Receive UDP | `NET_ReceiveDatagram(socket, &datagram)` |
| Non-blocking check | `NET_WaitUntilInputAvailable(socket, timeout)` |
| Connection status | `NET_GetConnectionStatus(socket)` |
| DNS | `NET_ResolveHostname(hostname)` (async, background thread) |

### Replacing the Window Message Pump

The Win32 implementation uses `WSAAsyncSelect` to get notified of socket
events via window messages. SDL3_net uses a polling model instead.

The SDL3 implementation uses a worker thread that polls sockets:

```cpp
class ATNetSockWorkerSDL3 {
    // Worker thread polls all active sockets
    void WorkerThread() {
        while (mRunning) {
            for (auto& sock : mActiveSockets) {
                if (NET_WaitUntilInputAvailable(sock.stream, 0)) {
                    // Data available -- read and dispatch
                    HandleSocketData(sock);
                }
            }

            // Also check for new connections on server sockets
            for (auto& srv : mServerSockets) {
                NET_StreamSocket *client = NET_AcceptClient(srv);
                if (client)
                    HandleNewConnection(client);
            }

            // Brief sleep to avoid busy-waiting
            SDL_Delay(1);
        }
    }
};
```

For better efficiency, check multiple sockets with a single timeout:

```cpp
// Poll with timeout to avoid busy-wait
NET_WaitUntilInputAvailable(socket, 10);  // 10ms timeout
```

### Interface Mapping

The existing interfaces between `ATNetwork` and `ATNetworkSockets` are:

- `IATEmuNetSocketListener` -- callback interface for socket events
- `IATStreamSocket` -- abstract stream socket
- `IATDatagramSocket` -- abstract datagram socket

These are clean interfaces with no Win32 types. The SDL3_net implementation
provides the same interfaces using SDL3_net calls internally.

### DNS Resolution

SDL3_net provides async DNS via `NET_ResolveHostname()` which runs on a
background thread. Check completion with `NET_GetAddressStatus()`. This
replaces the Winsock `getaddrinfo` wrapper in the existing code.

### VXLAN Tunnel

The existing `vxlantunnel.cpp` uses raw UDP sockets for VXLAN
encapsulation. Replace Winsock UDP calls with `NET_CreateDatagramSocket` /
`NET_SendDatagram` / `NET_ReceiveDatagram`.

## Build-Time Selection

The existing `ATNetworkSockets` project compiles Win32 sources. For the SDL3
build, compile the `_sdl3.cpp` variants instead. The CMake build system
handles this selection.

Alternatively, since `ATNetworkSockets` is a small library (7 files), create
a completely separate `ATNetworkSocketsSDL3` library that implements the
same interfaces. This avoids conditional compilation within a single file
and keeps the Win32 code completely untouched.

## Priority

Network emulation (DrACE, modem emulation, etc.) is a niche feature. This
is Phase 8 work -- the emulator is fully functional without it.

## Summary of New Files

| File | Purpose |
|------|---------|
| `src/ATNetworkSockets/source/nativesockets_sdl3.cpp` | SDL3_net initialization and shutdown |
| `src/ATNetworkSockets/source/worker_sdl3.cpp` | Polling-based socket worker (replaces HWND message pump) |
| `src/ATNetworkSockets/source/socketutils_sdl3.cpp` | Socket utility functions |

## Dependencies

- SDL3_net library
- `ATNetwork` interfaces (clean)
- `IATEmuNetSocketListener`, `IATStreamSocket`, `IATDatagramSocket` (clean)

Does **not** depend on Winsock2, `windows.h`, or any Win32 headers.
