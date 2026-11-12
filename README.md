# EdgeGuard

EdgeGuard is a Linux-based TCP Client–Server system that supports concurrent client connections, process-based task execution, resource monitoring, and secure file hashing.

## 1. Features

- TCP Client–Server communication
- Support for at least 10 concurrent clients
- Process-based concurrency using `fork()`
- Parent–Handler IPC using Unix domain sockets
- Shared job management using `mmap()` and process-shared semaphores
- Up to 3 concurrent Worker processes
- Memory admission control
- Worker execution timeout
- Graceful server shutdown
- SHA-256 file hashing with filename validation
- Configurable logging levels

## 2. Project Structure

```text
EdgeGuard/
├── include/       Header files
├── src/           Source code
├── lib/           Shared library
├── bin/           Executable files
├── tests/         Automated tests
├── hash_files/    Files available for hashing
├── captures/      Wireshark packet captures
├── Makefile       Build configuration
└── README.md      Project documentation
```

## 3. Build

Requirements:

- Linux (tested on Ubuntu 24.04 / WSL2)
- GCC
- Make
- OpenSSL development library
- Python 3 (for automated tests)

Install dependencies:

```bash
sudo apt update
sudo apt install build-essential libssl-dev python3
```

Compile:

```bash
make
```

Compile with debug information:

```bash
make DEBUG=1
```

## 4. Running the Server

```bash
./bin/server --port 8080 --log-level 2 --min-memory-mb 128 --worker-timeout-sec 10
```

Configuration options:

| Option | Description |
|---|---|
| `--port` | TCP listening port |
| `--log-level` | Logging verbosity (0–2) |
| `--min-memory-mb` | Minimum available memory required for task admission |
| `--worker-timeout-sec` | Maximum Worker execution time in seconds |

## 5. Running the Client

Open another terminal:

```bash
cd ~/EdgeGuard
./bin/client 127.0.0.1 8080 --log-level 2
```

Supported commands:

| Command | Description |
|---|---|
| `SYSINFO` | Display system information |
| `SUBMIT BENCH 1000000` | Submit a prime-counting task |
| `SUBMIT HASH sample.txt` | Submit a SHA-256 hashing task |
| `STATUS 1` | Query Job 1 status |
| `RESULT 1` | Retrieve Job 1 result |
| `QUIT` | Disconnect from the Server |

For HASH tasks, place the input file in the `hash_files/` directory.

## 6. System Architecture

The Parent Server accepts incoming TCP connections and creates a Child Handler process for each accepted client.

Child Handlers communicate with the Parent through Unix domain socket IPC.

The Parent Server is responsible for resource admission, Job management, and Worker process creation.

Worker processes execute BENCH or HASH tasks independently of client connections.

A shared JobTable stores Job states and results, protected by a process-shared semaphore.

## 7. Protocol

Client requests are text-based commands.

Server responses follow one of these formats:

```text
OK <LENGTH>
<PAYLOAD>
```

```text
ERR <CODE> <LENGTH>
<PAYLOAD>
```

## 8. Testing

Run the complete automated test suite:

```bash
make test
```

The test suite covers:

- Concurrent client connections
- Basic protocol operations
- Worker timeout and recovery
- HASH path and symbolic-link security
- SHA-256 correctness
- Graceful shutdown
- Worker termination and cleanup
- Semaphore timeout behavior

## 9. Implementation Notes

- The Server uses processes rather than threads for concurrent task execution.
- JobTable synchronization uses process-shared POSIX semaphores.
- Semaphore acquisition has a bounded waiting time.
- Worker termination and cleanup are handled by the Parent Server.
- Semaphore timeout protection does not provide automatic recovery if a process terminates while holding the lock.

## 10. Current Status

Core functionality implemented and automated tests passing as of November 11, 2026.
