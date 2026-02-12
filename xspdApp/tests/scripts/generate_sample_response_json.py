#!/usr/bin/env python3

"""
This script generates a JSON file containing sample responses from the XSPD API.

It queries various endpoints of the API and saves the responses in a structured format.
"""

import json
import requests
from pathlib import Path
import logging
import argparse



logging.basicConfig()
logger = logging.getLogger("gen_sample_responses")
logger.level = logging.INFO

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
    device_variables.update(get_sample_endpoint_value(base_uri, var_endpoint + "?path=info"))
    variables = get_sample_endpoint_value(base_uri, var_endpoint)
    device_variables.update(variables)
    for variable in variables[get_full_endpoint(base_uri, var_endpoint)]:
        variable_id = variable["path"]
        device_variables.update(get_sample_endpoint_value(base_uri, f"{var_endpoint}?path={variable_id}"))
    return device_variables


def main():
    parser = argparse.ArgumentParser(description="Utility for pulling all values from a running xspd instance into a json file")

    parser.add_argument("-u", "--uri", default="localhost:8008", help="Base URI on which xspd is running")

    base_uri = vars(parser.parse_args())["uri"]
    base_uri = base_uri + "/" if not base_uri.endswith("/") else base_uri

    sample_responses = {}

    api_info = get_sample_endpoint_value(base_uri, "api")
    base_uri += "api/v" + api_info[get_full_endpoint(base_uri, "api")]["api version"] + "/"

    all_device_info = get_sample_endpoint_value(base_uri, "devices")
    sample_responses.update(api_info)
    sample_responses.update(all_device_info)

    for device in all_device_info[get_full_endpoint(base_uri, "devices")]["devices"]:
        device_id = device["id"]
        sample_responses.update(get_sample_endpoint_value(base_uri, f"devices/{device_id}"))
        sample_responses.update(get_device_variable_values(base_uri, device_id))
        sample_responses.update(get_sample_endpoint_value(base_uri, f"devices/{device_id}/commands"))


    with open(Path(__file__).parent.absolute() / "../samples/xspd_sample_responses.json", "w") as fp:
        fp.write(json.dumps(sample_responses, indent=4))

if __name__ == "__main__":
    main()