#!/usr/bin/env python3

"""
Flask + ZMQ based simulator for the ADXSPD driver.
...
"""

from __future__ import annotations

import argparse
import os
import struct
import threading
import time
from typing import Dict, Any, List

from flask import Flask, jsonify, request
import zmq

parser = argparse.ArgumentParser(description="XSPD Simulator Web Server")
parser.add_argument(
    "--device-id", type=str, default="simdet", help="Device ID for the simulator"
)
parser.add_argument(
    "--detector-id", type=str, default="xspdsim", help="Detector ID for the simulator"
)
parser.add_argument("--data-port-id", type=str, default="port-1", help="Data port ID")
parser.add_argument(
    "--frame-width", type=int, default=516, help="Width of generated frames"
)
parser.add_argument(
    "--frame-height", type=int, default=1716, help="Height of generated frames"
)
parser.add_argument(
    "--data-port", type=int, default=4300, help="ZMQ data port to bind for frames"
)
parser.add_argument(
    "--host", type=str, default="0.0.0.0", help="Host address for the web server"
)
parser.add_argument(
    "--port", type=int, default=8000, help="Port number for the web server"
)
args = parser.parse_args()

app = Flask(__name__)

# ----------------------------------------------------------------------------
# In-memory state
# ----------------------------------------------------------------------------

state_lock = threading.Lock()
acquiring = False
frame_number = 0

detector_variables: Dict[str, Any] = {
    "beam_energy": 12.0,
    "bit_depth": 12,
    "charge_summing": "OFF",
    "compression_level": 2,
    "compressor": "NONE",
    "counter_mode": "SINGLE",
    "countrate_correction": "OFF",
    "flatfield_correction": "OFF",
    "gating_mode": "OFF",
    "n_frames": 1,
    "n_modules": 1,
    "roi_rows": 1024,
    "saturation_flag": "OFF",
    "shuffle_mode": "NO_SHUFFLE",
    "shutter_time": 1000.0,  # milliseconds
    "status": "ready",
    "summed_frames": 0,
    "thresholds": [],
    "trigger_mode": "SOFTWARE",
    "type": "LambdaSim",
    "user_data": "",
}

data_port_variables: Dict[str, Any] = {
    "frame_height": 1024,
    "frame_depth": 16,  # bits per pixel for transport (driver maps bit_depth->dtype)
    "frame_width": 1024,
    "frames_queued": 0,
    "pixel_mask": "",
}

modules_variables: List[Dict[str, Any]] = [
    {
        "module": "xspdsim/1",
        "firmware": "1.0.0",
        "compression_level": 1,
        "status": "ready",
        "max_frames": 1000,
        "sensor_current": 0.5,
    }
]

# ----------------------------------------------------------------------------
# Helper functions
# ----------------------------------------------------------------------------


def build_info_payload() -> Dict[str, Any]:
    return {
        "libxsp version": "sim-libxsp-1.0",
        "detectors": [
            {
                "detector-id": args.detector_id,
                "modules": modules_variables,
            }
        ],
    }


def get_var(path: str) -> Dict[str, Any]:
    if path == "info":
        return {"value": build_info_payload()}
    elif path.startswith(args.detector_id + "/xspdsim/1/"):
        key = path.split("/", 1)[1]
        return {"value": modules_variables.get(key, 0.0)}
    elif path.startswith(args.detector_id + "/"):
        key = path.split("/", 1)[1]
        if key == "thresholds":
            return {"value": detector_variables.get(key, [])}
        return {"value": detector_variables.get(key)}
    elif path.startswith(args.data_port_id + "/"):
        key = path.split("/", 1)[1]
        return {"value": data_port_variables.get(key)}
    # Fallback
    return {"value": None}


def set_var(path: str, value: str) -> Dict[str, Any]:
    global acquiring
    if path.startswith(args.detector_id + "/"):
        key = path.split("/", 1)[1]
        if key == "thresholds":
            # Value provided as comma-separated string, store as list of floats
            parts = [p for p in value.split(",") if p]
            thresholds = [float(p) for p in parts]
            detector_variables["thresholds"] = thresholds
            return {
                "thresholds": str(thresholds)
            }  # driver expects string of JSON array
        # Coerce numeric types where appropriate
        if key in {"beam_energy", "shutter_time"}:
            detector_variables[key] = float(value)
        elif key in {
            "bit_depth",
            "compression_level",
            "n_frames",
            "summed_frames",
            "roi_rows",
        }:
            detector_variables[key] = int(value)
        else:
            detector_variables[key] = value
        return {"value": detector_variables[key]}
    if path.startswith(args.data_port_id + "/"):
        key = path.split("/", 1)[1]
        if key in {"frame_height", "frame_width", "frames_queued", "frame_depth"}:
            data_port_variables[key] = int(value)
        else:
            data_port_variables[key] = value
        return {"value": data_port_variables[key]}
    return {"value": None}


def generate_frame_payload(bit_depth: int, width: int, height: int) -> bytes:
    # Approximate bytes per pixel (choose next power-of-two size similar to driver mapping)
    if bit_depth <= 8:
        bpp = 1
    elif bit_depth <= 16:
        bpp = 2
    elif bit_depth <= 24:
        bpp = 4
    else:
        bpp = 2
    size = width * height * bpp
    # Use os.urandom for variability; could be deterministic if needed
    return os.urandom(size)


def publisher_thread(context: zmq.Context):
    global acquiring, frame_number
    socket = context.socket(zmq.PUB)
    socket.bind(f"tcp://*:{args.data_port}")
    try:
        while True:
            with state_lock:
                local_acquiring = acquiring
                n_frames = int(detector_variables.get("n_frames", 1))
                bit_depth = int(detector_variables.get("bit_depth", 12))
                width = int(data_port_variables.get("frame_width", 1024))
                height = int(data_port_variables.get("frame_height", 1024))
            if not local_acquiring:
                time.sleep(0.05)
                continue

            if frame_number >= n_frames:
                # Auto-stop when done (simulates readiness)
                with state_lock:
                    acquiring = False
                    detector_variables["status"] = "ready"
                continue

            with state_lock:
                frame_number += 1
                fn = frame_number
            raw = generate_frame_payload(bit_depth, width, height)
            header = struct.pack("<4H", fn, fn, 0, width * height)
            # Metadata part (can be empty string)
            socket.send_multipart([b"META", header, raw])
            with state_lock:
                data_port_variables["frames_queued"] = frame_number
            time.sleep(0.01)  # throttle slightly
    except KeyboardInterrupt:
        pass
    finally:
        socket.close(0)


# Start publisher background thread
zmq_context = zmq.Context.instance()
threading.Thread(target=publisher_thread, args=(zmq_context,), daemon=True).start()

# ----------------------------------------------------------------------------
# Routes
# ----------------------------------------------------------------------------


@app.get("/api")
def api_root():
    return jsonify({"api version": "1", "xspd version": "1.5.0"})


@app.get("/api/v1/devices")
def list_devices():
    return jsonify({"devices": [{"id": args.device_id}]})


@app.get(f"/api/v1/devices/{args.device_id}")
def device_info():
    return jsonify(
        {
            "system": {
                "data-ports": [
                    {"id": args.data_port_id, "ip": "127.0.0.1", "port": args.data_port}
                ]
            }
        }
    )


@app.get(f"/api/v1/devices/{args.device_id}/commands")
def list_commands():
    return jsonify(
        [
            {"path": f"{args.detector_id}/start"},
            {"path": f"{args.detector_id}/stop"},
        ]
    )


@app.put(f"/api/v1/devices/{args.device_id}/command")
def run_command():
    global acquiring, frame_number
    cmd_path = request.args.get("path", "")
    if cmd_path == f"{args.detector_id}/start":
        with state_lock:
            acquiring = True
            frame_number = 0
            detector_variables["status"] = "busy"
        return jsonify({"status": "started"})
    elif cmd_path == f"{args.detector_id}/stop":
        with state_lock:
            acquiring = False
            detector_variables["status"] = "ready"
        return jsonify({"status": "stopped"})
    return jsonify({"error": "unknown command"}), 400


@app.get(f"/api/v1/devices/{args.device_id}/variables")
def get_variable():
    path = request.args.get("path")
    if not path:
        return jsonify({"error": "path parameter required"}), 400
    return jsonify(get_var(path))


@app.put(f"/api/v1/devices/{args.device_id}/variables")
def set_variable():
    path = request.args.get("path")
    value = request.args.get("value")
    if path is None or value is None:
        return jsonify({"error": "path and value parameters required"}), 400
    result = set_var(path, value)
    return jsonify(result)


def main():
    print(
        f"Starting XSPD simulator REST API on {args.host}:{args.port}, device-id={args.device_id}, detector-id={args.detector_id}"
    )
    app.run(host=args.host, port=args.port, debug=False, use_reloader=False)


if __name__ == "__main__":
    main()
