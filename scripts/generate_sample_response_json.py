#!/usr/bin/env python3

"""
This script generates a JSON file containing sample responses from the XSPD API.

It queries various endpoints of the API and saves the responses in a structured format.
"""

import json
import requests
from pathlib import Path
import logging
import sys
import argparse


logging.basicConfig()
logger = logging.getLogger("gen_sample_responses")

class ColorFormatter(logging.Formatter):
    """ANSI color formatter for warnings and errors."""

    COLOR_MAP = {
        logging.DEBUG: "\033[36m",  # Cyan
        logging.INFO: "\033[32m",  # Green
        logging.WARNING: "\033[33;1m",  # Bright Yellow
        logging.ERROR: "\033[31;1m",  # Bright Red
        logging.CRITICAL: "\033[41;97m",  # White on Red bg
    }
    RESET = "\033[0m"

    def __init__(self, fmt: str, use_color: bool = True):
        super().__init__(fmt)
        self.use_color = use_color

    def format(self, record: logging.LogRecord) -> str:
        if self.use_color and record.levelno in self.COLOR_MAP:
            # Temporarily modify the levelname with color codes
            original_levelname = record.levelname
            # Pad to 8 characters (length of "CRITICAL") for consistent alignment
            padded_levelname = original_levelname.ljust(8)
            record.levelname = (
                f"{self.COLOR_MAP[record.levelno]}{padded_levelname}{self.RESET}"
            )
            base = super().format(record)
            # Restore the original levelname
            record.levelname = original_levelname
            return base
        # For non-colored output, still pad for consistency
        original_levelname = record.levelname
        record.levelname = original_levelname.ljust(8)
        base = super().format(record)
        record.levelname = original_levelname
        return base


handler = logging.StreamHandler()
use_color = sys.stderr.isatty()
fmt = "%(asctime)s | %(levelname)-8s | %(name)s | %(message)s"
handler.setFormatter(ColorFormatter(fmt, use_color=use_color))
logger.addHandler(handler)
logger.setLevel(logging.INFO)
logger.propagate = False


def get_full_endpoint(base_uri, endpoint):
    return base_uri.split("/", 1)[-1] + endpoint


def get_sample_endpoint_value(base_uri, endpoint):
    url = base_uri + endpoint
    logger.info(f"Getting value of endpoint {endpoint}")
    response = requests.get("http://" + url)
    response.raise_for_status()
    return {get_full_endpoint(base_uri, endpoint): response.json()}


def get_device_variable_values(base_uri, device_id):
    device_variables = {}
    var_endpoint = f"devices/{device_id}/variables"
    device_variables.update(
        get_sample_endpoint_value(base_uri, var_endpoint + "?path=info")
    )
    variables = get_sample_endpoint_value(base_uri, var_endpoint)
    device_variables.update(variables)
    for variable in variables[get_full_endpoint(base_uri, var_endpoint)]:
        variable_id = variable["path"]
        device_variables.update(
            get_sample_endpoint_value(base_uri, f"{var_endpoint}?path={variable_id}")
        )
    return device_variables


def main():
    parser = argparse.ArgumentParser(
        description="Utility for pulling all values from a running xspd instance into a json file"
    )

    parser.add_argument(
        "output_path",
        help="Path where the generated JSON file should be saved. If the file already exists, it will be overwritten.",
    )

    parser.add_argument(
        "-u",
        "--uri",
        default="localhost",
        help="Base URI on which xspd is running",
    )

    parser.add_argument(
        "-p",
        "--port",
        default=8008,
        type=int,
        help="Port on which xspd is running"
    )

    args = parser.parse_args()
    base_uri = args.uri
    port = args.port
    base_uri = f"{base_uri}:{port}/" if not base_uri.endswith("/") else f"{base_uri}:{port}/"

    sample_responses = {}

    api_info = get_sample_endpoint_value(base_uri, "api")
    base_uri += (
        "api/v" + api_info[get_full_endpoint(base_uri, "api")]["api version"] + "/"
    )

    all_device_info = get_sample_endpoint_value(base_uri, "devices")
    sample_responses.update(api_info)
    sample_responses.update(all_device_info)

    for device in all_device_info[get_full_endpoint(base_uri, "devices")]["devices"]:
        device_id = device["id"]
        sample_responses.update(
            get_sample_endpoint_value(base_uri, f"devices/{device_id}")
        )
        sample_responses.update(get_device_variable_values(base_uri, device_id))
        sample_responses.update(
            get_sample_endpoint_value(base_uri, f"devices/{device_id}/commands")
        )

    with open(
        Path(__file__).parent.absolute() / "../samples/xspd_sample_responses.json", "w"
    ) as fp:
        fp.write(json.dumps(sample_responses, indent=4))


if __name__ == "__main__":
    main()
