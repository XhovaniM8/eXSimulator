#!/usr/bin/env python3
"""
Parse recorded Kraken order book JSONL and convert to engine replay format.

Outputs a binary file of order commands that can be replayed by the C++ engine.
Also prints throughput stats for the recorded feed.

Usage:
    python3 scripts/parse_feed.py --input data/kraken_btcusd.jsonl --output data/kraken_replay.bin --stats
"""

import argparse
import json
import struct
import sys
from dataclasses import dataclass
from typing import Optional

# Binary command format (matches C++ struct layout)
# Each record is 32 bytes:
#   uint8_t  type        (0=ADD, 1=CANCEL, 2=REPLACE)
#   uint8_t  side        (0=BUY, 1=SELL)
#   uint16_t _pad
#   uint64_t order_id
#   int64_t  price       (fixed point * 10000)
#   uint32_t quantity    (size * 10000, capped)
#   uint32_t _pad2
CMD_FMT = "<BBHQqII"
CMD_SIZE = struct.calcsize(CMD_FMT)  # should be 32

CMD_ADD = 0
CMD_CANCEL = 1
CMD_REPLACE = 2

SIDE_BUY = 0
SIDE_SELL = 1

PRICE_SCALE = 10000
QTY_SCALE = 10000


def price_to_fixed(price_str: str) -> int:
    return int(float(price_str) * PRICE_SCALE)


def size_to_qty(size_str: str) -> int:
    val = int(float(size_str) * QTY_SCALE)
    return min(val, 0xFFFFFFFF)


def make_order_id(side: int, price_fixed: int) -> int:
    # Deterministic order ID from side + price
    # Side in high bit, price in lower bits
    return (side << 60) | (price_fixed & 0x0FFFFFFFFFFFFFFF)


def parse_feed(input_path: str, output_path: Optional[str], print_stats: bool):
    commands = []

    # Track current book state: price -> order_id
    bid_book = {}  # price_str -> order_id
    ask_book = {}  # price_str -> order_id

    stats = {
        "total_messages": 0,
        "skipped": 0,
        "snapshots": 0,
        "updates": 0,
        "adds": 0,
        "cancels": 0,
        "replaces": 0,
        "first_ts": None,
        "last_ts": None,
    }

    with open(input_path) as f:
        for line in f:
            line = line.strip()
            if not line:
                continue

            stats["total_messages"] += 1
            envelope = json.loads(line)
            ts_ns = envelope["ts"]
            data = envelope["data"]

            if stats["first_ts"] is None:
                stats["first_ts"] = ts_ns
            stats["last_ts"] = ts_ns

            # Skip non-array messages (systemStatus, subscriptionStatus)
            if not isinstance(data, list):
                stats["skipped"] += 1
                continue

            payload = data[1]

            # --- Initial snapshot ---
            if "as" in payload or "bs" in payload:
                stats["snapshots"] += 1

                for price_str, size_str, _ in payload.get("as", []):
                    price = price_to_fixed(price_str)
                    qty = size_to_qty(size_str)
                    oid = make_order_id(SIDE_SELL, price)
                    ask_book[price_str] = oid
                    commands.append(
                        struct.pack(CMD_FMT, CMD_ADD, SIDE_SELL, 0, oid, price, qty, 0)
                    )
                    stats["adds"] += 1

                for price_str, size_str, _ in payload.get("bs", []):
                    price = price_to_fixed(price_str)
                    qty = size_to_qty(size_str)
                    oid = make_order_id(SIDE_BUY, price)
                    bid_book[price_str] = oid
                    commands.append(
                        struct.pack(CMD_FMT, CMD_ADD, SIDE_BUY, 0, oid, price, qty, 0)
                    )
                    stats["adds"] += 1

            # --- Incremental update ---
            elif "a" in payload or "b" in payload:
                stats["updates"] += 1

                for entry in payload.get("a", []):
                    price_str, size_str = entry[0], entry[1]
                    price = price_to_fixed(price_str)
                    qty = size_to_qty(size_str)

                    if qty == 0:
                        # Delete — cancel existing order
                        if price_str in ask_book:
                            oid = ask_book.pop(price_str)
                            commands.append(
                                struct.pack(
                                    CMD_FMT, CMD_CANCEL, SIDE_SELL, 0, oid, price, 0, 0
                                )
                            )
                            stats["cancels"] += 1
                    elif price_str in ask_book:
                        # Update — replace existing
                        oid = ask_book[price_str]
                        commands.append(
                            struct.pack(
                                CMD_FMT, CMD_REPLACE, SIDE_SELL, 0, oid, price, qty, 0
                            )
                        )
                        stats["replaces"] += 1
                    else:
                        # New price level
                        oid = make_order_id(SIDE_SELL, price)
                        ask_book[price_str] = oid
                        commands.append(
                            struct.pack(
                                CMD_FMT, CMD_ADD, SIDE_SELL, 0, oid, price, qty, 0
                            )
                        )
                        stats["adds"] += 1

                for entry in payload.get("b", []):
                    price_str, size_str = entry[0], entry[1]
                    price = price_to_fixed(price_str)
                    qty = size_to_qty(size_str)

                    if qty == 0:
                        if price_str in bid_book:
                            oid = bid_book.pop(price_str)
                            commands.append(
                                struct.pack(
                                    CMD_FMT, CMD_CANCEL, SIDE_BUY, 0, oid, price, 0, 0
                                )
                            )
                            stats["cancels"] += 1
                    elif price_str in bid_book:
                        oid = bid_book[price_str]
                        commands.append(
                            struct.pack(
                                CMD_FMT, CMD_REPLACE, SIDE_BUY, 0, oid, price, qty, 0
                            )
                        )
                        stats["replaces"] += 1
                    else:
                        oid = make_order_id(SIDE_BUY, price)
                        bid_book[price_str] = oid
                        commands.append(
                            struct.pack(
                                CMD_FMT, CMD_ADD, SIDE_BUY, 0, oid, price, qty, 0
                            )
                        )
                        stats["adds"] += 1
            else:
                stats["skipped"] += 1

    # Write binary output
    if output_path:
        with open(output_path, "wb") as f:
            # Header: magic + command count
            f.write(b"KRKN")
            f.write(struct.pack("<I", len(commands)))
            for cmd in commands:
                f.write(cmd)
        print(
            f"[+] Wrote {len(commands)} commands to {output_path} ({len(commands) * CMD_SIZE / 1024:.1f} KB)"
        )

    if print_stats:
        duration_s = (
            (stats["last_ts"] - stats["first_ts"]) / 1e9 if stats["first_ts"] else 0
        )
        total_cmds = stats["adds"] + stats["cancels"] + stats["replaces"]
        print(f"\n--- Feed Statistics ---")
        print(f"Duration:        {duration_s:.1f}s")
        print(f"Total messages:  {stats['total_messages']}")
        print(f"Skipped:         {stats['skipped']}")
        print(f"Snapshots:       {stats['snapshots']}")
        print(f"Updates:         {stats['updates']}")
        print(f"Commands total:  {total_cmds}")
        print(f"  Adds:          {stats['adds']}")
        print(f"  Cancels:       {stats['cancels']}")
        print(f"  Replaces:      {stats['replaces']}")
        if duration_s > 0:
            print(f"Commands/sec:    {total_cmds / duration_s:.0f} (real market rate)")
        print(f"CMD struct size: {CMD_SIZE} bytes")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Parse Kraken JSONL feed to binary replay format"
    )
    parser.add_argument("--input", type=str, default="data/kraken_btcusd.jsonl")
    parser.add_argument("--output", type=str, default="data/kraken_replay.bin")
    parser.add_argument("--stats", action="store_true", default=True)
    args = parser.parse_args()

    parse_feed(args.input, args.output, args.stats)
