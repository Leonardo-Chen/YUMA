#!/usr/bin/env python3
import argparse
import asyncio
import json
from datetime import datetime

import websockets


def now_str() -> str:
    return datetime.now().strftime("%Y-%m-%d %H:%M:%S")


def format_sensor_line(data: dict) -> str:
    timestamp = data.get("timestamp", "-")
    distance_mm = data.get("distance_mm", "-")
    out_of_range = data.get("out_of_range", False)

    acc = data.get("acc", {}) or {}
    gyro = data.get("gyro", {}) or {}

    ax = acc.get("x", 0.0)
    ay = acc.get("y", 0.0)
    az = acc.get("z", 0.0)
    gx = gyro.get("x", 0.0)
    gy = gyro.get("y", 0.0)
    gz = gyro.get("z", 0.0)

    return (
        f"ts={timestamp} dist={distance_mm}mm oor={out_of_range} "
        f"acc=({ax:.3f},{ay:.3f},{az:.3f}) "
        f"gyro=({gx:.3f},{gy:.3f},{gz:.3f})"
    )


async def handle_client(websocket, path=None):
    client = websocket.remote_address
    print(f"[{now_str()}] client connected: {client}")
    try:
        async for message in websocket:
            try:
                data = json.loads(message)
                print(f"[{now_str()}] {client} {format_sensor_line(data)}")
            except json.JSONDecodeError:
                print(f"[{now_str()}] {client} raw: {message}")
    except websockets.ConnectionClosed:
        pass
    finally:
        print(f"[{now_str()}] client disconnected: {client}")


async def main() -> None:
    parser = argparse.ArgumentParser(description="Sensor WebSocket server for M5StickC Plus2")
    parser.add_argument("--host", default="0.0.0.0", help="Host to bind (default: 0.0.0.0)")
    parser.add_argument("--port", type=int, default=8080, help="Port to bind (default: 8080)")
    args = parser.parse_args()

    print(f"[{now_str()}] starting WebSocket server on ws://{args.host}:{args.port}/")
    async with websockets.serve(handle_client, args.host, args.port, ping_interval=20, ping_timeout=20):
        await asyncio.Future()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print(f"\n[{now_str()}] server stopped")
