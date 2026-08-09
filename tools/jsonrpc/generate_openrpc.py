#!/usr/bin/env python3
#
#  Copyright (C) 2026 Team Kodi
#  This file is part of Kodi - https://kodi.tv
#
#  SPDX-License-Identifier: GPL-2.0-or-later
#  See LICENSES/README.md for more information.
#
"""Generate docs/jsonrpc/openrpc.json from the JSON-RPC service description.

Produces an OpenRPC 1.3.2 document covering all request/response methods.
Notifications are push-only and documented separately in asyncapi.json by
generate_asyncapi.py.  The output is deterministic: running the generator
twice yields byte-identical files.
"""

import kodi_schema

OUTPUT_PATH = kodi_schema.REPO_ROOT / "docs" / "jsonrpc" / "openrpc.json"

SCHEMA_PREFIX = "#/components/schemas/"

DESCRIPTION = (
    "Kodi's JSON-RPC 2.0 API. All methods are served from a single endpoint "
    "over HTTP POST and over WebSocket/TCP. Server-initiated notifications "
    "are only delivered over WebSocket/TCP and are documented separately in "
    "asyncapi.json.")


def build_method(name, method, taxonomy):
    params = []
    for descriptor in method.get("params", []):
        entry = {}
        for key, value in descriptor.items():
            if key == "schema":
                entry[key] = kodi_schema.rewrite_refs(value, SCHEMA_PREFIX)
            else:
                entry[key] = value
        params.append(entry)
    result_schema = kodi_schema.rewrite_refs(
        kodi_schema.returns_schema(method["returns"]), SCHEMA_PREFIX)
    entry = {
        "name": name,
        "summary": method["description"],
        "tags": [{"name": kodi_schema.namespace_of(name)}],
        "paramStructure": "either",
        "params": params,
        "result": {"name": "result", "schema": result_schema},
        "errors": [{"$ref": "#/components/errors/" + error["name"]}
                   for error in taxonomy],
        "x-kodi-permission": method["permission"],
    }
    if "transport" in method:
        entry["x-kodi-transport"] = method["transport"]
    return entry


def build_document():
    version = kodi_schema.load_version()
    taxonomy = kodi_schema.load_error_taxonomy()
    service = kodi_schema.load_service()
    methods = [build_method(name, method, taxonomy)
               for name, method in service["methods"].items()]
    schemas = {name: kodi_schema.rewrite_refs(schema, SCHEMA_PREFIX)
               for name, schema in service["types"].items()}
    errors = {}
    for error in taxonomy:
        errors[error["name"]] = {
            "code": error["code"],
            "message": error["message"],
            "data": {"description": error["description"]},
        }
    return {
        "openrpc": "1.3.2",
        "info": {
            "title": "Kodi JSON-RPC API",
            "version": version,
            "description": DESCRIPTION,
            "license": {"name": "GPL-2.0-or-later"},
        },
        "servers": [
            {
                "name": "http",
                "url": "http://{host}:{port}/jsonrpc",
                "variables": {
                    "host": {"default": "localhost"},
                    "port": {"default": "8080"},
                },
            },
            {
                "name": "websocket",
                "url": "ws://{host}:{port}/jsonrpc",
                "variables": {
                    "host": {"default": "localhost"},
                    "port": {"default": "9090"},
                },
            },
        ],
        "methods": methods,
        "components": {
            "schemas": schemas,
            "errors": errors,
        },
    }


def main():
    kodi_schema.dump_document(build_document(), OUTPUT_PATH)


if __name__ == "__main__":
    main()
