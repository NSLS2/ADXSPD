#!/usr/bin/env python3

import json
import requests
import logging

SAMPLE_RESPONSE_DICT = {}

BASE_URI = "localhost:8008/api"


def save_sample_endpoint_value(endpoint):
    url = BASE_URI + endpoint
    response = requests.get("http://" + url)
    response.raise_for_status()
    SAMPLE_RESPONSE_DICT[url] = response.json()
    return response.json()


def save_device_variable_values(device_id):
    var_endpoint = f"devices/{device_id}/variables"
    save_sample_endpoint_value(var_endpoint + "?path=info")
    variables = save_sample_endpoint_value(var_endpoint)
    for variable in variables:
        variable_id = variable["path"]
        save_sample_endpoint_value(f"{var_endpoint}?path={variable_id}")


api_info = save_sample_endpoint_value("")
BASE_URI += "/v" + api_info["api version"] + "/"

all_device_info = save_sample_endpoint_value("devices")

for device in all_device_info["devices"]:
    device_id = device["id"]
    device_info = save_sample_endpoint_value(f"devices/{device_id}")
    save_device_variable_values(device_id)
    save_sample_endpoint_value(f"devices/{device_id}/commands")


with open("xspd_sample_responses.json", "w") as fp:
    fp.write(json.dumps(SAMPLE_RESPONSE_DICT, indent=4))
