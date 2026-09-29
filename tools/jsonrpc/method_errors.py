#!/usr/bin/env python3
#
#  Copyright (C) 2026 Team Kodi
#  This file is part of Kodi - https://kodi.tv
#
#  SPDX-License-Identifier: GPL-2.0-or-later
#  See LICENSES/README.md for more information.
#
"""Derive which errors and reasons each JSON-RPC method can fail with.

The errors a method can return are the JSONRPC_STATUS names its handler's
body mentions, plus those of every JSONRPC_STATUS function it calls,
transitively.  Its reasons are found the same way, from each Fail() call,
which names the status and the reason together; a reason is declared under
every status a call pairs it with.  This module computes that closure from
the source text in xbmc/interfaces/json-rpc and compares it with the
"errors" and "reasons" members each method declares in methods.json.

    python tools/jsonrpc/method_errors.py            # report any drift
    python tools/jsonrpc/method_errors.py --write    # declare the derived sets
    python tools/jsonrpc/method_errors.py --explain Player.Open

The derived sets are supersets of the truth: a status named in a comparison
counts as if it were returned.  They are never subsets, which is the
property a client generated from the declarations depends on.
"""

import json
import re
import sys
from collections import defaultdict
from pathlib import Path

import kodi_schema

SOURCE_DIR = kodi_schema.SCHEMA_DIR.parent
METHOD_MAP = SOURCE_DIR / "JSONServiceDescription.cpp"

# Produced before a handler runs, so every method can return them
PRE_DISPATCH = ("InvalidRequest", "MethodNotFound", "InvalidParams",
                "ParseError", "BadPermission")
SUCCESS = ("OK", "ACK")

_DEFINITION = re.compile(
    r"^[ \t]*(?:static\s+)?JSONRPC_STATUS\s+((?:JSONRPC::)?(?:\w+::)?\w+)\s*\(",
    re.MULTILINE)
# A call written without an object: a static of the same class, a qualified
# static, or a free function; obj.f( and ptr->f( are excluded.
_CALL = re.compile(r"(?<![\w.])(?<!->)((?:\w+::)?\w+)\s*\(")
# A function passed by name as an argument, which the callee may call.
_ARGUMENT = re.compile(r"[(,]\s*((?:\w+::)?\w+)\s*(?=[,)])")
_REASON = re.compile(r"\bReason::(\w+)\b")
_FAIL = re.compile(r"(?<![\w.:>])Fail\s*\(")
# A handler may be a template instantiated for the method, e.g. CLib::List<Kind::Movie>; its
# statuses are the template's.
_MAP_ENTRY = re.compile(r'\{\s*"([\w.]+)"\s*,\s*(\w+::\w+)(?:<[^{}]*>)?\s*\}')


def _strip(text):
    """Blank comments and string literals so their contents are not parsed."""
    out = []
    i, n = 0, len(text)
    while i < n:
        if text.startswith("//", i):
            end = text.find("\n", i)
            i = n if end < 0 else end
        elif text.startswith("/*", i):
            end = text.find("*/", i + 2)
            end = n if end < 0 else end + 2
            out.append("\n" * text.count("\n", i, end))
            i = end
        elif text[i] in "\"'":
            quote = text[i]
            i += 1
            while i < n and text[i] != quote:
                i += 2 if text[i] == "\\" else 1
            out.append(quote + quote)
            i += 1
        else:
            out.append(text[i])
            i += 1
    return "".join(out)


def _matching(text, start, open_char, close_char):
    depth = 0
    for i in range(start, len(text)):
        if text[i] == open_char:
            depth += 1
        elif text[i] == close_char:
            depth -= 1
            if depth == 0:
                return i
    raise ValueError(f"unbalanced {open_char} at offset {start}")


def _definitions(text):
    """Yield (qualified name, body) for each JSONRPC_STATUS function defined."""
    for match in _DEFINITION.finditer(text):
        params_end = _matching(text, match.end() - 1, "(", ")")
        body_start = params_end + 1
        while text[body_start] not in "{;":
            body_start += 1
        if text[body_start] == ";":
            continue
        body_end = _matching(text, body_start, "{", "}")
        name = match.group(1).removeprefix("JSONRPC::")
        yield name, text[body_start:body_end + 1]


def _status_names(taxonomy):
    return [error["name"] for error in taxonomy]


def _arguments(text):
    """Split a call's argument text at its top-level commas."""
    args, depth, current = [], 0, []
    for char in text:
        if char in "([{":
            depth += 1
        elif char in ")]}":
            depth -= 1
        if char == "," and depth == 0:
            args.append("".join(current))
            current = []
        else:
            current.append(char)
    args.append("".join(current))
    return args


def _reason_pairs(name, body, status_pattern, by_enumerator):
    """Return the (status, reason name) pairs the Fail() calls in body make."""
    pairs = set()
    covered = 0
    for match in _FAIL.finditer(body):
        close = _matching(body, match.end() - 1, "(", ")")
        args = _arguments(body[match.end():close])
        if len(args) < 3:
            continue
        statuses = set(status_pattern.findall(args[1])) - set(SUCCESS)
        enumerators = _REASON.findall(args[2])
        if not statuses or not enumerators:
            raise ValueError(f"{name} calls Fail() without naming its status and reason")
        covered += len(enumerators)
        for enumerator in enumerators:
            if enumerator not in by_enumerator:
                raise ValueError(f"{name} names Reason::{enumerator}, which is not described")
            pairs |= {(status, by_enumerator[enumerator]["name"]) for status in statuses}
    if covered != len(_REASON.findall(body)):
        raise ValueError(f"{name} names a reason outside a Fail() call")
    return pairs


def build_graph(source_dir=SOURCE_DIR, taxonomy=None, reasons=None):
    """Return (failures, calls) per JSONRPC_STATUS function in source_dir.

    A function's failures are the error names and the (error, reason) pairs
    its own body names, without following calls.
    """
    taxonomy = taxonomy or kodi_schema.load_error_taxonomy()
    reasons = reasons or kodi_schema.load_reason_taxonomy()
    by_enumerator = {reason["enumerator"]: reason for reason in reasons}
    status_pattern = re.compile(
        r"\b(" + "|".join(_status_names(taxonomy) + list(SUCCESS)) + r")\b")
    bodies = {}
    for pattern in ("*.h", "*.cpp"):
        for path in sorted(Path(source_dir).glob(pattern)):
            text = _strip(path.read_text(encoding="utf-8"))
            bodies.update(_definitions(text))
    by_bare_name = defaultdict(set)
    for name in bodies:
        by_bare_name[name.rsplit("::", 1)[-1]].add(name)

    failures = {}
    calls = {}
    for name, body in bodies.items():
        # Fail() forwards what its callers name
        pairs = set() if name == "Fail" else _reason_pairs(name, body, status_pattern,
                                                           by_enumerator)
        failures[name] = (set(status_pattern.findall(body)) - set(SUCCESS)) | pairs
        cls = name.split("::")[0] if "::" in name else None
        callees = set()
        for callee in _CALL.findall(body) + _ARGUMENT.findall(body):
            if "::" in callee:
                if callee in bodies:
                    callees.add(callee)
            else:
                candidates = by_bare_name.get(callee, set())
                same_class = {c for c in candidates
                              if cls and c.startswith(cls + "::")}
                free = {c for c in candidates if "::" not in c}
                # a bare call that neither the class nor a free function defines reaches a
                # static inherited from the one class that does
                inherited = {c for c in candidates if "::" in c}
                callees |= same_class or free or (inherited if len(inherited) == 1 else set())
        callees.discard(name)
        calls[name] = callees
    return failures, calls


def method_handlers(method_map=METHOD_MAP):
    """Return {method name: handler function} from the C++ method table."""
    text = Path(method_map).read_text(encoding="utf-8")
    return dict(_MAP_ENTRY.findall(text))


def derive_all(source_dir=SOURCE_DIR, taxonomy=None, reasons=None):
    """Return {method name: {"errors": [...], "reasons": {error: [...]}}}.

    Errors, and the errors reasons are declared under, are in taxonomy order;
    reasons are in the order of the reason table.
    """
    taxonomy = taxonomy or kodi_schema.load_error_taxonomy()
    reasons = reasons or kodi_schema.load_reason_taxonomy()
    error_order = {name: index for index, name in enumerate(_status_names(taxonomy))}
    reason_order = {reason["name"]: index for index, reason in enumerate(reasons)}
    failures, calls = build_graph(source_dir, taxonomy, reasons)

    closure = {name: set(found) for name, found in failures.items()}
    changed = True
    while changed:
        changed = False
        for name, callees in calls.items():
            merged = closure[name].union(*(closure[c] for c in callees))
            if merged != closure[name]:
                closure[name] = merged
                changed = True

    handlers = method_handlers(Path(source_dir) / METHOD_MAP.name)
    derived = {}
    for method, handler in handlers.items():
        if handler not in closure:
            raise ValueError(f"{method} maps to {handler}, which is not defined")
        found = closure[handler]
        errors = sorted({f for f in found if isinstance(f, str)}, key=error_order.__getitem__)
        pairs = [f for f in found if isinstance(f, tuple)]
        derived[method] = {
            "errors": errors,
            "reasons": {error: sorted((r for e, r in pairs if e == error),
                                      key=reason_order.__getitem__)
                        for error in errors if any(e == error for e, _ in pairs)},
        }
    return derived


def derive(source_dir=SOURCE_DIR, taxonomy=None):
    """Return {method name: [error names]} in taxonomy order."""
    return {method: failures["errors"]
            for method, failures in derive_all(source_dir, taxonomy).items()}


def derive_reasons(source_dir=SOURCE_DIR):
    """Return {method name: {error: [reason names]}}."""
    return {method: failures["reasons"]
            for method, failures in derive_all(source_dir).items()}


def explain(method, source_dir=SOURCE_DIR):
    """Print the call tree behind a method's derived errors and reasons."""
    failures, calls = build_graph(source_dir)
    handler = method_handlers(Path(source_dir) / METHOD_MAP.name)[method]

    def describe(found):
        return sorted(f if isinstance(f, str) else f"{f[0]}/{f[1]}" for f in found)

    def walk(name, depth, seen):
        print("  " * depth + name, describe(failures[name]))
        for callee in sorted(calls[name]):
            if callee not in seen:
                seen.add(callee)
                walk(callee, depth + 1, seen)

    walk(handler, 0, {handler})


def declared(schema_dir=kodi_schema.SCHEMA_DIR):
    methods = kodi_schema.load_service(schema_dir)["methods"]
    return {name: {"errors": method.get("errors"), "reasons": method.get("reasons")}
            for name, method in methods.items()}


def write(schema_dir=kodi_schema.SCHEMA_DIR, source_dir=SOURCE_DIR):
    """Declare the derived errors and reasons on every method in methods.json."""
    path = Path(schema_dir) / "methods.json"
    raw = path.read_bytes().decode("utf-8")
    methods = json.loads(raw)
    derived = derive_all(source_dir)
    for name, method in methods.items():
        method.pop("errors", None)
        method.pop("reasons", None)
        entries = list(method.items())
        after = next((i for i, (key, _) in enumerate(entries)
                      if key == "returns"), len(entries) - 1)
        entries.insert(after + 1, ("errors", derived[name]["errors"]))
        entries.insert(after + 2, ("reasons", derived[name]["reasons"]))
        methods[name] = dict(entries)
    text = json.dumps(methods, indent=2, ensure_ascii=False) + "\n"
    if "\r\n" in raw:
        text = text.replace("\n", "\r\n")
    path.write_bytes(text.encode("utf-8"))


def main(argv):
    if len(argv) == 2 and argv[0] == "--explain":
        explain(argv[1])
        return 0
    if argv == ["--write"]:
        write()
        return 0
    if argv:
        print(__doc__, file=sys.stderr)
        return 2
    schema = declared()
    drift = {name: failures for name, failures in derive_all().items()
             if schema.get(name) != failures}
    if not drift:
        print("every method declares the errors and reasons its handler can fail with")
        return 0
    for name, failures in drift.items():
        declaration = schema.get(name) or {}
        for member in ("errors", "reasons"):
            if declaration.get(member) != failures[member]:
                print(f"{name}: declares {member} {declaration.get(member)}, "
                      f"handler has {failures[member]}")
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
