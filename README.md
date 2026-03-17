```text
  ____   ___  _  _______ ____  
 |  _ \ / _ \| |/ / ____|  _ \ 
 | |_) | | | | ' /|  _| | |_) |
 |  __/| |_| | . \| |___|  _ < 
 |_|    \___/|_|\_\_____|_| \_\

 .-------. .-------. .-------. .-------. .-------.
 |A      | |K      | |Q      | |J      | |10     |
 |   ♠   | |   ♠   | |   ♠   | |   ♠   | |   ♠   |
 |      A| |      K| |      Q| |      J| |     10|
 '-------' '-------' '-------' '-------' '-------'
```
> A C++20 Texas Hold'em poker server

---

## What is this?

A from-scratch poker server written in C++20. The goal is a clean, low-overhead backend that handles full Texas Hold'em game logic and exposes a networked API for clients to join, sit, and play. Still early — the core game loop runs, and the HTTP layer is taking shape.

---

## Project Structure

```
src/
  pokercore/       # Game logic: deck, hand evaluation, game state machine
  pokernetwork/    # HTTP server, routing, JWT auth
  pokergame/       # Entry point / wiring
  tests/           # Unit tests
```

---

## Dependencies

| Library | Version | Purpose |
|---|---|---|
| [uWebSockets](https://github.com/uNetworking/uWebSockets) | v20.75.0 | HTTP/WebSocket server |
| [uSockets](https://github.com/uNetworking/uSockets) | (bundled with uWebSockets) | Low-level async I/O |
| [jwt-cpp](https://github.com/Thalhammer/jwt-cpp) | v0.7.2 | JWT generation and validation |
| [OpenSSL](https://www.openssl.org/) | system | TLS + crypto primitives |
| [ZLIB](https://zlib.net/) | system | Compression (required by uWebSockets) |

Dependencies are fetched automatically via CMake's `FetchContent`. OpenSSL and ZLIB must be installed on the host system.

---

## Building

### Prerequisites

- CMake >= 4.1.2
- C++20 compiler (GCC 12+ or Clang 16+)
- OpenSSL
- ZLIB

### Steps

```bash
# TODO
```

---

## Configuration

```bash
# TODO: document .env and ENV_PATH
```

---

## API

```
# TODO: document HTTP routes
POST /v1/path/createGame
```

---

## Roadmap

- [ ] ...

---

## License

<!-- TODO -->
