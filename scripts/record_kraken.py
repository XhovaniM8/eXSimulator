#!/usr/bin/env python3
"""
Record Kraken XBT/USD order book events to a JSONL file.
Each line is one raw WebSocket message with a local timestamp added.

Usage:
    python3 scripts/record_kraken.py --duration 60 --output data/kraken_btcusd.jsonl
"""

import argparse
import json
import signal
import sys
import time
import websocket

OUTPUT_FILE = None
START_TIME = None
DURATION = None
MSG_COUNT = 0


def on_open(ws):
    print(f"[+] Connected to Kraken WebSocket", flush=True)
    sub = {
        "event": "subscribe",
        "pair": ["XBT/USD"],
        "subscription": {"name": "book", "depth": 10},
    }
    ws.send(json.dumps(sub))
    print(f"[+] Subscribed to XBT/USD order book (depth 10)", flush=True)


def on_message(ws, message):
    global MSG_COUNT, OUTPUT_FILE, START_TIME, DURATION

    # Add local receive timestamp
    envelope = {"ts": time.time_ns(), "data": json.loads(message)}
    OUTPUT_FILE.write(json.dumps(envelope) + "\n")
    MSG_COUNT += 1

    # Progress every 100 messages
    if MSG_COUNT % 100 == 0:
        elapsed = time.time() - START_TIME
        remaining = DURATION - elapsed
        print(
            f"[+] {MSG_COUNT} messages recorded ({elapsed:.1f}s elapsed, {remaining:.1f}s remaining)",
            flush=True,
        )

    # Stop after duration
    if time.time() - START_TIME >= DURATION:
        print(f"\n[+] Duration reached. Closing.", flush=True)
        ws.close()


def on_error(ws, error):
    print(f"[!] WebSocket error: {error}", file=sys.stderr)


def on_close(ws, close_status_code, close_msg):
    elapsed = time.time() - START_TIME
    print(f"\n[+] Connection closed after {elapsed:.1f}s")
    print(f"[+] Total messages recorded: {MSG_COUNT}")
    print(f"[+] Output written to: {args.output}")


def signal_handler(sig, frame):
    print(f"\n[+] Interrupted. {MSG_COUNT} messages recorded.", flush=True)
    sys.exit(0)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Record Kraken order book to JSONL")
    parser.add_argument(
        "--duration",
        type=int,
        default=60,
        help="Recording duration in seconds (default: 60)",
    )
    parser.add_argument(
        "--output",
        type=str,
        default="data/kraken_btcusd.jsonl",
        help="Output file path (default: data/kraken_btcusd.jsonl)",
    )
    parser.add_argument(
        "--pair", type=str, default="XBT/USD", help="Trading pair (default: XBT/USD)"
    )
    parser.add_argument(
        "--depth", type=int, default=10, help="Order book depth (default: 10)"
    )
    args = parser.parse_args()

    DURATION = args.duration
    START_TIME = time.time()

    signal.signal(signal.SIGINT, signal_handler)

    print(f"[+] Recording {args.pair} order book for {args.duration}s -> {args.output}")

    with open(args.output, "w") as f:
        OUTPUT_FILE = f

        ws = websocket.WebSocketApp(
            "wss://ws.kraken.com",
            on_open=on_open,
            on_message=on_message,
            on_error=on_error,
            on_close=on_close,
        )
        ws.run_forever()
