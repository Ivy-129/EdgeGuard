
#!/usr/bin/env python3
import socket
import subprocess
import time

HOST = "127.0.0.1"
PORT = 18080
TIMEOUT_SEC = 1


def receive_line(sock):
    data = bytearray()

    while not data.endswith(b"\n"):
        chunk = sock.recv(1)
        if not chunk:
            raise RuntimeError("Connection closed unexpectedly")
        data.extend(chunk)

    return data.decode().rstrip("\n")


def send_command(sock, command):
    sock.sendall((command + "\n").encode())

    header = receive_line(sock)
    parts = header.split()

    if len(parts) == 2 and parts[0] == "OK":
        length = int(parts[1])
    elif len(parts) == 3 and parts[0] == "ERR":
        length = int(parts[2])
    else:
        raise RuntimeError(f"Invalid response header: {header}")

    payload = bytearray()

    while len(payload) < length:
        chunk = sock.recv(length - len(payload))
        if not chunk:
            raise RuntimeError("Connection closed while reading payload")
        payload.extend(chunk)

    return header, payload.decode()


def main():
    server = subprocess.Popen(
        [
            "./bin/server",
            "--port", str(PORT),
            "--log-level", "1",
            "--min-memory-mb", "128",
            "--worker-timeout-sec", str(TIMEOUT_SEC),
        ],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
        text=True,
    )

    try:
        # Wait until the test server is listening.
        for _ in range(50):
            if server.poll() is not None:
                raise RuntimeError("Test server exited unexpectedly")

            try:
                sock = socket.create_connection((HOST, PORT), timeout=2)
                break
            except OSError:
                time.sleep(0.1)
        else:
            raise RuntimeError("Test server did not start")

        with sock:
            sock.settimeout(5)

            header, payload = send_command(
                sock, "SUBMIT BENCH 10000000"
            )

            if not header.startswith("OK "):
                raise AssertionError(f"Submit failed: {payload}")

            job_id = None

            for line in payload.splitlines():
                if line.startswith("JOB_ID="):
                    job_id = int(line.split("=", 1)[1])

            if job_id is None:
                raise AssertionError("Missing JOB_ID")

            print(f"Submitted Job {job_id}")

            deadline = time.monotonic() + 8

            while time.monotonic() < deadline:
                _, status = send_command(
                    sock, f"STATUS {job_id}"
                )

                if "STATUS=TIMEOUT" in status:
                    print("PASS: Worker timeout detected")
                    break

                if "STATUS=COMPLETED" in status:
                    raise AssertionError(
                        "Worker completed before timeout"
                    )

                time.sleep(0.2)
            else:
                raise AssertionError("Worker did not time out")

            _, result = send_command(
                sock, f"RESULT {job_id}"
            )

            assert "Worker exceeded execution time limit" in result
            print("PASS: Timeout result recorded")

            # Verify the server still accepts new work.
            _, payload = send_command(sock, "SUBMIT BENCH 100")

            next_job_id = None

            for line in payload.splitlines():
                if line.startswith("JOB_ID="):
                    next_job_id = int(line.split("=", 1)[1])

            if next_job_id is None:
                raise AssertionError("New job was rejected")

            deadline = time.monotonic() + 5

            while time.monotonic() < deadline:
                _, status = send_command(
                    sock, f"STATUS {next_job_id}"
                )

                if "STATUS=COMPLETED" in status:
                    break

                time.sleep(0.1)
            else:
                raise AssertionError("New job did not complete")

            _, result = send_command(
                sock, f"RESULT {next_job_id}"
            )

            assert "PRIME_COUNT=25" in result
            print("PASS: New Worker completed after timeout")

            send_command(sock, "QUIT")

    finally:
        server.terminate()

        try:
            server.wait(timeout=3)
        except subprocess.TimeoutExpired:
            server.kill()
            server.wait()

        if server.stderr:
            server.stderr.close()


if __name__ == "__main__":
    main()
