# Network evidence

Wireshark captures on the PC's adapter, direct cable to the board (PC 192.168.50.1, board 192.168.50.2, 10BASE-T half duplex). The raw `.pcapng` files stay local (`docs/network-packets/`, gitignored): they hold MAC addresses and the browser's request headers. The files here are `tshark` summary lines only.

| File | Firmware | What |
|---|---|---|
| [http-get.txt](http-get.txt) | `4bc2cf7` | Chrome loads `http://192.168.50.2/` |
| [concurrency-before.txt](concurrency-before.txt) | `4bc2cf7` + `NET_MAX_CONN=8` | 1, 2 and 3 simultaneous HTTP clients, before the poll fix |
| [concurrency-after.txt](concurrency-after.txt) | both fixes below | `tools/http_concurrency_test.py`: echo held open + 3 HTTP clients, then 2 and 3 at once |

## Page load (http-get.txt)

| Frames | Time | What happens |
|---|---|---|
| 1-3 | 0-1.2 ms | TCP handshake on client port 61163. The board answers SYN in 1.0 ms, MSS 1460, receive window 1,536 B. |
| 4 | 1.5 ms | `GET / HTTP/1.1` (463 B, `Accept-Encoding: gzip, deflate`). |
| 6 | 3.4 ms | The board ACKs the request. |
| 8 | 251 ms | `HTTP/1.1 200 OK`, `Content-Type: text/html`, `Content-Length: 773`, `Content-Encoding: gzip`, headers and gzipped page in one 862 B segment; Wireshark decodes it to the page. |
| 5, 7, 10-17 | 3-2045 ms | The browser's second, speculative connection (port 55832) gets RST five times (finding 1). |
| 20-21 | 9.94 s | The board closes the idle connection: `HTTP_SERVER_CLIENT_INACTIVITY_TIMEOUT` = 10 s. |
| 22-23 | 54.9 s | The browser's keep-alive probe on the closed connection gets RST, as expected. |

## Findings

The server was meant to take three clients (`HTTP_SERVER_MAX_CLIENTS=3`), but two Zephyr defaults limited the board to **one TCP client at a time**. Single-client tests (`curl`, `echo_test.py`, a browser) all passed, so only a capture showed it.

**1. Connection table full: second SYN gets RST.** Zephyr keeps one entry in `conns[CONFIG_NET_MAX_CONN]` (`subsys/net/ip/connection.c`) per listener, accepted TCP connection and bound UDP port. The default 4 was filled by the echo listener (4242), the HTTP listener (80), the DHCPv4 client (UDP 68) and the first client. The next SYN failed in `tcp_conn_new()` → `net_conn_register()`, and the stack answered with RST (http-get.txt frames 7-17). Logged only at TCP log level ERR, which is off. **Fix:** `CONFIG_NET_MAX_CONN=8` (peak use 7; 40 B per entry).

**2. Poll array too small: the HTTP server restarts.** The server polls its eventfd, its listener and every client in one `zsock_poll()`, and each fd takes one of `CONFIG_ZVFS_POLL_MAX` slots (`lib/os/zvfs/zvfs_poll.c`), default 3. A second client made the socket layer return `-ENOMEM` (`sockets_inet.c`); `http_server_run()` treats a failed poll as fatal, closes every socket and restarts after `HTTP_SERVER_RESTART_DELAY` (1 s). concurrency-before.txt shows it: two clients are both closed (FIN) within 1 ms of connecting, before their requests arrive. A third client's SYN gets RST while the listener is gone, and is accepted ~1 s later when the server is back. **Fix:** `CONFIG_ZVFS_POLL_MAX=5` (1 + 1 + 3 clients; the array is on the poller's stack, no static RAM).

After both fixes, concurrency-after.txt and `tools/http_concurrency_test.py`: the echo connection stays open while three HTTP clients get `200 OK` at once, and 2 and 3 parallel requests all succeed. A browser capture after the fixes (kept local) shows the same: Chrome's two connections (page + speculative) are both accepted, with no RST, the page arrives 54 ms after the request, and the board closes the unused connection after the 10 s inactivity timeout.

**3. First response delayed by up to ~250 ms (open).** The board ACKs a request within 2 ms, but the response can take 50-250 ms. In concurrency-after.txt (client 54801) the board sends the 89 B headers at once (frame 28), then holds the 773 B body until the PC's ACK arrives 56 ms later (frames 30-31): a small segment in flight blocks the next one (Nagle's algorithm) while Windows delays its ACK. The other clients got headers and body in one frame after ~270 ms. Likely cause: the server sends headers and body as two writes and doesn't set `TCP_NODELAY`; not confirmed yet. It costs latency only, not correctness. The post-fix browser capture shows the same split: headers 2 ms after the request, body 52 ms later, right after the PC's ACK.

**4. A SYN arriving while the board resolves the PC's MAC address is answered late (open).** In the post-fix browser capture, two SYNs arrive 0.1 ms apart while the board's ARP entry for the PC has expired. The board sends one ARP request, answers the second SYN at 3.6 ms, but answers the first only at 240 ms, about one SYN-ACK retransmission timeout later. Likely: only one packet is kept per pending ARP entry and the other SYN-ACK is dropped until TCP retransmits it. Not confirmed.
