#!/usr/bin/env python3
#
#  Copyright (C) 2026 Team Kodi
#  This file is part of Kodi - https://kodi.tv
#
#  SPDX-License-Identifier: GPL-2.0-or-later
#  See LICENSES/README.md for more information.
#
"""Generate the static HTML documentation site for Kodi's JSON-RPC API.

Renders the service description loaded by kodi_schema into a fully static
site: a landing page, one reference page per method, notification and type,
namespace-grouped indexes, and the error taxonomy.  The machine-readable
openrpc.json and asyncapi.json artifacts are copied in verbatim under a
major-version directory.  Every link is relative so the site works from
any base path (a github.io project page or a domain root).  No JavaScript
is emitted and the output is deterministic: running the generator twice
yields byte-identical files.

Usage: python tools/jsonrpc/generate_site.py [--out DIR]
"""

import argparse
import html
import json
import shutil
from pathlib import Path

import kodi_schema

DOCS_DIR = kodi_schema.REPO_ROOT / "docs" / "jsonrpc"
DEFAULT_OUT = DOCS_DIR / "site"
EXAMPLES_DIR = Path(__file__).resolve().parent / "examples"
GITIGNORE_PATH = DOCS_DIR / ".gitignore"

DEFS_PREFIX = "#/$defs/"

CONSTRAINT_KEYS = ("minimum", "maximum", "exclusiveMinimum",
                   "exclusiveMaximum", "minLength", "maxLength", "minItems",
                   "maxItems", "multipleOf", "format")


def esc(text):
    return html.escape(str(text), quote=True)


def dumps(value):
    return json.dumps(value, ensure_ascii=False)


def pre_json(value):
    text = json.dumps(value, indent=2, ensure_ascii=False)
    return f"<pre><code>{esc(text)}</code></pre>"


def ref_name(schema):
    return schema["$ref"][len(DEFS_PREFIX):]


def transports(entity):
    value = entity.get("transport")
    if value is None:
        return []
    if isinstance(value, str):
        return [value]
    return list(value)


def constraints_span(schema):
    parts = []
    for key in CONSTRAINT_KEYS:
        if key in schema:
            parts.append(f"{key}: {dumps(schema[key])}")
    if schema.get("uniqueItems"):
        parts.append("unique items")
    if not parts:
        return ""
    return f' <span class="constraints">({esc("; ".join(parts))})</span>'


def is_simple(schema):
    """True when the schema renders to a single inline phrase."""
    if not isinstance(schema, dict):
        return True
    if "$ref" in schema:
        return True
    if "anyOf" in schema:
        return all(is_simple(branch) for branch in schema["anyOf"])
    if "allOf" in schema or "properties" in schema:
        return False
    if isinstance(schema.get("additionalProperties"), dict):
        return False
    if schema.get("type") == "array":
        return is_simple(schema.get("items", {}))
    return True


class SiteBuilder:

    def __init__(self, out_dir):
        self.out = Path(out_dir)
        self.version = kodi_schema.load_version()
        self.vdir = "v" + self.version.split(".")[0]
        self.taxonomy = kodi_schema.load_error_taxonomy()
        self.service = kodi_schema.load_service()
        self.examples = self._load_examples()
        self.reverse_refs = self._reverse_refs()
        self.written = []

    # ------------------------------------------------------------------
    # infrastructure

    def _load_examples(self):
        examples = []
        for path in sorted(EXAMPLES_DIR.glob("*.json")):
            with open(path, encoding="utf-8") as handle:
                examples.append(json.load(handle))
        return examples

    def _reverse_refs(self):
        """Map type name -> ordered list of (kind, name) referencing it."""
        reverse = {}
        sections = (("method", self.service["methods"]),
                    ("notification", self.service["notifications"]),
                    ("type", self.service["types"]))
        for kind, entities in sections:
            for name, entity in entities.items():
                for target in sorted(kodi_schema.collect_refs(entity)):
                    if kind == "type" and target == name:
                        continue
                    reverse.setdefault(target, []).append((kind, name))
        return reverse

    def write(self, relative, text):
        path = self.out / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8", newline="\n")
        self.written.append(relative)

    def page(self, relative, title, body):
        """Wrap body in the shared page chrome and write it."""
        depth = relative.count("/")
        rel = "../" * depth
        nav = " ".join(
            f'<a href="{rel}{self.vdir}/{target}">{label}</a>'
            for label, target in (("Methods", "methods/index.html"),
                                  ("Notifications", "notifications/index.html"),
                                  ("Types", "types/index.html"),
                                  ("Errors", "errors.html")))
        text = (
            "<!DOCTYPE html>\n"
            '<html lang="en">\n'
            "<head>\n"
            '<meta charset="utf-8">\n'
            '<meta name="viewport" content="width=device-width, '
            'initial-scale=1">\n'
            f"<title>{esc(title)}</title>\n"
            f'<link rel="stylesheet" href="{rel}style.css">\n'
            "</head>\n"
            "<body>\n"
            '<header><div class="inner">'
            f'<a class="site" href="{rel}index.html">Kodi JSON-RPC API</a> '
            f'<span class="badge">{esc(self.version)}</span>'
            f"<nav>{nav}</nav>"
            "</div></header>\n"
            f"<main>\n{body}</main>\n"
            "</body>\n"
            "</html>\n")
        self.write(relative, text)

    def type_link(self, name, depth):
        href = "../" * depth + f"{self.vdir}/types/{name}.html"
        return f'<a href="{esc(href)}">{esc(name)}</a>'

    # ------------------------------------------------------------------
    # schema rendering

    def render_inline(self, schema, depth):
        """Render a schema as an inline phrase (type-column style)."""
        if not isinstance(schema, dict) or not schema:
            return "any"
        if "$ref" in schema:
            return self.type_link(ref_name(schema), depth) \
                + constraints_span(schema)
        if "anyOf" in schema:
            branches = schema["anyOf"]
            if all(is_simple(branch) for branch in branches):
                return " | ".join(self.render_inline(branch, depth)
                                  for branch in branches)
            items = "".join(f"<li>{self.render_inline(branch, depth)}</li>"
                            for branch in branches)
            return f'<ul class="branches">{items}</ul>'
        if "allOf" in schema:
            bases = ", ".join(self.render_inline(base, depth)
                              for base in schema["allOf"])
            own = ", ".join(esc(name) for name in schema.get("properties", ()))
            suffix = f' <span class="props">{{{own}}}</span>' if own else ""
            return f"object extending {bases}{suffix}"
        if "enum" in schema:
            values = " | ".join(f"<code>{esc(dumps(value))}</code>"
                                for value in schema["enum"])
            return values + constraints_span(schema)
        stype = schema.get("type")
        if isinstance(stype, list):
            return esc(" | ".join(stype)) + constraints_span(schema)
        if stype == "array":
            items = self.render_inline(schema.get("items", {}), depth)
            return f"array of {items}" + constraints_span(schema)
        if stype == "object":
            names = ", ".join(esc(name)
                              for name in schema.get("properties", ()))
            if names:
                return f'object <span class="props">{{{names}}}</span>'
            extra = schema.get("additionalProperties")
            if isinstance(extra, dict):
                rendered = self.render_inline(extra, depth)
                return f"object of {rendered} values"
            return "object"
        if stype is None:
            return "any"
        return esc(stype) + constraints_span(schema)

    def properties_table(self, schema, depth):
        required = set(schema.get("required", ()))
        rows = []
        for name, prop in schema.get("properties", {}).items():
            prop = prop if isinstance(prop, dict) else {}
            default = ""
            if "default" in prop:
                default = f"<code>{esc(dumps(prop['default']))}</code>"
            description = esc(prop.get("description", ""))
            rows.append(
                "<tr>"
                f"<td><code>{esc(name)}</code></td>"
                f"<td>{self.render_inline(prop, depth)}</td>"
                f"<td>{'Yes' if name in required else 'No'}</td>"
                f"<td>{default}</td>"
                f"<td>{description}</td>"
                "</tr>")
        extra = schema.get("additionalProperties")
        if isinstance(extra, dict):
            rows.append(
                "<tr>"
                "<td><em>any name</em></td>"
                f"<td>{self.render_inline(extra, depth)}</td>"
                "<td>No</td><td></td>"
                f"<td>{esc(extra.get('description', ''))}</td>"
                "</tr>")
        return (
            '<div class="tablewrap"><table>'
            "<thead><tr><th>Name</th><th>Type</th><th>Required</th>"
            "<th>Default</th><th>Description</th></tr></thead>"
            f"<tbody>{''.join(rows)}</tbody>"
            "</table></div>")

    def render_block(self, schema, depth):
        """Render a schema as a block (returns section / type pages)."""
        if not isinstance(schema, dict) or not schema:
            return "<p>any</p>"
        parts = []
        if schema.get("description"):
            parts.append(f'<p class="muted">{esc(schema["description"])}</p>')
        if "allOf" in schema:
            bases = ", ".join(self.render_inline(base, depth)
                              for base in schema["allOf"])
            parts.append(f"<p>Extends {bases}</p>")
        if "properties" in schema \
                or isinstance(schema.get("additionalProperties"), dict):
            parts.append(self.properties_table(schema, depth))
        elif "enum" in schema:
            items = "".join(f"<li><code>{esc(dumps(value))}</code></li>"
                            for value in schema["enum"])
            parts.append(f'<ul class="enum-list">{items}</ul>'
                         + constraints_span(schema))
        elif "anyOf" in schema and not is_simple(schema):
            items = "".join(f"<li>{self.render_block(branch, depth)}</li>"
                            for branch in schema["anyOf"])
            parts.append(f'<ol class="branches">{items}</ol>')
        elif schema.get("type") == "array" \
                and not is_simple(schema.get("items", {})):
            parts.append("<p>array of" + constraints_span(schema) + ":</p>")
            parts.append(self.render_block(schema.get("items", {}), depth))
        elif "allOf" not in schema:
            parts.append(f"<p>{self.render_inline(schema, depth)}</p>")
        return "".join(parts)

    # ------------------------------------------------------------------
    # request/message skeletons

    def placeholder(self, schema):
        """Plain-text type placeholder for an envelope skeleton."""
        if not isinstance(schema, dict) or not schema:
            return "any"
        if "$ref" in schema:
            return ref_name(schema)
        if "anyOf" in schema:
            labels = list(dict.fromkeys(self.placeholder(branch)
                                        for branch in schema["anyOf"]))
            return " | ".join(labels)
        if "enum" in schema:
            values = [dumps(value) for value in schema["enum"]]
            if len(values) > 4:
                values = values[:4] + ["..."]
            return " | ".join(values)
        stype = schema.get("type")
        if isinstance(stype, list):
            return " | ".join(stype)
        if stype == "array":
            return f"array of {self.placeholder(schema.get('items', {}))}"
        if stype == "object" and schema.get("properties"):
            return "{" + ", ".join(schema["properties"]) + "}"
        return stype or "any"

    def envelope_skeleton(self, name, entity, notification=False):
        envelope = {"jsonrpc": "2.0"}
        if not notification:
            envelope["id"] = 1
        envelope["method"] = name
        params = entity.get("params", [])
        if params:
            envelope["params"] = {
                descriptor["name"]:
                    f"<{self.placeholder(descriptor['schema'])}>"
                for descriptor in params}
        return envelope

    # ------------------------------------------------------------------
    # shared page fragments

    def params_table(self, params, depth):
        rows = []
        for descriptor in params:
            schema = descriptor.get("schema", {})
            default = ""
            source = descriptor if "default" in descriptor else schema
            if isinstance(source, dict) and "default" in source:
                default = f"<code>{esc(dumps(source['default']))}</code>"
            description = descriptor.get("description") \
                or (schema.get("description", "")
                    if isinstance(schema, dict) else "")
            rows.append(
                "<tr>"
                f"<td><code>{esc(descriptor['name'])}</code></td>"
                f"<td>{self.render_inline(schema, depth)}</td>"
                f"<td>{'Yes' if descriptor.get('required') else 'No'}</td>"
                f"<td>{default}</td>"
                f"<td>{esc(description)}</td>"
                "</tr>")
        return (
            '<div class="tablewrap"><table>'
            "<thead><tr><th>Name</th><th>Type</th><th>Required</th>"
            "<th>Default</th><th>Description</th></tr></thead>"
            f"<tbody>{''.join(rows)}</tbody>"
            "</table></div>")

    def raw_schema_details(self, entity):
        return (f"<details><summary>Schema</summary>{pre_json(entity)}"
                "</details>")

    def index_page(self, relative, title, entities, intro):
        """Namespace-grouped index for methods/notifications/types."""
        groups = {}
        for name in entities:
            groups.setdefault(kodi_schema.namespace_of(name), []).append(name)
        toc = " ".join(f'<a href="#{esc(ns)}">{esc(ns)}</a>'
                       for ns in groups)
        sections = []
        for ns, names in groups.items():
            rows = []
            for name in names:
                entity = entities[name]
                description = entity.get("description", "")
                badge = ""
                if entity.get("x-kodi-runtime-enum"):
                    badge = ' <span class="badge">runtime enum</span>'
                rows.append(
                    "<tr>"
                    f'<td><a href="{esc(name)}.html">{esc(name)}</a>'
                    f"{badge}</td>"
                    f"<td>{esc(description)}</td>"
                    "</tr>")
            sections.append(
                f'<section id="{esc(ns)}"><h2>{esc(ns)}</h2>'
                '<div class="tablewrap"><table>'
                f"<tbody>{''.join(rows)}</tbody></table></div></section>")
        body = (f"<h1>{esc(title)}</h1>{intro}"
                f'<p class="toc">{toc}</p>{"".join(sections)}')
        self.page(relative, f"{title} - Kodi JSON-RPC API", body)

    # ------------------------------------------------------------------
    # pages

    def build_method_page(self, name, method):
        depth = 2
        ns = kodi_schema.namespace_of(name)
        parts = [
            f'<p class="crumbs"><a href="index.html#{esc(ns)}">{esc(ns)}</a>'
            "</p>",
            f"<h1>{esc(name)}</h1>",
            f"<p>{esc(method['description'])}</p>",
        ]
        badges = [f'<span class="badge">Permission: '
                  f"{esc(method['permission'])}</span>"]
        badges.extend(f'<span class="badge">Transport: {esc(label)}</span>'
                      for label in transports(method))
        for key, value in method.items():
            if key.startswith("x-kodi-"):
                badges.append(f'<span class="badge">{esc(key[7:])}: '
                              f"{esc(dumps(value))}</span>")
        parts.append(f'<p class="meta">{" ".join(badges)}</p>')
        example = next((entry for entry in self.examples
                        if entry.get("method") == name), None)
        parts.append("<h2>Request</h2>")
        if example:
            parts.append(f'<p class="muted">{esc(example["title"])}</p>')
            parts.append(pre_json(example["request"]))
            parts.append("<h2>Response</h2>")
            parts.append(pre_json(example["response"]))
        else:
            parts.append(pre_json(self.envelope_skeleton(name, method)))
        params = method.get("params", [])
        if params:
            parts.append("<h2>Parameters</h2>")
            parts.append(self.params_table(params, depth))
        parts.append("<h2>Returns</h2>")
        parts.append(self.render_block(
            kodi_schema.returns_schema(method["returns"]), depth))
        parts.append("<h2>Errors</h2>")
        parts.append(
            '<p>Any method can return the <a href="../errors.html">standard '
            "errors</a>; <code>-32602</code> (Invalid params) carries "
            "structured <code>error.data</code> naming the offending "
            "parameter.</p>")
        parts.append(self.raw_schema_details(method))
        self.page(f"{self.vdir}/methods/{name}.html",
                  f"{name} - Kodi JSON-RPC API", "".join(parts))

    def build_notification_page(self, name, notification):
        depth = 2
        ns = kodi_schema.namespace_of(name)
        parts = [
            f'<p class="crumbs"><a href="index.html#{esc(ns)}">{esc(ns)}</a>'
            "</p>",
            f"<h1>{esc(name)}</h1>",
            f"<p>{esc(notification['description'])}</p>",
            '<p class="meta"><span class="badge">Transport: WebSocket / TCP '
            "only</span></p>",
            "<p>Server-initiated push message; it is never delivered over "
            "HTTP and carries no <code>id</code> member.</p>",
        ]
        example = next((entry for entry in self.examples
                        if entry.get("notification") == name), None)
        parts.append("<h2>Message</h2>")
        if example:
            parts.append(f'<p class="muted">{esc(example["title"])}</p>')
            parts.append(pre_json(example["message"]))
        else:
            parts.append(pre_json(
                self.envelope_skeleton(name, notification,
                                       notification=True)))
        params = notification.get("params", [])
        if params:
            parts.append("<h2>Parameters</h2>")
            parts.append(self.params_table(params, depth))
        parts.append(self.raw_schema_details(notification))
        self.page(f"{self.vdir}/notifications/{name}.html",
                  f"{name} - Kodi JSON-RPC API", "".join(parts))

    def build_type_page(self, name, schema):
        depth = 2
        ns = kodi_schema.namespace_of(name)
        parts = [
            f'<p class="crumbs"><a href="index.html#{esc(ns)}">{esc(ns)}</a>'
            "</p>",
            f"<h1>{esc(name)}</h1>",
        ]
        if schema.get("x-kodi-runtime-enum"):
            parts.append('<p class="meta"><span class="badge">runtime enum'
                         "</span></p>")
            parts.append(
                "<p>The values of this enumeration are registered at "
                "runtime by the running Kodi instance and are not part of "
                "the static schema; the live value list is enumerated via "
                '<a href="../methods/JSONRPC.Introspect.html">'
                "JSONRPC.Introspect</a>.</p>")
        else:
            if schema.get("description"):
                parts.append(f"<p>{esc(schema['description'])}</p>")
            body_schema = {key: value for key, value in schema.items()
                           if key != "description"}
            parts.append(self.render_block(body_schema, depth))
        referrers = self.reverse_refs.get(name, [])
        if referrers:
            parts.append("<h2>Referenced by</h2>")
            items = []
            for kind, referrer in referrers:
                folder = {"method": "methods",
                          "notification": "notifications",
                          "type": "types"}[kind]
                items.append(
                    f'<li><a href="../{folder}/{esc(referrer)}.html">'
                    f"{esc(referrer)}</a> "
                    f'<span class="muted">({kind})</span></li>')
            parts.append(f'<ul class="refs">{"".join(items)}</ul>')
        parts.append(self.raw_schema_details(schema))
        self.page(f"{self.vdir}/types/{name}.html",
                  f"{name} - Kodi JSON-RPC API", "".join(parts))

    def build_errors_page(self):
        rows = []
        for error in self.taxonomy:
            rows.append(
                "<tr>"
                f"<td><code>{error['code']}</code></td>"
                f"<td>{esc(error['name'])}</td>"
                f"<td>{esc(error['message'])}</td>"
                f"<td>{esc(error['description'])}</td>"
                f"<td>{'Yes' if error['has_data'] else 'No'}</td>"
                "</tr>")
        body = (
            "<h1>Errors</h1>"
            "<p>Every method can fail with any of these errors; the error "
            "object is returned in the <code>error</code> member of the "
            "response envelope with the listed <code>code</code> and "
            "<code>message</code>.</p>"
            '<div class="tablewrap"><table>'
            "<thead><tr><th>Code</th><th>Name</th><th>Message</th>"
            "<th>Description</th><th><code>error.data</code> populated</th>"
            "</tr></thead>"
            f"<tbody>{''.join(rows)}</tbody></table></div>")
        self.page(f"{self.vdir}/errors.html",
                  "Errors - Kodi JSON-RPC API", body)

    def build_landing_page(self):
        v = self.vdir
        parts = [
            "<h1>Kodi JSON-RPC API</h1>",
            f'<p class="meta"><span class="badge">schema version '
            f"{esc(self.version)}</span></p>",
            "<p>Reference documentation for the JSON-RPC API exposed by "
            "<a href=\"https://kodi.tv\">Kodi</a>, the open source media "
            "center. It covers every request/response method, every "
            "server-initiated notification, every schema type and the "
            "error taxonomy, and is generated directly from the "
            "machine-readable schema shipped inside Kodi itself.</p>",

            "<h2>How the API actually works</h2>",
            "<p>The API is <a href=\"https://www.jsonrpc.org/specification\">"
            "JSON-RPC 2.0</a> served over three transports:</p>",
            "<ul>"
            "<li><strong>HTTP</strong> - POST the request envelope to "
            "<code>/jsonrpc</code> (default port 8080). Request/response "
            "only; notifications are never delivered over HTTP. Requires "
            "the 'Allow remote control via HTTP' setting to be enabled.</li>"
            "<li><strong>WebSocket</strong> - default port 9090; supports "
            "request/response and server-initiated notifications.</li>"
            "<li><strong>Raw TCP</strong> - port 9090; a newline-free "
            "stream of JSON objects over a plain socket; supports "
            "request/response and notifications.</li>"
            "</ul>",
            "<p>There is a single endpoint: the method is selected by the "
            "<code>method</code> member of the request envelope, not by the "
            "URL. Authentication is HTTP basic auth when a username and "
            "password are configured. A permissions model gates the "
            "methods; each method page states the permission it "
            "requires.</p>",

            "<h2>Worked examples</h2>",
            "<p>Each example shows the exact envelopes on the wire. The "
            "curl command targets the HTTP transport; the same request "
            "envelope works over WebSocket and raw TCP verbatim.</p>",
        ]
        for example in self.examples:
            if "method" in example:
                name = example["method"]
                parts.append(
                    f"<h3>{esc(example['title'])}</h3>"
                    f'<p class="muted"><a href="{v}/methods/{esc(name)}'
                    f'.html">{esc(name)}</a></p>')
                compact = json.dumps(example["request"],
                                     separators=(",", ":"),
                                     ensure_ascii=False)
                curl = ("curl -X POST http://localhost:8080/jsonrpc "
                        "-H 'content-type: application/json' "
                        f"-d '{compact}'")
                parts.append(f"<pre><code>{esc(curl)}</code></pre>")
                parts.append(pre_json(example["request"]))
                parts.append(pre_json(example["response"]))
            else:
                name = example["notification"]
                parts.append(
                    f"<h3>{esc(example['title'])}</h3>"
                    f'<p class="muted"><a href="{v}/notifications/'
                    f'{esc(name)}.html">{esc(name)}</a> - pushed over '
                    "WebSocket/TCP only</p>")
                parts.append(pre_json(example["message"]))
        parts.extend([
            "<h2>About this documentation</h2>",
            "<p>This site is generated from the machine-readable schema "
            "shipped inside Kodi "
            "(<code>xbmc/interfaces/json-rpc/schema</code>) on every "
            "change, so it cannot drift from the implementation. The same "
            "schema is served live by a running Kodi instance via the "
            f'<a href="{v}/methods/JSONRPC.Introspect.html">'
            "JSONRPC.Introspect</a> method.</p>",
            "<p>Machine-readable artifacts:</p>",
            "<ul>"
            f'<li><a href="{v}/openrpc.json">openrpc.json</a> - '
            '<a href="https://open-rpc.org/">OpenRPC</a> document covering '
            "all request/response methods</li>"
            f'<li><a href="{v}/asyncapi.json">asyncapi.json</a> - '
            '<a href="https://www.asyncapi.com/">AsyncAPI</a> document '
            "covering the notifications</li>"
            "</ul>",

            "<h2>Reference</h2>",
            "<ul>"
            f'<li><a href="{v}/methods/index.html">Methods</a> - '
            f"{len(self.service['methods'])} request/response methods</li>"
            f'<li><a href="{v}/notifications/index.html">Notifications</a> '
            f"- {len(self.service['notifications'])} server-initiated "
            "notifications</li>"
            f'<li><a href="{v}/types/index.html">Types</a> - '
            f"{len(self.service['types'])} schema types</li>"
            f'<li><a href="{v}/errors.html">Errors</a> - the error '
            "taxonomy</li>"
            "</ul>",
        ])
        self.page("index.html", "Kodi JSON-RPC API", "".join(parts))

    # ------------------------------------------------------------------

    def build(self):
        self.write("style.css", STYLESHEET)
        self.write(".nojekyll", "")
        for artifact in ("openrpc.json", "asyncapi.json"):
            source = DOCS_DIR / artifact
            target = self.out / self.vdir / artifact
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
            self.written.append(f"{self.vdir}/{artifact}")
        self.build_landing_page()
        methods = self.service["methods"]
        self.index_page(
            f"{self.vdir}/methods/index.html", "Methods", methods,
            f"<p>{len(methods)} request/response methods.</p>")
        for name, method in methods.items():
            self.build_method_page(name, method)
        notifications = self.service["notifications"]
        self.index_page(
            f"{self.vdir}/notifications/index.html", "Notifications",
            notifications,
            f"<p>{len(notifications)} server-initiated notifications, "
            "delivered over the WebSocket and raw TCP transports only.</p>")
        for name, notification in notifications.items():
            self.build_notification_page(name, notification)
        types = self.service["types"]
        self.index_page(
            f"{self.vdir}/types/index.html", "Types", types,
            f"<p>{len(types)} schema types.</p>")
        for name, schema in types.items():
            self.build_type_page(name, schema)
        self.build_errors_page()
        return self.written


STYLESHEET = """\
:root {
  --bg: #ffffff;
  --fg: #1c2731;
  --muted: #5b6b78;
  --surface: #f2f7fa;
  --border: #d9e4eb;
  --accent: #0d84b4;
  --accent-soft: #d5eefa;
  --badge-fg: #0a5d80;
}
@media (prefers-color-scheme: dark) {
  :root {
    --bg: #121a20;
    --fg: #dbe6ee;
    --muted: #90a3b1;
    --surface: #1a2530;
    --border: #2b3b48;
    --accent: #3ec6f2;
    --accent-soft: #0f3d51;
    --badge-fg: #9edff8;
  }
}
* {
  box-sizing: border-box;
}
body {
  margin: 0;
  background: var(--bg);
  color: var(--fg);
  font-family: system-ui, -apple-system, "Segoe UI", Roboto, "Helvetica Neue",
    sans-serif;
  line-height: 1.6;
}
header {
  border-bottom: 1px solid var(--border);
  background: var(--surface);
}
header .inner {
  max-width: 72rem;
  margin: 0 auto;
  padding: 0.6rem 1.5rem;
  display: flex;
  align-items: baseline;
  gap: 0.75rem;
  flex-wrap: wrap;
}
header .site {
  font-weight: 700;
  color: var(--fg);
}
header nav {
  margin-left: auto;
  display: flex;
  gap: 1rem;
}
main {
  max-width: 72rem;
  margin: 0 auto;
  padding: 1rem 1.5rem 4rem;
}
a {
  color: var(--accent);
  text-decoration: none;
}
a:hover {
  text-decoration: underline;
}
h1 {
  font-size: 1.7rem;
  margin: 1rem 0 0.5rem;
}
h2 {
  font-size: 1.25rem;
  margin: 2rem 0 0.5rem;
  border-bottom: 1px solid var(--border);
  padding-bottom: 0.25rem;
}
h3 {
  font-size: 1.05rem;
  margin: 1.5rem 0 0.25rem;
}
.crumbs {
  margin: 1rem 0 -0.5rem;
  font-size: 0.9rem;
}
.badge {
  display: inline-block;
  padding: 0.05rem 0.6rem;
  border-radius: 999px;
  background: var(--accent-soft);
  color: var(--badge-fg);
  font-size: 0.8rem;
  white-space: nowrap;
}
.meta {
  margin: 0.25rem 0 1rem;
}
.muted,
.constraints,
.props,
.enum {
  color: var(--muted);
  font-size: 0.9em;
}
.toc {
  font-size: 0.9rem;
}
.toc a {
  margin-right: 0.6rem;
  white-space: nowrap;
}
pre {
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: 6px;
  padding: 0.75rem 1rem;
  overflow-x: auto;
}
code {
  font-family: ui-monospace, "Cascadia Code", Consolas, Menlo, monospace;
  font-size: 0.9em;
}
.tablewrap {
  overflow-x: auto;
  margin: 0.5rem 0 1rem;
}
table {
  border-collapse: collapse;
  width: 100%;
}
th,
td {
  text-align: left;
  vertical-align: top;
  padding: 0.4rem 0.75rem;
  border-bottom: 1px solid var(--border);
}
th {
  color: var(--muted);
  font-size: 0.85rem;
}
details {
  border-left: 3px solid var(--accent);
  background: var(--surface);
  border-radius: 0 6px 6px 0;
  padding: 0.4rem 1rem;
  margin: 1.5rem 0;
}
summary {
  cursor: pointer;
  font-weight: 600;
}
ul.branches,
ol.branches {
  margin: 0.25rem 0;
  padding-left: 1.5rem;
}
ol.branches > li {
  margin: 0.5rem 0;
}
ul.enum-list {
  columns: 3 14rem;
  padding-left: 1.5rem;
  margin: 0.5rem 0;
}
ul.refs {
  columns: 2 22rem;
  padding-left: 1.5rem;
}
section {
  scroll-margin-top: 1rem;
}
"""


def generate(out_dir):
    """Generate the whole site into out_dir; returns the written paths."""
    return SiteBuilder(out_dir).build()


def ensure_gitignore():
    if not GITIGNORE_PATH.exists():
        GITIGNORE_PATH.write_text("/site/\n", encoding="utf-8", newline="\n")


def main():
    parser = argparse.ArgumentParser(
        description="Generate the static JSON-RPC documentation site.")
    parser.add_argument("--out", default=str(DEFAULT_OUT),
                        help="output directory (default: docs/jsonrpc/site)")
    arguments = parser.parse_args()
    ensure_gitignore()
    written = generate(arguments.out)
    print(f"wrote {len(written)} files to {arguments.out}")


if __name__ == "__main__":
    main()
