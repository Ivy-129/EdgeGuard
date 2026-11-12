
#!/usr/bin/env python3

import os
import signal
import socket
import subprocess
import time

SERVER = "./bin/server"


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


def get_children(pid):
    result = subprocess.run(
        ["ps", "-o", "pid=", "--ppid", str(pid)],
        capture_output=True,
        text=True,
        check=True,
    )

    return [int(x) for x in result.stdout.split()]


def main():
    port = 18083

    server = subprocess.Popen(
        [
            SERVER,
            "--port", str(port),
            "--log-level", "2",
            "--worker-timeout-sec", "60",
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )

    client = None

    try:
        client = wait_for_server(port)

        # Wait until the Parent creates a Handler.
        deadline = time.monotonic() + 3
        children = []

        while time.monotonic() < deadline:
            children = get_children(server.pid)

            if children:
                break

            time.sleep(0.05)

        assert children, "No Child Handler created"

        handler_pid = children[0]
        print(f"Handler PID: {handler_pid}", flush=True)

        # Trigger graceful shutdown.
        server.send_signal(signal.SIGINT)

        server.wait(timeout=5)

        assert server.returncode == 0, (
            f"Server exited with code {server.returncode}"
        )

        # Check whether the Handler still exists.
        result = subprocess.run(
            ["ps", "-p", str(handler_pid), "-o", "pid="],
            capture_output=True,
            text=True,
        )

        assert not result.stdout.strip(), (
            "Child Handler remains after shutdown"
        )

        print("PASS: Server exited after SIGINT")
        print("PASS: Child Handler reaped")
        print("PASS: Graceful Shutdown")

    finally:
        if client is not None:
            client.close()

        if server.poll() is None:
            server.kill()
            server.wait()

        if server.stdout is not None:
            server.stdout.close()


if __name__ == "__main__":
    main()
