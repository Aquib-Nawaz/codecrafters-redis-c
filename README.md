# Codecrafters Redis Challenge – C Implementation

This repository contains a minimal Redis server implementation written in C for the [Codecrafters Redis challenge](https://codecrafters.io/challenges/redis).

## Features

- **TCP server**: Accepts connections from Redis clients.
- **RESP protocol parsing**: Handles the Redis Serialization Protocol for requests and responses.
- **Key-value store**: Supports basic Redis commands (`PING`, `ECHO`, `SET`, `GET`).
- **Key expiry (TTL)**: Supports time-to-live for keys.
- **Streams and Replication**: Includes stream and replication modules for more advanced Redis features.

## Folder Structure

```
app/
├── hashset.c       # Hashset (key-value store) implementation
├── hashset.h
├── message.c       # Handles message/request/response logic
├── message.h
├── parser.c        # RESP protocol parser
├── parser.h
├── replication.c   # Redis replication logic
├── replication.h
├── server.c        # TCP server and main event loop
├── stream.c        # Stream data-type and logic
├── stream.h
```

## Getting Started

### Prerequisites

- GCC or Clang (C99 compatible)
- `make` utility
- Linux/macOS (Windows support may vary)
- [`redis-cli`](https://redis.io/docs/ui/cli/) or [`netcat`](https://nmap.org/ncat/) for manual testing

### Build

```sh
cd app
make
```

This produces an executable (e.g., `redis-server`).

### Run

```sh
./redis-server
```

By default, the server listens on port 6379.

### Connect & Test

Using `redis-cli`:

```sh
redis-cli -p 6379 ping
redis-cli -p 6379 set foo bar
redis-cli -p 6379 get foo
```

Or with `nc`:

```sh
echo -en "*1\r\n$4\r\nPING\r\n" | nc localhost 6379
```

## Extending

- Add new commands in `message.c` and update protocol handling in `parser.c`.
- Stream and replication features can be extended by editing `stream.c` and `replication.c`.

## License

MIT License

---

Inspired by [Redis](https://redis.io/). Built for the [Codecrafters Redis challenge](https://codecrafters.io/challenges/redis).
