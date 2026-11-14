"""Check that the board serves several TCP clients at the same time.

Holds one connection to the echo server (port 4242) open, then opens three
HTTP connections (port 80, HTTP_SERVER_MAX_CLIENTS) before sending any request,
and expects "200 OK" on every one. Exit code 0 = all passed, 1 = a failure.

Usage: python tools/http_concurrency_test.py [host]   (default 192.168.50.2)
"""
import socket
import sys

HOST = sys.argv[1] if len(sys.argv) > 1 else "192.168.50.2"
HTTP_CLIENTS = 3

failed = False
echo = socket.create_connection((HOST, 4242), timeout=5)
http = [socket.create_connection((HOST, 80), timeout=5) for _ in range(HTTP_CLIENTS)]

echo.sendall(b"held open\n")
if echo.recv(64) != b"held open\n":
    print("echo: FAILED")
    failed = True
else:
    print("echo: ok (held open)")

request = f"GET / HTTP/1.1\r\nHost: {HOST}\r\n\r\n".encode()
for s in http:
    s.sendall(request)

for i, s in enumerate(http):
    try:
        status = s.recv(2048).split(b"\r\n", 1)[0].decode(errors="replace")
    except OSError as e:
        status = repr(e)
    ok = status.startswith("HTTP/1.1 200")
    failed |= not ok
    print(f"http {i}: {'ok' if ok else 'FAILED'} ({status or 'closed without a response'})")

for s in http + [echo]:
    s.close()

sys.exit(1 if failed else 0)
