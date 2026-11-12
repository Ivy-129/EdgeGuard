
#!/usr/bin/env python3
import hashlib
import os
import socket
import subprocess
import time
from pathlib import Path

HOST = "127.0.0.1"
PORT = 18081
ROOT = Path(__file__).resolve().parent.parent
HASH_DIR = ROOT / "hash_files"


def recv_exact(sock, length):
    data = bytearray()
    while len(data) < length:
        chunk = sock.recv(length - len(data))
        if not chunk:
            raise RuntimeError("Connection closed during response")
        data.extend(chunk)
    return bytes(data)


def recv_line(sock):
    data = bytearray()
    while not data.endswith(b"\n"):
        chunk = sock.recv(1)
        if not chunk:
            raise RuntimeError("Connection closed unexpectedly")
        data.extend(chunk)
    return data.decode().strip()


def command(sock, request):
    sock.sendall((request + "\n").encode())
    header = recv_line(sock)
    parts = header.split()

    if len(parts) == 2 and parts[0] == "OK":
        success = True
        length = int(parts[1])
        code = None
    elif len(parts) == 3 and parts[0] == "ERR":
        success = False
        code = parts[1]
        length = int(parts[2])
    else:
        raise AssertionError(f"Invalid header: {header}")

    payload = recv_exact(sock, length).decode()
    return success, code, payload


def submit(sock, filename):
    success, code, payload = command(
        sock, f"SUBMIT HASH {filename}"
    )
    assert success, f"SUBMIT failed: {code}: {payload}"

    for line in payload.splitlines():
        if line.startswith("JOB_ID="):
            return int(line.split("=", 1)[1])

    raise AssertionError("Missing JOB_ID")


def wait_status(sock, job_id, expected):
    deadline = time.monotonic() + 5

    while time.monotonic() < deadline:
        success, code, payload = command(
            sock, f"STATUS {job_id}"
        )
        assert success, f"STATUS failed: {code}: {payload}"

        fields = dict(
            line.split("=", 1)
            for line in payload.splitlines()
            if "=" in line
        )

        status = fields.get("STATUS")

        if status == expected:
            return

        if status in ("COMPLETED", "FAILED", "TIMEOUT"):
            raise AssertionError(
                f"Expected {expected}, got {status}"
            )

        time.sleep(0.1)

    raise AssertionError(f"Job {job_id} did not reach {expected}")


def main():
    HASH_DIR.mkdir(exist_ok=True)

    # Use dedicated test files; do not modify existing test.txt.
    content = b"EdgeGuard HASH security test\n"
    (HASH_DIR / "security_test.txt").write_bytes(content)

    outside = ROOT / "security_secret.txt"
    outside.write_text("PRIVATE TEST CONTENT\n")

    link = HASH_DIR / "security_link.txt"
    if link.is_symlink() or link.exists():
        link.unlink()
    link.symlink_to(outside)

    missing = HASH_DIR / "security_missing.txt"
    if missing.is_symlink() or missing.exists():
        missing.unlink()

    server = subprocess.Popen(
        [
            str(ROOT / "bin/server"),
            "--port", str(PORT),
            "--log-level", "0",
            "--min-memory-mb", "128",
            "--worker-timeout-sec", "10",
        ],
        cwd=ROOT,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )

    try:
        for _ in range(50):
            if server.poll() is not None:
                raise RuntimeError("Server exited unexpectedly")
            try:
                sock = socket.create_connection(
                    (HOST, PORT), timeout=2
                )
                break
            except OSError:
                time.sleep(0.1)
        else:
            raise RuntimeError("Server did not start")

        with sock:
            sock.settimeout(5)

            # 1. Path Traversal
            ok, code, _ = command(
                sock, "SUBMIT HASH ../security_secret.txt"
            )
            assert not ok and code == "INVALID_ARGUMENT"
            print("PASS: Path traversal rejected")

            # 2. Absolute Path
            ok, code, _ = command(
                sock, "SUBMIT HASH /etc/passwd"
            )
            assert not ok and code == "INVALID_ARGUMENT"
            print("PASS: Absolute path rejected")

            # 3. Symbolic Link
            job_id = submit(sock, "security_link.txt")
            wait_status(sock, job_id, "FAILED")
            ok, _, result = command(sock, f"RESULT {job_id}")
            assert ok and "Cannot open requested file" in result
            print("PASS: Symbolic link rejected")

            # 4. Missing File
            job_id = submit(sock, "security_missing.txt")
            wait_status(sock, job_id, "FAILED")
            ok, _, result = command(sock, f"RESULT {job_id}")
            assert ok and "Cannot open requested file" in result
            print("PASS: Missing file handled")

            # 5. Valid SHA-256
            job_id = submit(sock, "security_test.txt")
            wait_status(sock, job_id, "COMPLETED")
            ok, _, result = command(sock, f"RESULT {job_id}")

            expected = hashlib.sha256(content).hexdigest()
            assert ok and f"SHA256={expected}" in result
            print("PASS: SHA-256 matches hashlib")

            command(sock, "QUIT")

    finally:
        server.terminate()
        try:
            server.wait(timeout=3)
        except subprocess.TimeoutExpired:
            server.kill()
            server.wait()

        # Clean up only files created by this test.
        link.unlink(missing_ok=True)
        (HASH_DIR / "security_test.txt").unlink(missing_ok=True)
        outside.unlink(missing_ok=True)


if __name__ == "__main__":
    main()
