#!/usr/bin/env python3

"""
FastAPI + ZMQ based simulator for the ADXSPD driver.

Loads initial state from a JSON dump file (same format as
xspdApp/tests/samples/xspd_sample_responses.json) and serves
the REST API expected by the ADXSPD EPICS driver while streaming
simulated image data over ZMQ.

Usage:
    python xspdSimulator.py [OPTIONS] dump_file

Example:
    python xspdSimulator.py ../xspdApp/tests/samples/xspd_sample_responses.json
"""

from __future__ import annotations

import argparse
import copy
import enum
import json
import struct
import threading
import time
from dataclasses import dataclass
from typing import Any, Dict, List, Optional, Type

import numpy as np
import uvicorn
import zmq
from fastapi import FastAPI, Query, HTTPException
from fastapi.responses import JSONResponse

# ---------------------------------------------------------------------------
# Enums mirroring XSPDAPI.h  (case-insensitive matching via helper)
# ---------------------------------------------------------------------------


class OnOff(enum.IntEnum):
    OFF = 0
    ON = 1


class Compressor(enum.IntEnum):
    NONE = 0
    ZLIB = 1
    BLOSC = 2


class ShuffleMode(enum.IntEnum):
    NO_SHUFFLE = 0
    AUTO_SHUFFLE = 1
    SHUFFLE_BIT = 2
    SHUFFLE_BYTE = 3


class TriggerMode(enum.IntEnum):
    SOFTWARE = 0
    EXT_FRAMES = 1
    EXT_SEQ = 2


class CounterMode(enum.IntEnum):
    SINGLE = 0
    DUAL = 1


class Status(enum.IntEnum):
    CONNECTED = 0
    READY = 1
    BUSY = 2


def enum_names(cls: Type[enum.IntEnum]) -> List[str]:
    return [m.name for m in cls]


def enum_lookup(cls: Type[enum.IntEnum], value_str: str) -> str:
    """Case-insensitive enum name lookup.  Returns the canonical name."""
    upper = value_str.upper()
    for m in cls:
        if m.name == upper:
            return m.name
    raise ValueError(
        f"Invalid value '{value_str}' for {cls.__name__}. Allowed: {enum_names(cls)}"
    )


# ---------------------------------------------------------------------------
# Variable specification / validation harness
# ---------------------------------------------------------------------------


@dataclass
class VarSpec:
    vtype: type  # int, float, bool, str
    min_val: Optional[float] = None
    max_val: Optional[float] = None
    allowed: Optional[List[Any]] = None
    enum_class: Optional[Type[enum.IntEnum]] = None


# Keyed by the *last* component of the variable path (the variable name).
# Some names appear in multiple scopes (detector, module, data-port) — the
# same spec applies everywhere.
VARIABLE_SPECS: Dict[str, VarSpec] = {
    "beam_energy": VarSpec(float, min_val=0.0),
    "bit_depth": VarSpec(int, allowed=[1, 6, 12, 24]),
    "charge_summing": VarSpec(str, enum_class=OnOff),
    "compression_level": VarSpec(int, min_val=0, max_val=9),
    "compressor": VarSpec(str, enum_class=Compressor),
    "counter_mode": VarSpec(str, enum_class=CounterMode),
    "countrate_correction": VarSpec(str, enum_class=OnOff),
    "countrate_correction_enabled": VarSpec(bool),
    "flatfield_correction": VarSpec(str, enum_class=OnOff),
    "flatfield_enabled": VarSpec(bool),
    "frame_depth": VarSpec(int, allowed=[1, 6, 12, 24]),
    "frame_height": VarSpec(int, min_val=1),
    "frame_width": VarSpec(int, min_val=1),
    "frames_queued": VarSpec(int, min_val=0),
    "gating_mode": VarSpec(str, enum_class=OnOff),
    "humidity": VarSpec(float, min_val=0.0, max_val=100.0),
    "interpolation": VarSpec(str, enum_class=OnOff),
    "interpolation_enabled": VarSpec(bool),
    "max_frames": VarSpec(int, min_val=1),
    "n_connectors": VarSpec(int, min_val=0),
    "n_frames": VarSpec(int, min_val=1),
    "n_modules": VarSpec(int, min_val=1),
    "n_subframes": VarSpec(int, min_val=1),
    "pixel_mask_enabled": VarSpec(bool),
    "ram_allocated": VarSpec(bool),
    "roi_rows": VarSpec(int, min_val=1),
    "saturation_flag": VarSpec(str, enum_class=OnOff),
    "saturation_flag_enabled": VarSpec(bool),
    "saturation_threshold": VarSpec(int, min_val=0),
    "sensor_current": VarSpec(float, min_val=0.0),
    "shuffle_mode": VarSpec(str, enum_class=ShuffleMode),
    "shutter_time": VarSpec(float, min_val=0.0),
    "status": VarSpec(str),  # managed internally
    "summed_frames": VarSpec(int, min_val=0),
    "trigger_mode": VarSpec(str, enum_class=TriggerMode),
    "voltage": VarSpec(float, min_val=0.0),
}


def _var_suffix(path: str) -> str:
    """Extract the variable name (last component) from a path like 'lambda/1/voltage'."""
    return path.rsplit("/", 1)[-1]


def coerce_value(var_path: str, raw: str) -> Any:
    """Coerce a raw string value to the appropriate type per VARIABLE_SPECS.

    Returns the coerced value.  Raises ValueError on validation failure.
    """
    suffix = _var_suffix(var_path)
    spec = VARIABLE_SPECS.get(suffix)

    if spec is None:
        # Unknown variable — attempt simple type inference from string
        if raw.lower() in ("true", "false"):
            return raw.lower() == "true"
        try:
            return int(raw)
        except ValueError:
            pass
        try:
            return float(raw)
        except ValueError:
            pass
        return raw

    # Enum-backed string
    if spec.enum_class is not None:
        canonical = enum_lookup(spec.enum_class, raw)
        return canonical

    # Boolean
    if spec.vtype is bool:
        if raw.lower() in ("true", "1"):
            return True
        if raw.lower() in ("false", "0"):
            return False
        raise ValueError(f"Invalid boolean value '{raw}' for {var_path}")

    # Numeric
    if spec.vtype is int:
        val = int(raw)
    elif spec.vtype is float:
        val = float(raw)
    else:
        return raw  # str, no further validation

    # Range check
    if spec.min_val is not None and val < spec.min_val:
        raise ValueError(f"Value {val} for {var_path} is below minimum {spec.min_val}")
    if spec.max_val is not None and val > spec.max_val:
        raise ValueError(f"Value {val} for {var_path} is above maximum {spec.max_val}")
    if spec.allowed is not None and val not in spec.allowed:
        raise ValueError(
            f"Value {val} for {var_path} not in allowed set {spec.allowed}"
        )
    return val


# ---------------------------------------------------------------------------
# Simulator state
# ---------------------------------------------------------------------------


class SimulatorState:
    """Central mutable state for the simulator, shared between REST handlers
    and ZMQ publisher thread(s)."""

    def __init__(self) -> None:
        self.lock = threading.Lock()
        self.acquiring = False
        self.frame_number = 0

        # Populated by load_dump()
        self.device_id: str = ""
        self.detector_id: str = ""
        self.module_ids: List[str] = []  # e.g. ["lambda/1"]
        self.data_port_ids: List[str] = []  # e.g. ["port-1"]

        # Flat variable store: { "lambda/shutter_time": 1.0, ... }
        self.variables: Dict[str, Any] = {}

        # Raw dump sections kept for direct serving
        self.api_response: Dict[str, Any] = {}
        self.devices_response: Dict[str, Any] = {}
        self.device_info: Dict[str, Any] = {}
        self.info_response: Dict[str, Any] = {}
        self.variables_list: List[Dict[str, Any]] = []
        self.commands_list: List[Dict[str, Any]] = []

    # Convenience accessors (must hold lock externally or accept races for
    # read-only access — the publisher thread only reads these).
    def get_var(self, path: str, default: Any = None) -> Any:
        return self.variables.get(path, default)

    def set_var(self, path: str, value: Any) -> None:
        self.variables[path] = value


# ---------------------------------------------------------------------------
# JSON dump loader
# ---------------------------------------------------------------------------


def load_dump(filepath: str, state: SimulatorState) -> None:
    """Parse xspd_sample_responses.json format into SimulatorState."""

    with open(filepath, "r") as f:
        dump: Dict[str, Any] = json.load(f)

    # --- Static endpoint responses ---
    state.api_response = dump.get("api", {"api version": "1", "xspd version": "1.6.0"})
    state.devices_response = dump.get("api/v1/devices", {"devices": []})

    # Discover device ID
    devices = state.devices_response.get("devices", [])
    if not devices:
        raise RuntimeError("No devices found in dump file")
    state.device_id = devices[0]["id"]

    device_key = f"api/v1/devices/{state.device_id}"
    state.device_info = dump.get(device_key, {})

    # Discover detector ID, module IDs, data-port IDs from device info
    system = state.device_info.get("system", {})
    detectors = system.get("detectors", [])
    if detectors:
        state.detector_id = detectors[0].get("id", "")

    data_ports = system.get("data-ports", [])
    state.data_port_ids = [dp["id"] for dp in data_ports]

    # Module IDs from the info variable
    info_key = f"{device_key}/variables?path=info"
    state.info_response = dump.get(info_key, {})
    info_val = state.info_response.get("value", {})
    info_detectors = info_val.get("detectors", [])
    if info_detectors:
        modules = info_detectors[0].get("modules", [])
        state.module_ids = [m["module"] for m in modules]

    # Variables list (descriptions)
    vars_list_key = f"{device_key}/variables"
    state.variables_list = dump.get(vars_list_key, [])

    # Commands list
    cmds_key = f"{device_key}/commands"
    state.commands_list = dump.get(cmds_key, [])

    # --- Load individual variable values ---
    var_prefix = f"{device_key}/variables?path="
    for key, resp in dump.items():
        if key.startswith(var_prefix) and isinstance(resp, dict) and "value" in resp:
            var_path = resp.get("path", key[len(var_prefix) :])
            state.variables[var_path] = resp["value"]


# ---------------------------------------------------------------------------
# Image generation helpers
# ---------------------------------------------------------------------------


def _dtype_for_bit_depth(bit_depth: int) -> np.dtype:
    if bit_depth <= 8:
        return np.dtype(np.uint8)
    elif bit_depth <= 16:
        return np.dtype(np.uint16)
    else:
        return np.dtype(np.uint32)


def _max_for_dtype(dt: np.dtype) -> int:
    return int(np.iinfo(dt).max)


def _draw_streak(
    img: np.ndarray, max_val: int, dt: np.dtype, intensity: float = 1.0
) -> None:
    """Draw a single cosmic-ray-like streak onto img (in-place).

    The streak is 5-100 pixels long and 1-5 pixels wide, kept fairly
    straight: the cumulative angular deviation from the initial direction
    is clamped to +/-10 degrees.

    intensity (0-1) scales the pixel brightness range.
    """
    height, width = img.shape
    length = np.random.randint(5, 101)
    base_width = np.random.randint(1, 6)

    # Random starting point
    y = np.random.randint(0, height)
    x = np.random.randint(0, width)

    # Random dominant direction (angle in radians)
    base_angle = np.random.uniform(0, 2 * np.pi)
    max_deviation = np.radians(10.0)  # max 10 degrees from the base direction

    # Scale pixel value range by intensity
    low_val = max(1, int(max_val * 0.3 * intensity))
    high_val = max(low_val + 1, int(max_val * intensity) + 1)

    cy, cx = float(y), float(x)
    for step in range(length):
        # Small per-step jitter, but clamp total deviation to +/-10 degrees
        deviation = np.random.uniform(-max_deviation, max_deviation)
        angle = base_angle + deviation
        cx += np.cos(angle)
        cy += np.sin(angle)

        iy, ix_ = int(round(cy)), int(round(cx))

        # Vary width per-pixel for slight irregularity
        w = max(1, base_width + np.random.randint(-1, 2))
        half_w = w // 2

        for wy in range(iy - half_w, iy + half_w + 1):
            for wx in range(ix_ - half_w, ix_ + half_w + 1):
                if 0 <= wy < height and 0 <= wx < width:
                    img[wy, wx] = np.random.randint(low_val, high_val)


def generate_module_image(
    width: int,
    height: int,
    bit_depth: int,
    threshold_low: float,
    frame_number: int,
    shutter_time_ms: float = 1000.0,
) -> np.ndarray:
    """Generate a single module image with noise shaped by threshold_low.

    threshold_low < 2  -> heavy noise (most pixels random)
    threshold_low 2-5  -> progressively sparser
    threshold_low > 5  -> nearly empty, occasional streaks simulating cosmic rays or similar

    Longer shutter_time_ms increases cosmic ray probability and intensity.
    """
    dt = _dtype_for_bit_depth(bit_depth)
    max_val = _max_for_dtype(dt)

    if threshold_low < 2.0:
        # Heavy noise -- full random image
        keep_fraction = 1.0
    elif threshold_low <= 5.0:
        # Exponential decay from ~80% at 2 to ~6% at 5
        keep_fraction = 0.8 * np.exp(-0.864 * (threshold_low - 2.0))
    else:
        # Steeper exponential from 6% at 5, approaching zero gradually
        # ~0.5% at 6, ~0.04% at 7, never hard-clips to zero
        keep_fraction = 0.06 * np.exp(-2.5 * (threshold_low - 5.0))

    # Generate base image
    if keep_fraction < 1e-6:
        img = np.zeros((height, width), dtype=dt)
    else:
        img = np.random.randint(0, max_val + 1, size=(height, width), dtype=dt)
        if keep_fraction < 1.0:
            mask = np.random.random((height, width)) < keep_fraction
            img[~mask] = 0

    # Add rare cosmic-ray streaks for threshold > 3
    # Probability scales with shutter time: longer exposures collect more
    # cosmic rays.  Base probability at 1000ms is 0.05; scales linearly
    # with shutter_time_ms, capped at 0.2 (1 in 5).
    # Each additional streak after the first has exponentially decaying
    # probability: p_next = p_first * 0.4^k.
    # Intensity also scales with shutter time (more charge deposited).
    if threshold_low > 3.0:
        # Base probability at 1000ms -> 0.05, scales linearly, cap at 0.2
        p_first = min(0.2, 0.05 * (shutter_time_ms / 1000.0))
        # Intensity factor: 0.4 at 1ms, 1.0 at 1000ms+
        intensity = min(1.0, 0.4 + 0.6 * (shutter_time_ms / 1000.0))

        max_streaks = 7
        n_streaks = 0
        if np.random.random() < p_first:
            n_streaks = 1
            # Each subsequent: exponential decay  p_first * 0.4^k
            for k in range(1, max_streaks):
                p_next = p_first * (0.4**k)
                if np.random.random() < p_next:
                    n_streaks += 1
                else:
                    break
        for _ in range(n_streaks):
            _draw_streak(img, max_val, dt, intensity)

    return img


def generate_stitched_image(
    state: SimulatorState,
    bit_depth: int,
    threshold_low: float,
    frame_number: int,
    shutter_time_ms: float = 1000.0,
) -> np.ndarray:
    """Generate a stitched image from all modules, placed by their positions.

    Gaps between/around modules are zero-filled.
    """
    # Find data port dimensions for the full canvas
    dp_id = state.data_port_ids[0] if state.data_port_ids else "port-1"
    canvas_w = int(state.get_var(f"{dp_id}/frame_width", 1024))
    canvas_h = int(state.get_var(f"{dp_id}/frame_height", 1024))
    dt = _dtype_for_bit_depth(bit_depth)

    canvas = np.zeros((canvas_h, canvas_w), dtype=dt)

    for mod_id in state.module_ids:
        mod_w = int(state.get_var(f"{mod_id}/frame_width", canvas_w))
        mod_h = int(state.get_var(f"{mod_id}/frame_height", canvas_h))
        position = state.get_var(f"{mod_id}/position", [0.0, 0.0, 0.0])

        # position = [x, y, z] in pixel coordinates
        px = int(position[0])
        py = int(position[1])

        mod_img = generate_module_image(
            mod_w, mod_h, bit_depth, threshold_low, frame_number, shutter_time_ms
        )

        # Clip to canvas bounds
        src_y_end = min(mod_h, canvas_h - py)
        src_x_end = min(mod_w, canvas_w - px)
        if src_y_end > 0 and src_x_end > 0 and py >= 0 and px >= 0:
            canvas[py : py + src_y_end, px : px + src_x_end] = mod_img[
                :src_y_end, :src_x_end
            ]

    return canvas


def apply_high_threshold(img: np.ndarray, threshold_high: float) -> np.ndarray:
    """Create dual-counter Frame B: zero out pixels below cutoff derived from threshold_high."""
    dt = img.dtype
    max_val = _max_for_dtype(dt)

    # Scale threshold against pixel range: cutoff is proportional
    # threshold_high is in keV-like units; map against dtype max
    cutoff = int(threshold_high / 10.0 * max_val)
    cutoff = max(0, min(cutoff, max_val))

    result = img.copy()
    result[result < cutoff] = 0
    return result


def compress_data(raw_bytes: bytes, compressor_name: str) -> bytes:
    """Optionally compress pixel data.  Returns raw_bytes if compressor is NONE."""
    upper = compressor_name.upper() if isinstance(compressor_name, str) else "NONE"
    if upper == "NONE":
        return raw_bytes
    if upper == "ZLIB":
        try:
            import zlib as _zlib
        except ImportError:
            raise RuntimeError("zlib module not available for compression")
        return _zlib.compress(raw_bytes)
    if upper == "BLOSC":
        try:
            import blosc as _blosc
        except ImportError:
            raise RuntimeError(
                "blosc package not installed. Install with: pip install blosc"
            )
        return _blosc.compress(raw_bytes, typesize=1)
    return raw_bytes


# ---------------------------------------------------------------------------
# ZMQ publisher thread
# ---------------------------------------------------------------------------


def publisher_thread(
    state: SimulatorState,
    zmq_context: zmq.Context,
    stitched: bool,
    base_data_port: int,
) -> None:
    """Background thread that streams frames over ZMQ when acquiring."""

    # Build sockets
    sockets: List[zmq.Socket] = []
    if stitched:
        sock = zmq_context.socket(zmq.PUB)
        sock.bind(f"tcp://*:{base_data_port}")
        sockets.append(sock)
        print(f"[ZMQ] Stitched publisher bound to tcp://*:{base_data_port}")
    else:
        for i, mod_id in enumerate(state.module_ids):
            sock = zmq_context.socket(zmq.PUB)
            port = base_data_port + i
            sock.bind(f"tcp://*:{port}")
            sockets.append(sock)
            print(f"[ZMQ] Module {mod_id} publisher bound to tcp://*:{port}")

    def send_frame(
        sock: zmq.Socket, pixel_data: bytes, frame_num: int, trigger_num: int
    ) -> None:
        header = struct.pack("BBBB", frame_num & 0xFF, trigger_num & 0xFF, 0, 0)
        sock.send_multipart([b"", header, pixel_data])

    try:
        while True:
            # Check acquisition state
            with state.lock:
                local_acquiring = state.acquiring
            if not local_acquiring:
                time.sleep(0.05)
                continue

            # Read parameters (accept minor races for performance)
            det_id = state.detector_id
            n_frames = int(state.get_var(f"{det_id}/n_frames", 1))
            bit_depth = int(state.get_var(f"{det_id}/bit_depth", 12))
            shutter_time_ms = float(state.get_var(f"{det_id}/shutter_time", 1000.0))
            counter_mode = str(
                state.get_var(f"{det_id}/counter_mode", "SINGLE")
            ).upper()
            compressor_name = str(state.get_var(f"{det_id}/compressor", "NONE"))

            thresholds = state.get_var(f"{det_id}/thresholds", [])
            threshold_low = float(thresholds[0]) if len(thresholds) > 0 else 0.0
            threshold_high = float(thresholds[1]) if len(thresholds) > 1 else 5.0

            with state.lock:
                frame_num = state.frame_number

            if frame_num >= n_frames:
                # Auto-stop
                with state.lock:
                    state.acquiring = False
                    state.frame_number = 0
                state.set_var(f"{det_id}/status", "ready")
                for mod_id in state.module_ids:
                    state.set_var(f"{mod_id}/status", "ready")
                print(f"[ACQ] Completed {n_frames} frames, auto-stopped")
                continue

            # Sleep for shutter_time before generating/sending the frame
            time.sleep(shutter_time_ms / 1000.0)

            # Re-check acquisition state after sleep (may have been stopped)
            with state.lock:
                if not state.acquiring:
                    continue

            # Generate frame(s)
            if stitched:
                img = generate_stitched_image(
                    state, bit_depth, threshold_low, frame_num, shutter_time_ms
                )
                raw = img.tobytes()
                compressed = compress_data(raw, compressor_name)
                send_frame(sockets[0], compressed, frame_num, 0)

                if counter_mode == "DUAL":
                    img_b = apply_high_threshold(img, threshold_high)
                    raw_b = img_b.tobytes()
                    compressed_b = compress_data(raw_b, compressor_name)
                    send_frame(sockets[0], compressed_b, frame_num, 1)
            else:
                # Non-stitched: one frame per module per socket
                for i, mod_id in enumerate(state.module_ids):
                    mod_w = int(state.get_var(f"{mod_id}/frame_width", 1024))
                    mod_h = int(state.get_var(f"{mod_id}/frame_height", 1024))
                    img = generate_module_image(
                        mod_w,
                        mod_h,
                        bit_depth,
                        threshold_low,
                        frame_num,
                        shutter_time_ms,
                    )
                    raw = img.tobytes()
                    compressed = compress_data(raw, compressor_name)
                    send_frame(sockets[i], compressed, frame_num, 0)

                    if counter_mode == "DUAL":
                        img_b = apply_high_threshold(img, threshold_high)
                        raw_b = img_b.tobytes()
                        compressed_b = compress_data(raw_b, compressor_name)
                        send_frame(sockets[i], compressed_b, frame_num, 1)

            # Update counters
            with state.lock:
                state.frame_number += 1
                fn = state.frame_number

            for dp_id in state.data_port_ids:
                state.set_var(f"{dp_id}/frames_queued", fn)
            for mod_id in state.module_ids:
                state.set_var(f"{mod_id}/frames_queued", fn)

    except zmq.ContextTerminated:
        pass
    finally:
        for s in sockets:
            s.close(linger=0)


# ---------------------------------------------------------------------------
# FastAPI application factory
# ---------------------------------------------------------------------------


def create_app(state: SimulatorState, stitched: bool, base_data_port: int) -> FastAPI:
    app = FastAPI(title="XSPD Simulator")

    @app.get("/api")
    def api_root():
        return JSONResponse(content=state.api_response)

    @app.get("/api/v1/devices")
    def list_devices():
        return JSONResponse(content=state.devices_response)

    @app.get("/api/v1/devices/{device_id}")
    def device_info(device_id: str):
        info = copy.deepcopy(state.device_info)
        # Dynamically adjust data-ports based on stitched flag
        system = info.get("system", {})
        if stitched:
            dp_id = state.data_port_ids[0] if state.data_port_ids else "port-1"
            system["data-ports"] = [
                {
                    "ref": state.module_ids[0] if state.module_ids else "",
                    "id": dp_id,
                    "ip": "0.0.0.0",
                    "port": base_data_port,
                }
            ]
        else:
            ports = []
            for i, mod_id in enumerate(state.module_ids):
                dp_id = f"port-{i + 1}"
                ports.append(
                    {
                        "ref": mod_id,
                        "id": dp_id,
                        "ip": "0.0.0.0",
                        "port": base_data_port + i,
                    }
                )
            system["data-ports"] = ports
        info["system"] = system
        return JSONResponse(content=info)

    @app.get("/api/v1/devices/{device_id}/variables")
    def get_variable(device_id: str, path: Optional[str] = Query(default=None)):
        if path is None:
            # Return the variable listing
            return JSONResponse(content=state.variables_list)

        # Special case: info endpoint
        if path == "info":
            return JSONResponse(content=state.info_response)

        value = state.get_var(path)
        if value is None:
            raise HTTPException(status_code=404, detail=f"Variable '{path}' not found")
        return JSONResponse(content={"path": path, "value": value})

    @app.put("/api/v1/devices/{device_id}/variables")
    def set_variable(
        device_id: str,
        path: str = Query(...),
        value: str = Query(...),
    ):
        # Special handling for thresholds (comma-separated -> list of floats)
        suffix = _var_suffix(path)
        if suffix == "thresholds":
            parts = [p.strip() for p in value.split(",") if p.strip()]
            try:
                new_val = [float(p) for p in parts]
            except ValueError:
                raise HTTPException(
                    status_code=400, detail=f"Invalid threshold values: {value}"
                )
            state.set_var(path, new_val)
            return JSONResponse(content={"path": path, "value": new_val})

        try:
            new_val = coerce_value(path, value)
        except ValueError as e:
            raise HTTPException(status_code=400, detail=str(e))

        state.set_var(path, new_val)
        return JSONResponse(content={"path": path, "value": new_val})

    @app.get("/api/v1/devices/{device_id}/commands")
    def list_commands(device_id: str):
        return JSONResponse(content=state.commands_list)

    @app.put("/api/v1/devices/{device_id}/commands")
    def run_command(device_id: str, path: str = Query(...)):
        det_id = state.detector_id

        # Normalise: "start" and "<det>/start" both trigger start
        cmd = path
        if cmd.startswith(f"{det_id}/"):
            cmd = cmd[len(det_id) + 1 :]

        if cmd == "start":
            with state.lock:
                state.acquiring = True
                state.frame_number = 0
            state.set_var(f"{det_id}/status", "busy")
            for mod_id in state.module_ids:
                state.set_var(f"{mod_id}/status", "busy")
            print("[CMD] Acquisition started")
            return JSONResponse(content={"status": "started"})

        elif cmd == "stop":
            with state.lock:
                state.acquiring = False
                state.frame_number = 0
            state.set_var(f"{det_id}/status", "ready")
            for mod_id in state.module_ids:
                state.set_var(f"{mod_id}/status", "ready")
            print("[CMD] Acquisition stopped")
            return JSONResponse(content={"status": "stopped"})

        elif cmd == "reset":
            with state.lock:
                state.acquiring = False
                state.frame_number = 0
            state.set_var(f"{det_id}/status", "ready")
            for mod_id in state.module_ids:
                state.set_var(f"{mod_id}/status", "ready")
            for dp_id in state.data_port_ids:
                state.set_var(f"{dp_id}/frames_queued", 0)
            print("[CMD] Detector reset")
            return JSONResponse(content={"status": "reset"})

        # Unknown command -- just acknowledge
        return JSONResponse(content={"status": "ok", "command": path})

    return app


# ---------------------------------------------------------------------------
# CLI & main
# ---------------------------------------------------------------------------


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="XSPD Detector Simulator (FastAPI + ZMQ)"
    )
    parser.add_argument(
        "dump_file",
        type=str,
        help="Path to JSON dump file (xspd_sample_responses.json format)",
    )
    parser.add_argument(
        "--host",
        type=str,
        default="0.0.0.0",
        help="Host address for the REST API (default: 0.0.0.0)",
    )
    parser.add_argument(
        "--port",
        type=int,
        default=8008,
        help="Port number for the REST API (default: 8008)",
    )
    parser.add_argument(
        "--data-port",
        type=int,
        default=4300,
        help="Base ZMQ data port for frame streaming (default: 4300)",
    )
    parser.add_argument(
        "--stitched",
        action=argparse.BooleanOptionalAction,
        default=True,
        help="Enable stitched image mode (single data port). Use --no-stitched for per-module ports.",
    )
    parser.add_argument(
        "--compressor",
        type=str,
        choices=["none", "zlib", "blosc"],
        default="none",
        help="Initial compressor for ZMQ frame data (default: none)",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()

    # Load state from dump file
    state = SimulatorState()
    load_dump(args.dump_file, state)

    # Override compressor from CLI
    det_id = state.detector_id
    state.set_var(f"{det_id}/compressor", args.compressor.upper())
    for mod_id in state.module_ids:
        state.set_var(f"{mod_id}/compressor", args.compressor.upper())

    print(f"[INIT] Device: {state.device_id}")
    print(f"[INIT] Detector: {state.detector_id}")
    print(f"[INIT] Modules: {state.module_ids}")
    print(f"[INIT] Data ports: {state.data_port_ids}")
    print(f"[INIT] Stitched: {args.stitched}")
    print(f"[INIT] Compressor: {args.compressor}")
    print(f"[INIT] Variables loaded: {len(state.variables)}")

    # Start ZMQ publisher thread
    zmq_context = zmq.Context.instance()
    pub_thread = threading.Thread(
        target=publisher_thread,
        args=(state, zmq_context, args.stitched, args.data_port),
        daemon=True,
    )
    pub_thread.start()

    # Create and run FastAPI app
    app = create_app(state, args.stitched, args.data_port)

    print(f"[INIT] Starting REST API on {args.host}:{args.port}")
    uvicorn.run(app, host=args.host, port=args.port, log_level="info")


if __name__ == "__main__":
    main()
