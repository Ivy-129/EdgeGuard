#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
PORT=18080
./bin/server --port "$PORT" --log-level 0 >/tmp/edgeguard-phase1-server.log 2>&1 &
server=$!
trap 'kill "$server" 2>/dev/null || true; wait "$server" 2>/dev/null || true' EXIT
sleep 0.3
output=$(printf 'SYSINFO\nQUIT\n' | ./bin/client 127.0.0.1 "$PORT" --log-level 0)
grep -q 'OS=Linux' <<<"$output"
grep -q 'Goodbye' <<<"$output"
python3 - "$PORT" <<'PY'
import socket,sys
port=int(sys.argv[1]); sockets=[]
try:
    for _ in range(10):
        s=socket.create_connection(('127.0.0.1',port),timeout=3)
        sockets.append(s)
    for s in sockets:
        s.sendall(b'SYSINFO\n')
        f=s.makefile('rb'); header=f.readline()
        assert header.startswith(b'OK '),header
        length=int(header.split()[1]); payload=f.read(length)
        assert b'OS=Linux' in payload
    print('PASS: 10 simultaneous connected clients; SYSINFO responses received')
finally:
    for s in sockets:s.close()
PY
printf 'PASS: basic protocol and QUIT\n'
