#!/usr/bin/env python3

import dbtoolspy
import argparse
import os

from dataclasses import dataclass

from logging import basicConfig, getLogger

from enum import Enum

basicConfig(level="INFO")
logger = getLogger("generate_param_defs")


class ParamType(Enum):
    INT = "asynInt32"
    DOUBLE = "asynFloat64"
    STRINGIN = "asynOctetRead"
    STRINGOUT = "asynOctetWrite"


@dataclass(frozen=True)
class ParamDef:
    record_str: str
    name: str
    type: ParamType


def get_internal_param_type_from_dtyp(dtyp):
    if dtyp in [ParamType.STRINGIN, ParamType.STRINGOUT]:
        return "asynParamOctet"
    else:
        return "asynParam" + dtyp.value.split("asyn")[-1]


def get_params_from_db(database, base_name):
    params = []
    for record in database.values():
        for field_name in ["OUT", "INP"]:
            if field_name in record.fields:
                param_string = record.fields[field_name].rsplit(")", 1)[-1]
                param_suffix = "".join(
                    [p.lower().capitalize() for p in param_string.split("_")[1:]]
                )
                param_name = f"{base_name}_{param_suffix}"
                params.append(
                    ParamDef(
                        record_str=param_string,
                        name=param_name,
                        type=ParamType(record.fields["DTYP"]),
                    )
                )
    return set(params)


def generate_header_file_for_db(params, output_dir, base_name):
    header_file = os.path.join(output_dir, f"{base_name}ParamDefs.h")

    with open(header_file, "w") as hf:
        hf.write(f"#ifndef {base_name.upper()}_PARAM_DEFS_H\n")
        hf.write(f"#define {base_name.upper()}_PARAM_DEFS_H\n\n")
        hf.write("// This file is auto-generated. Do not edit directly.\n")
        hf.write(f"// Generated from {base_name}.template\n\n")

        hf.write("// String definitions for parameters\n")
        for param in params:
            logger.info("Defining string for param: %s", param.name)
            hf.write(f'#define {param.name}String "{param.record_str}"\n')
        hf.write("\n")

        hf.write("// Parameter index definitions\n")
        for param in params:
            logger.info("Defining index for param: %s", param.name)
            hf.write(f"int {param.name};\n")

        hf.write(f"\n#define {base_name.upper()}_FIRST_PARAM {list(params)[0].name}\n")
        hf.write(f"#define {base_name.upper()}_LAST_PARAM {list(params)[-1].name}\n\n")
        hf.write(
            f"#define NUM_{base_name.upper()}_PARAMS ((int) (&{base_name.upper()}_LAST_PARAM - &{base_name.upper()}_FIRST_PARAM + 1))\n\n"
        )
        hf.write("#endif\n")


def generate_cpp_file_for_db(params, output_dir, base_name):
    cpp_file = os.path.join(output_dir, f"{base_name}ParamDefs.cpp")

    with open(cpp_file, "w") as cf:
        cf.write("// This file is auto-generated. Do not edit directly.\n")
        cf.write(f"// Generated from {base_name}.template\n\n")
        cf.write(f'#include "{base_name}.h"\n\n')
        cf.write(f"void {base_name}::createAllParams() {{\n")
        for param in params:
            logger.info("Creating param: %s", param.name)
            cf.write(
                f"    createParam({param.name}String, {get_internal_param_type_from_dtyp(param.type)}, &{param.name});\n"
            )
        cf.write("}\n")


def main():
    parser = argparse.ArgumentParser(
        description="Generate asyn parameter definitions from EPICS DB template."
    )
    parser.add_argument("template_file", help="Path to the EPICS DB template file.")
    parser.add_argument(
        "output_dir", help="Directory to save the generated header and source files."
    )
    parser.add_argument(
        "-m", "--macros", nargs="*", help="Optional macros to apply to the template."
    )

    args = parser.parse_args()
    template_file = args.template_file
    output_dir = args.output_dir
    base_name = os.path.splitext(os.path.basename(template_file))[0]

    database = dbtoolspy.load_database_file(template_file, macros=args.macros)
    params = get_params_from_db(database, base_name)
    for param in params:
        logger.info("Found param: %s of type %s", param.record_str, param.type.value)
    generate_header_file_for_db(params, output_dir, base_name)
    generate_cpp_file_for_db(params, output_dir, base_name)


if __name__ == "__main__":
    main()
