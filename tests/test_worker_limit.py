
import socket
import threading
import time

HOST = "127.0.0.1"
PORT = 8080
CLIENT_COUNT = 4
TASK = "SUBMIT BENCH 10000000"

barrier = threading.Barrier(CLIENT_COUNT)


def recv_exact(sock, length):
    data = b""

    while len(data) < length:
        chunk = sock.recv(length - len(data))
        if not chunk:
            raise ConnectionError("Server disconnected")
        data += chunk

    return data


def submit_task(client_id):
    try:
        with socket.create_connection((HOST, PORT), timeout=10) as sock:
            # Wait until all 4 clients are connected.
            barrier.wait(timeout=10)

            sock.sendall((TASK + "\n").encode())

            header = b""
            while not header.endswith(b"\n"):
                chunk = sock.recv(1)
                if not chunk:
                    raise ConnectionError("Connection closed")
                header += chunk

            parts = header.decode().strip().split()
            length = int(parts[-1])
            payload = recv_exact(sock, length).decode()

            print(
                f"\n[Client {client_id}] "
                f"{header.decode().strip()}\n"
                f"{payload}",
                flush=True,
            )

    except Exception as exc:
        print(f"[Client {client_id}] ERROR: {exc}", flush=True)


threads = []

for i in range(CLIENT_COUNT):
    thread = threading.Thread(target=submit_task, args=(i + 1,))
    threads.append(thread)
    thread.start()

for thread in threads:
    thread.join()

print("\nAll submissions finished.")
