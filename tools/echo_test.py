"""Send five lines to the board's TCP echo server and check each comes back intact.

Usage: python tools/echo_test.py [host]   (default 192.168.50.2, port 4242)
"""
import socket
import sys

PORT = 4242

host = sys.argv[1] if len(sys.argv) > 1 else "192.168.50.2"

with socket.create_connection((host, PORT), timeout=5) as s:
    for i in range(5):
        msg = f"ping {i}\n".encode()
        s.sendall(msg)
        got = b""
        # TCP is a byte stream: the echo may arrive in several pieces
        while len(got) < len(msg):
            part = s.recv(len(msg) - len(got))
            if not part:
                sys.exit("server closed early")
            got += part
        if got != msg:
            sys.exit(f"mismatch: sent {msg!r}, got {got!r}")
        print("ok", got.decode().strip())
