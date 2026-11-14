"""Decode ENC28J60 SPI traffic from a Saleae Logic 2 digital CSV export.

SPI mode 0 (sample on rising SCK, MSB first), one transaction per CS-low window.
Prints every transaction (MOSI / MISO bytes) and the SCK frequency, averaged over
whole bytes (7 periods per byte), which is finer than the analyzer's sample step.

Usage: python tools/spi_decode.py <digital.csv> [--sck N] [--mosi N] [--miso N] [--cs N]
Default channels match the bench wiring: SCK 4, MOSI 2, MISO 0, CS 6.
"""
import argparse
import csv
import statistics

parser = argparse.ArgumentParser()
parser.add_argument("csv")
parser.add_argument("--sck", type=int, default=4)
parser.add_argument("--mosi", type=int, default=2)
parser.add_argument("--miso", type=int, default=0)
parser.add_argument("--cs", type=int, default=6)
args = parser.parse_args()

with open(args.csv, newline="", encoding="utf-8") as f:
    reader = csv.reader(f)
    next(reader)
    rows = [(float(r[0]), [int(v) for v in r[1:]]) for r in reader]

txns = []
byte_periods = []
cur = None
prev = rows[0][1]
for t, s in rows[1:]:
    if prev[args.cs] == 1 and s[args.cs] == 0:
        cur = {"start": t, "mosi": [], "miso": [], "o": 0, "i": 0, "n": 0, "edges": []}
    if cur is not None and s[args.cs] == 0 and prev[args.sck] == 0 and s[args.sck] == 1:
        cur["o"] = (cur["o"] << 1) | s[args.mosi]
        cur["i"] = (cur["i"] << 1) | s[args.miso]
        cur["n"] += 1
        cur["edges"].append(t)
        if cur["n"] == 8:
            cur["mosi"].append(cur["o"])
            cur["miso"].append(cur["i"])
            byte_periods.append((cur["edges"][-1] - cur["edges"][0]) / 7)
            cur["o"] = cur["i"] = cur["n"] = 0
            cur["edges"] = []
    if cur is not None and prev[args.cs] == 0 and s[args.cs] == 1:
        cur["end"] = t
        txns.append(cur)
        cur = None
    prev = s

for k, x in enumerate(txns):
    mosi = " ".join(f"{b:02X}" for b in x["mosi"])
    miso = " ".join(f"{b:02X}" for b in x["miso"])
    stray = f"  (+{x['n']} stray bits)" if x["n"] else ""
    print(f"{k:3d}  t={x['start'] * 1e6:10.1f} us  CS low {(x['end'] - x['start']) * 1e6:5.2f} us"
          f"  MOSI {mosi:<12} MISO {miso}{stray}")

if byte_periods:
    period = statistics.mean(byte_periods)
    print(f"\n{len(txns)} transactions, {len(byte_periods)} bytes; "
          f"SCK {period * 1e9:.1f} ns = {1 / period / 1e6:.3f} MHz "
          f"(per-byte spread {statistics.pstdev(byte_periods) * 1e9:.1f} ns)")
