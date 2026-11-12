
#!/usr/bin/env python3

import os
import signal
import socket
import subprocess
import time

SERVER = "./bin/server"
PORT = 18084


def get_children(parent_pid):
    result = subprocess.run(
        ["ps", "-o", "pid=", "--ppid", str(parent_pid)],
        capture_output=True,
        text=True,
        check=True,
    )
    return [int(pid) for pid in result.stdout.split()]


def process_exists(pid):
    result = subprocess.run(
        ["ps", "-p", str(pid), "-o", "pid="],
        capture_output=True,
        text=True,
    )
    return bool(result.stdout.strip())


def wait_for_server(port, timeout=5):
    deadline = time.monotonic() + timeout

    while time.monotonic() < deadline:
        try:
            return socket.create_connection(
                ("127.0.0.1", port), timeout=0.5
            )
        except OSError:
            time.sleep(0.05)

    raise RuntimeError("Server did not start")


def main():
    server = subprocess.Popen(
        [
            SERVER,
            "--port", str(PORT),
            "--log-level", "2",
            "--worker-timeout-sec", "60",
        ],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.STDOUT,
        start_new_session=True,
    )

    client = None
    worker_pid = None
    handler_pid = None

    try:
        client = wait_for_server(PORT)

        # Wait for the Child Handler.
        deadline = time.monotonic() + 3

        while time.monotonic() < deadline:
            children = get_children(server.pid)
            if len(children) == 1:
                handler_pid = children[0]
                break
            time.sleep(0.01)

        assert handler_pid is not None, "Handler not created"
        print(f"Handler PID: {handler_pid}", flush=True)

        # Submit a BENCH task.
        client.sendall(b"SUBMIT BENCH 10000000\n")

        # Find the newly created Worker.
        deadline = time.monotonic() + 2

        while time.monotonic() < deadline:
            children = get_children(server.pid)
            workers = [pid for pid in children if pid != handler_pid]

            if workers:
                worker_pid = workers[0]
                os.kill(worker_pid, signal.SIGSTOP)
                break

            time.sleep(0.005)

        assert worker_pid is not None, "Worker not observed"

        print(f"Worker PID: {worker_pid}", flush=True)
        print("Worker SIGSTOP sent", flush=True)

        # Confirm the Worker is stopped.
        result = subprocess.run(
            ["ps", "-o", "stat=", "-p", str(worker_pid)],
            capture_output=True,
            text=True,
        )

        assert result.stdout.strip().startswith("T"), (
            "Worker did not enter stopped state"
        )

        print("PASS: Worker stopped", flush=True)

        # Trigger shutdown while Worker is stopped.
        server.send_signal(signal.SIGINT)

        # SIGTERM cannot terminate a SIGSTOP'ed process.
        # Server should escalate to SIGKILL after grace period.
        server.wait(timeout=8)

        assert server.returncode == 0, (
            f"Server exited with code {server.returncode}"
        )

        assert not process_exists(worker_pid), (
            "Worker remains after shutdown"
        )

        assert not process_exists(handler_pid), (
            "Handler remains after shutdown"
        )

        print("PASS: Server exited after SIGINT")
        print("PASS: Stopped Worker terminated and reaped")
        print("PASS: Child Handler reaped")
        print("PASS: Worker Shutdown")

    finally:
        if client is not None:
            client.close()

        if server.poll() is None:
            # Clean up only the process group created by this test.
            os.killpg(server.pid, signal.SIGKILL)
            server.wait()
        else:
            # Also clean up any descendants left in the test group.
            try:
                os.killpg(server.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass


if __name__ == "__main__":
    main()

