#!/usr/bin/env python3
#
#  Copyright (C) 2026 Team Kodi
#  This file is part of Kodi - https://kodi.tv
#
#  SPDX-License-Identifier: GPL-2.0-or-later
#  See LICENSES/README.md for more information.
#
"""Migrate the JSON-RPC service description schemas from JSON Schema draft-03
to the JSON Schema 2020-12 keyword dialect.

The three schema files stay flat one-definition-per-key maps because
JsonSchemaBuilder splits them by counting braces; only the shape inside each
definition changes:

  required: true on a property   ->  a "required" array on the containing schema
  "type": [ <inline schemas> ]   ->  "anyOf" (stray member "required" dropped)
  extends: "Name" / ["A", "B"]   ->  "allOf": [ {"$ref": "#/$defs/Name"}, ... ]
  "$ref": "Name"                 ->  "$ref": "#/$defs/Name"
  inline "id" named schemas      ->  hoisted into types.json
  flattened params               ->  content descriptors:
                                     { name, required?, description?, schema }

Params become content descriptors (the OpenRPC shape) because the flattened
form cannot carry both the param-level boolean "required" and a 2020-12
"required" array for the param's own object properties.

The transformation is verified structurally: the original files are read with
draft-03 semantics, the migrated files with 2020-12 semantics, and the two
canonical models must be identical.

Usage:
  migrate-schema-draft03-to-2020-12.py [--schema-dir DIR]          migrate in place
  migrate-schema-draft03-to-2020-12.py [--schema-dir DIR] --lint   CI check only
"""

import argparse
import json
import sys
from pathlib import Path

SCHEMA_FILES = ("methods.json", "types.json", "notifications.json")
REF_PREFIX = "#/$defs/"
PARAM_WRAPPER_KEYS = ("name", "required", "description")


# ---------------------------------------------------------------------------
# Raw-preserving JSON tree
# ---------------------------------------------------------------------------

class Scalar:
    """A JSON scalar kept as its raw source lexeme so numbers and escapes
    re-serialize byte-identically."""

    __slots__ = ("raw",)

    def __init__(self, raw):
        self.raw = raw

    @property
    def value(self):
        return json.loads(self.raw)

    @staticmethod
    def of(value):
        return Scalar(json.dumps(value))


class Arr:
    __slots__ = ("items",)

    def __init__(self, items=None):
        self.items = items if items is not None else []


class Obj:
    """A JSON object as an ordered list of (raw_key, key, node) entries."""

    __slots__ = ("entries",)

    def __init__(self, entries=None):
        self.entries = entries if entries is not None else []

    @staticmethod
    def entry(key, node):
        return (json.dumps(key), key, node)

    def keys(self):
        return [key for _, key, _ in self.entries]

    def has(self, key):
        return any(k == key for _, k, _ in self.entries)

    def get(self, key):
        for _, k, node in self.entries:
            if k == key:
                return node
        return None

    def index(self, key):
        for i, (_, k, _) in enumerate(self.entries):
            if k == key:
                return i
        return -1

    def set(self, key, node):
        i = self.index(key)
        if i >= 0:
            raw, k, _ = self.entries[i]
            self.entries[i] = (raw, k, node)
        else:
            self.entries.append(Obj.entry(key, node))

    def replace_key(self, key, new_key, node):
        i = self.index(key)
        self.entries[i] = (json.dumps(new_key), new_key, node)

    def insert_after(self, anchor_key, key, node):
        i = self.index(anchor_key)
        self.entries.insert(i + 1, Obj.entry(key, node))

    def remove(self, key):
        i = self.index(key)
        if i >= 0:
            del self.entries[i]


class Tokenizer:
    def __init__(self, text):
        self.text = text
        self.pos = 0

    def error(self, message):
        line = self.text.count("\n", 0, self.pos) + 1
        raise ValueError(f"line {line}: {message}")

    def skip_whitespace(self):
        while self.pos < len(self.text) and self.text[self.pos] in " \t\r\n":
            self.pos += 1

    def next(self):
        self.skip_whitespace()
        if self.pos >= len(self.text):
            return ("end", "")
        ch = self.text[self.pos]
        if ch in "{}[]:,":
            self.pos += 1
            return (ch, ch)
        if ch == '"':
            start = self.pos
            self.pos += 1
            while self.pos < len(self.text):
                if self.text[self.pos] == "\\":
                    self.pos += 2
                    continue
                if self.text[self.pos] == '"':
                    self.pos += 1
                    return ("scalar", self.text[start:self.pos])
                self.pos += 1
            self.error("unterminated string")
        start = self.pos
        while self.pos < len(self.text) and self.text[self.pos] not in " \t\r\n{}[]:,":
            self.pos += 1
        raw = self.text[start:self.pos]
        if raw in ("true", "false", "null") or raw[0] in "-0123456789":
            return ("scalar", raw)
        self.error(f"unexpected token {raw!r}")


def parse(text):
    tokens = Tokenizer(text)

    def parse_value(kind, raw):
        if kind == "scalar":
            return Scalar(raw)
        if kind == "{":
            obj = Obj()
            kind, raw = tokens.next()
            while kind != "}":
                if kind == ",":
                    kind, raw = tokens.next()
                    continue
                if kind != "scalar" or not raw.startswith('"'):
                    tokens.error(f"expected object key, got {raw!r}")
                key_raw = raw
                kind, _ = tokens.next()
                if kind != ":":
                    tokens.error("expected ':' after object key")
                kind, raw = tokens.next()
                obj.entries.append((key_raw, json.loads(key_raw), parse_value(kind, raw)))
                kind, raw = tokens.next()
            return obj
        if kind == "[":
            arr = Arr()
            kind, raw = tokens.next()
            while kind != "]":
                if kind == ",":
                    kind, raw = tokens.next()
                    continue
                arr.items.append(parse_value(kind, raw))
                kind, raw = tokens.next()
            return arr
        tokens.error(f"unexpected token {raw!r}")

    kind, raw = tokens.next()
    root = parse_value(kind, raw)
    kind, raw = tokens.next()
    if kind != "end":
        tokens.error(f"trailing content {raw!r}")
    return root


def serialize(node, indent=0):
    pad = " " * indent
    inner = " " * (indent + 2)
    if isinstance(node, Scalar):
        return node.raw
    if isinstance(node, Arr):
        if not node.items:
            return "[]"
        body = ",\n".join(inner + serialize(item, indent + 2) for item in node.items)
        return "[\n" + body + "\n" + pad + "]"
    if isinstance(node, Obj):
        if not node.entries:
            return "{}"
        body = ",\n".join(
            inner + raw_key + ": " + serialize(child, indent + 2)
            for raw_key, _, child in node.entries
        )
        return "{\n" + body + "\n" + pad + "}"
    raise TypeError(f"unexpected node {node!r}")


def to_python(node):
    if isinstance(node, Scalar):
        return node.value
    if isinstance(node, Arr):
        return [to_python(item) for item in node.items]
    if isinstance(node, Obj):
        return {key: to_python(child) for _, key, child in node.entries}
    raise TypeError(f"unexpected node {node!r}")


def is_scalar_of(node, *types):
    return isinstance(node, Scalar) and isinstance(node.value, types)


# ---------------------------------------------------------------------------
# Draft-03 -> 2020-12 transformation
# ---------------------------------------------------------------------------

class Migration:
    def __init__(self):
        self.hoisted = []  # (name, schema Obj) hoisted out of inline "id" sites
        self.dropped_root_required = []
        self.fixed_string_required = []
        self.errors = []

    def fail(self, path, message):
        self.errors.append(f"{path}: {message}")

    def transform_schema(self, schema, path):
        if not isinstance(schema, Obj):
            self.fail(path, "schema is not an object")
            return schema

        # Inline "id" names an anonymous schema: hoist it into types.json and
        # reference it. A boolean "required" at the site belongs to the
        # containing properties pass, which has already lifted it.
        id_node = schema.get("id")
        if id_node is not None and is_scalar_of(id_node, str):
            name = id_node.value
            hoisted = Obj([e for e in schema.entries if e[1] not in ("id", "required")])
            self.transform_schema(hoisted, f"$defs/{name}")
            self.hoisted.append((name, hoisted))
            schema.entries = []
            schema.set("$ref", Scalar.of(REF_PREFIX + name))
            return schema

        # extends -> allOf; the type of an extending schema is inherited, so a
        # "type" sibling must not exist (and must not be introduced)
        extends = schema.get("extends")
        if extends is not None:
            if schema.has("type"):
                self.fail(path, "extends with a type sibling")
            if is_scalar_of(extends, str):
                names = [extends.value]
            elif isinstance(extends, Arr):
                names = [item.value for item in extends.items]
            else:
                self.fail(path, "unexpected extends value")
                names = []
            all_of = Arr([Obj([Obj.entry("$ref", Scalar.of(REF_PREFIX + n))])
                          for n in names])
            schema.replace_key("extends", "allOf", all_of)

        ref = schema.get("$ref")
        if ref is not None and is_scalar_of(ref, str) and not ref.value.startswith("#"):
            schema.set("$ref", Scalar.of(REF_PREFIX + ref.value))

        type_node = schema.get("type")
        if isinstance(type_node, Arr):
            members = type_node.items
            if any(isinstance(m, Obj) for m in members):
                branches = []
                for i, member in enumerate(members):
                    if isinstance(member, Scalar):
                        branches.append(Obj([Obj.entry("type", Scalar(member.raw))]))
                    elif isinstance(member, Obj):
                        member.remove("required")
                        self.transform_schema(member, f"{path}/type[{i}]")
                        branches.append(member)
                    else:
                        self.fail(path, f"unexpected union member {i}")
                schema.replace_key("type", "anyOf", Arr(branches))
            # a pure list of type-name strings is already valid 2020-12

        properties = schema.get("properties")
        if isinstance(properties, Obj):
            required_names = []
            for _, prop_name, prop_schema in properties.entries:
                if isinstance(prop_schema, Obj):
                    required = prop_schema.get("required")
                    if required is not None and is_scalar_of(required, bool):
                        if required.value:
                            required_names.append(prop_name)
                        prop_schema.remove("required")
                    self.transform_schema(prop_schema,
                                          f"{path}/properties/{prop_name}")
                else:
                    self.fail(path, f"property {prop_name} is not an object")
            if required_names:
                schema.insert_after(
                    "properties", "required",
                    Arr([Scalar.of(n) for n in required_names]))

        items = schema.get("items")
        if isinstance(items, Obj):
            self.transform_schema(items, f"{path}/items")
        elif isinstance(items, Arr):
            self.fail(path, "tuple-form items is not used by this schema")

        additional = schema.get("additionalProperties")
        if isinstance(additional, Obj):
            self.transform_schema(additional, f"{path}/additionalProperties")

        # A boolean "required" still present at this level is a meaningless
        # draft-03 stray (e.g. on a global type root): drop it
        required = schema.get("required")
        if required is not None and is_scalar_of(required, bool):
            self.dropped_root_required.append(path)
            schema.remove("required")

        return schema

    def transform_param(self, param, path):
        """Flattened param -> content descriptor { name, required?, description?, schema }."""
        if not isinstance(param, Obj):
            self.fail(path, "param is not an object")
            return
        if param.has("schema"):
            self.fail(path, "param already has a schema member")
            return

        name = param.get("name")
        if name is None or not is_scalar_of(name, str):
            self.fail(path, "param has no name")
            return

        wrapper = []
        wrapper.append(Obj.entry("name", name))

        required = param.get("required")
        if required is not None and not is_scalar_of(required, bool):
            # a handful of params carry "required": "false" (a string); the
            # parser never read non-boolean values, so the param was optional
            self.fixed_string_required.append(path)
            required = None
        if required is not None and required.value:
            wrapper.append(Obj.entry("required", required))

        description = param.get("description")
        if description is not None:
            wrapper.append(Obj.entry("description", description))

        schema = Obj([e for e in param.entries if e[1] not in PARAM_WRAPPER_KEYS])
        self.transform_schema(schema, f"{path}/schema")
        wrapper.append(Obj.entry("schema", schema))

        param.entries = wrapper

    def transform_entry(self, name, definition):
        entry_type = definition.get("type")
        entry_kind = entry_type.value if is_scalar_of(entry_type, str) else None
        if entry_kind not in ("method", "notification"):
            self.transform_schema(definition, name)
            return

        params = definition.get("params")
        if isinstance(params, Arr):
            for i, param in enumerate(params.items):
                self.transform_param(param, f"{name}/params[{i}]")

        returns = definition.get("returns")
        if isinstance(returns, Obj):
            self.transform_schema(returns, f"{name}/returns")

    def transform_file(self, root):
        for _, name, definition in root.entries:
            self.transform_entry(name, definition)


# ---------------------------------------------------------------------------
# Semantic verification: draft-03 reading vs 2020-12 reading
# ---------------------------------------------------------------------------

MISSING = object()

SCHEMA_KEYWORDS_KEPT = ("minimum", "maximum", "minLength", "maxLength",
                        "minItems", "maxItems", "uniqueItems", "description")


def strip_boolean_required(data):
    return {k: v for k, v in data.items()
            if not (k == "required" and isinstance(v, bool))}


def canon_schema(data, draft03):
    """Reduce a schema (a plain dict) to a canonical model that is identical
    for a draft-03 schema and its 2020-12 migration."""
    if not isinstance(data, dict):
        return {"invalid": repr(data)}

    model = {}

    if "id" in data and draft03:
        # an inline named definition migrates to a reference to its hoisted
        # self; the hoisted content is compared separately
        return {"ref": data["id"]}

    ref = data.get("$ref")
    if isinstance(ref, str):
        name = ref[len(REF_PREFIX):] if ref.startswith(REF_PREFIX) else ref
        model["ref"] = name

    if draft03:
        extends = data.get("extends")
        if isinstance(extends, str):
            model["bases"] = [extends]
        elif isinstance(extends, list):
            model["bases"] = list(extends)
    else:
        all_of = data.get("allOf")
        if isinstance(all_of, list):
            model["bases"] = [
                member["$ref"][len(REF_PREFIX):] for member in all_of]

    type_value = data.get("type")
    union = None
    if draft03:
        if isinstance(type_value, list):
            if any(isinstance(m, dict) for m in type_value):
                union = type_value
            else:
                model["types"] = sorted(type_value)
        elif isinstance(type_value, str):
            model["types"] = [type_value]
    else:
        any_of = data.get("anyOf")
        if isinstance(any_of, list):
            union = any_of
        elif isinstance(type_value, list):
            model["types"] = sorted(type_value)
        elif isinstance(type_value, str):
            model["types"] = [type_value]

    if union is not None:
        members = []
        for member in union:
            if isinstance(member, str):
                members.append({"types": [member]})
            else:
                members.append(canon_schema(strip_boolean_required(member), draft03))
        model["union"] = members

    properties = data.get("properties")
    if isinstance(properties, dict):
        if draft03:
            required = sorted(name for name, prop in properties.items()
                              if isinstance(prop, dict)
                              and prop.get("required") is True)
        else:
            required = sorted(data.get("required", []))
        model["props"] = {
            name: canon_schema(
                strip_boolean_required(prop) if draft03 else prop, draft03)
            for name, prop in properties.items()}
        model["required"] = required

    items = data.get("items")
    if isinstance(items, dict):
        model["items"] = canon_schema(items, draft03)

    additional = data.get("additionalProperties")
    if isinstance(additional, dict):
        model["addprops"] = canon_schema(additional, draft03)
    elif isinstance(additional, bool):
        model["addprops"] = additional

    if "enum" in data:
        model["enum"] = data["enum"]
    if "default" in data:
        model["default"] = data["default"]
    for key in SCHEMA_KEYWORDS_KEPT:
        if key in data:
            model[key] = data[key]

    return model


def canon_param(param, draft03):
    if draft03:
        schema = {k: v for k, v in param.items() if k not in PARAM_WRAPPER_KEYS}
    else:
        schema = param.get("schema", {})
    return {"name": param.get("name"),
            "required": param.get("required") is True,
            "description": param.get("description"),
            "schema": canon_schema(schema, draft03)}


def canon_entry(data, draft03):
    if data.get("type") not in ("method", "notification"):
        if draft03:
            data = strip_boolean_required(data)
        return canon_schema(data, draft03)

    model = {"kind": data["type"]}
    for key in ("description", "transport", "permission"):
        if key in data:
            model[key] = data[key]

    params = data.get("params")
    if isinstance(params, list):
        model["params"] = [canon_param(param, draft03) for param in params]

    returns = data.get("returns")
    if isinstance(returns, dict):
        model["returns"] = canon_schema(returns, draft03)
    else:
        model["returns"] = returns

    return model


def collect_inline_ids(data, out):
    """Find draft-03 inline "id" definitions and record their content."""
    if isinstance(data, dict):
        if isinstance(data.get("id"), str):
            out[data["id"]] = {k: v for k, v in data.items()
                              if k not in ("id", "required")}
        for value in data.values():
            collect_inline_ids(value, out)
    elif isinstance(data, list):
        for value in data:
            collect_inline_ids(value, out)


def diff_models(before, after, path, out):
    if isinstance(before, dict) and isinstance(after, dict):
        for key in sorted(set(before) | set(after)):
            diff_models(before.get(key, MISSING), after.get(key, MISSING),
                        f"{path}/{key}", out)
        return
    if isinstance(before, list) and isinstance(after, list):
        if len(before) != len(after):
            out.append(f"{path}: length {len(before)} -> {len(after)}")
            return
        for i, (b, a) in enumerate(zip(before, after)):
            diff_models(b, a, f"{path}[{i}]", out)
        return
    if before != after:
        b = "<absent>" if before is MISSING else json.dumps(before)
        a = "<absent>" if after is MISSING else json.dumps(after)
        out.append(f"{path}: {b} -> {a}")


def verify(original_roots, migrated_roots):
    problems = []

    inline_ids = {}
    for filename in SCHEMA_FILES:
        collect_inline_ids(to_python(original_roots[filename]), inline_ids)

    migrated_types = to_python(migrated_roots["types.json"])

    for filename in SCHEMA_FILES:
        before_all = to_python(original_roots[filename])
        after_all = to_python(migrated_roots[filename])

        before_names = set(before_all)
        after_names = set(after_all)
        expected = before_names
        if filename == "types.json":
            expected = before_names | set(inline_ids)
        if after_names != expected:
            problems.append(f"{filename}: definition set changed: "
                            f"missing {sorted(expected - after_names)}, "
                            f"extra {sorted(after_names - expected)}")

        for name in sorted(before_names & after_names):
            diff_models(canon_entry(before_all[name], draft03=True),
                        canon_entry(after_all[name], draft03=False),
                        f"{filename}:{name}", problems)

    # hoisted inline definitions must survive with identical content
    for name, content in sorted(inline_ids.items()):
        if name not in migrated_types:
            problems.append(f"types.json: hoisted {name} is missing")
            continue
        diff_models(canon_schema(content, draft03=True),
                    canon_schema(migrated_types[name], draft03=False),
                    f"types.json:{name} (hoisted)", problems)

    return problems


# ---------------------------------------------------------------------------
# Lint: assert no draft-03 construct remains (CI mode)
# ---------------------------------------------------------------------------

def lint_schema(data, path, problems):
    if not isinstance(data, dict):
        return
    if "extends" in data:
        problems.append(f"{path}: draft-03 'extends'")
    if "id" in data:
        problems.append(f"{path}: draft-03 inline 'id'")
    if "enums" in data:
        problems.append(f"{path}: non-standard 'enums'")
    if "divisibleBy" in data:
        problems.append(f"{path}: draft-03 'divisibleBy'")
    if isinstance(data.get("required"), bool):
        problems.append(f"{path}: draft-03 boolean 'required'")
    ref = data.get("$ref")
    if isinstance(ref, str) and not ref.startswith(REF_PREFIX):
        problems.append(f"{path}: unqualified $ref {ref!r}")
    type_value = data.get("type")
    if isinstance(type_value, list) and any(isinstance(m, dict) for m in type_value):
        problems.append(f"{path}: draft-03 union type array")
    for key in ("items", "additionalProperties"):
        if isinstance(data.get(key), dict):
            lint_schema(data[key], f"{path}/{key}", problems)
    if isinstance(data.get("properties"), dict):
        for name, prop in data["properties"].items():
            lint_schema(prop, f"{path}/properties/{name}", problems)
    for group in ("anyOf", "allOf"):
        if isinstance(data.get(group), list):
            for i, member in enumerate(data[group]):
                lint_schema(member, f"{path}/{group}[{i}]", problems)


def lint_param(param, path, problems):
    if not isinstance(param, dict):
        problems.append(f"{path}: param is not an object")
        return
    unexpected = set(param) - {"name", "required", "description", "schema"}
    if unexpected:
        problems.append(f"{path}: flattened param (unexpected keys "
                        f"{sorted(unexpected)}); expected a content descriptor")
        return
    if "schema" not in param:
        problems.append(f"{path}: param has no schema")
        return
    if "required" in param and param["required"] is not True:
        problems.append(f"{path}: param 'required' must be true or absent")
    lint_schema(param["schema"], f"{path}/schema", problems)


def lint(roots):
    problems = []

    # types must stay topologically ordered: the C++ parser resolves an
    # out-of-order allOf reference against an unparsed stub and silently
    # degrades the derived type
    seen = set()
    for name, definition in to_python(roots["types.json"]).items():
        for member in definition.get("allOf", []) or []:
            ref = member.get("$ref", "")
            base = ref[len(REF_PREFIX):] if ref.startswith(REF_PREFIX) else ref
            if base not in seen:
                problems.append(f"types.json:{name}: allOf references {base} "
                                "before it is defined")
        seen.add(name)

    for filename in SCHEMA_FILES:
        for name, definition in to_python(roots[filename]).items():
            if definition.get("type") in ("method", "notification"):
                for i, param in enumerate(definition.get("params", []) or []):
                    lint_param(param, f"{filename}:{name}/params[{i}]", problems)
                returns = definition.get("returns")
                if isinstance(returns, dict):
                    lint_schema(returns, f"{filename}:{name}/returns", problems)
            else:
                lint_schema(definition, f"{filename}:{name}", problems)
    return problems


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def load(schema_dir):
    roots = {}
    texts = {}
    for filename in SCHEMA_FILES:
        text = (schema_dir / filename).read_text(encoding="utf-8")
        texts[filename] = text
        roots[filename] = parse(text)
    return roots, texts


def roundtrip_report(roots, texts):
    """The serializer must reproduce the input except for lines an earlier
    clang-format pass wrapped; those get unwrapped, which is reported."""
    reports = []
    for filename in SCHEMA_FILES:
        rendered = serialize(roots[filename]) + "\n"
        if rendered != texts[filename]:
            original_lines = texts[filename].splitlines()
            rendered_lines = rendered.splitlines()
            joined_original = "".join(line.strip() for line in original_lines)
            joined_rendered = "".join(line.strip() for line in rendered_lines)
            if joined_original.replace(" ", "") != joined_rendered.replace(" ", ""):
                reports.append((filename, "content drift, aborting", True))
            else:
                delta = len(original_lines) - len(rendered_lines)
                reports.append(
                    (filename, f"normalizes {delta} wrapped line(s)", False))
    return reports


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--schema-dir", type=Path,
                        default=Path(__file__).resolve().parents[2]
                        / "xbmc" / "interfaces" / "json-rpc" / "schema")
    parser.add_argument("--lint", action="store_true",
                        help="only check that no draft-03 construct remains")
    args = parser.parse_args()

    roots, texts = load(args.schema_dir)

    if args.lint:
        problems = lint(roots)
        for problem in problems:
            print(f"lint: {problem}", file=sys.stderr)
        return 1 if problems else 0

    for filename, message, fatal in roundtrip_report(roots, texts):
        print(f"roundtrip: {filename}: {message}", file=sys.stderr)
        if fatal:
            return 1

    originals, _ = load(args.schema_dir)

    migration = Migration()
    for filename in SCHEMA_FILES:
        migration.transform_file(roots[filename])
    for name, schema in migration.hoisted:
        roots["types.json"].set(name, schema)

    if migration.errors:
        for error in migration.errors:
            print(f"error: {error}", file=sys.stderr)
        return 1

    problems = verify(originals, roots)
    if problems:
        for problem in problems:
            print(f"verify: {problem}", file=sys.stderr)
        return 1

    problems = lint(roots)
    if problems:
        for problem in problems:
            print(f"lint: {problem}", file=sys.stderr)
        return 1

    for filename in SCHEMA_FILES:
        (args.schema_dir / filename).write_text(serialize(roots[filename]) + "\n",
                                                encoding="utf-8")

    print(f"migrated {', '.join(SCHEMA_FILES)}; "
          f"hoisted {len(migration.hoisted)} inline id definition(s); "
          f"dropped {len(migration.dropped_root_required)} stray root "
          f"'required' flag(s)")
    for path in migration.dropped_root_required:
        print(f"  dropped root required: {path}")
    for path in migration.fixed_string_required:
        print(f"  dropped non-boolean param required: {path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
