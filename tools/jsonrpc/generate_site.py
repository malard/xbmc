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
import re
import shutil
from pathlib import Path

import kodi_schema

DOCS_DIR = kodi_schema.REPO_ROOT / "docs" / "jsonrpc"
DEFAULT_OUT = DOCS_DIR / "site"
EXAMPLES_DIR = Path(__file__).resolve().parent / "examples"
STYLESHEET_PATH = Path(__file__).resolve().parent / "site.css"
GITIGNORE_PATH = DOCS_DIR / ".gitignore"

DEFS_PREFIX = "#/$defs/"

#! Hand-written documents in docs/jsonrpc, rendered into the site as pages
PROSE_DOCUMENTS = {
    "MIGRATING-v13-to-v14.md": "Migrating from 13 to 14",
    "CHANGELOG.md": "Changelog",
}

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


_MD_INLINE_CODE = re.compile(r"`([^`]+)`")
_MD_BOLD = re.compile(r"\*\*([^*]+)\*\*")
_MD_LINK = re.compile(r"\[([^\]]+)\]\(([^)]+)\)")


def md_inline(text):
    """Inline markdown: code spans, bold, links.

    Code spans are lifted out first so their content is never read as
    markup, but they are replaced by a placeholder rather than rendered in
    place: bold and links have to see the whole line, or emphasis wrapping
    a code span is left with its markers in two different pieces.
    """
    spans = []

    def lift(match):
        spans.append(match.group(1))
        return f"\x00{len(spans) - 1}\x00"

    lifted = _MD_INLINE_CODE.sub(lift, text)

    rendered = esc(lifted)
    rendered = _MD_BOLD.sub(r"<strong>\1</strong>", rendered)
    rendered = _MD_LINK.sub(r'<a href="\2">\1</a>', rendered)

    for index, span in enumerate(spans):
        rendered = rendered.replace(f"\x00{index}\x00",
                                    f"<code>{esc(span)}</code>")
    return rendered


def md_to_html(source, link_rewrite=None):
    """Render the markdown subset the JSON-RPC documents use.

    Headings, paragraphs, bullet lists, fenced code, tables, horizontal
    rules and the inline forms above. Anything else is emitted as a
    paragraph, which is the failure mode that loses the least.
    """
    if link_rewrite:
        for before, after in link_rewrite.items():
            source = source.replace(f"]({before})", f"]({after})")

    parts = []
    lines = source.splitlines()
    index = 0
    while index < len(lines):
        line = lines[index]

        if line.startswith("```"):
            index += 1
            block = []
            while index < len(lines) and not lines[index].startswith("```"):
                block.append(lines[index])
                index += 1
            index += 1
            parts.append(f"<pre><code>{esc(chr(10).join(block))}</code></pre>")
            continue

        if line.startswith("#"):
            level = len(line) - len(line.lstrip("#"))
            parts.append(f"<h{level}>{md_inline(line[level:].strip())}"
                         f"</h{level}>")
            index += 1
            continue

        if line.strip() in ("---", "***", "___"):
            parts.append("<hr>")
            index += 1
            continue

        if line.startswith("|"):
            rows = []
            while index < len(lines) and lines[index].startswith("|"):
                rows.append(lines[index])
                index += 1
            parts.append(_md_table(rows))
            continue

        if line.lstrip().startswith("- "):
            items = []
            while index < len(lines) and (lines[index].lstrip().startswith("- ")
                                          or (lines[index].startswith("  ")
                                              and lines[index].strip()
                                              and items)):
                current = lines[index]
                if current.lstrip().startswith("- "):
                    items.append(current.lstrip()[2:])
                else:
                    # a wrapped continuation of the item above
                    items[-1] += " " + current.strip()
                index += 1
            body = "".join(f"<li>{md_inline(item)}</li>" for item in items)
            parts.append(f"<ul>{body}</ul>")
            continue

        if not line.strip():
            index += 1
            continue

        paragraph = []
        while index < len(lines) and lines[index].strip() \
                and not lines[index].startswith(("#", "|", "```")) \
                and not lines[index].lstrip().startswith("- "):
            paragraph.append(lines[index].strip())
            index += 1
        parts.append(f"<p>{md_inline(' '.join(paragraph))}</p>")

    return "".join(parts)


def _md_table(rows):
    cells = [[cell.strip() for cell in row.strip().strip("|").split("|")]
             for row in rows]
    # the second row of a markdown table is the alignment rule, not data
    body = cells[2:] if len(cells) > 1 and set("-: |") >= set(rows[1]) else \
        cells[1:]
    head = "".join(f"<th>{md_inline(cell)}</th>" for cell in cells[0])
    out = [f"<thead><tr>{head}</tr></thead><tbody>"]
    for row in body:
        out.append("<tr>"
                   + "".join(f"<td>{md_inline(cell)}</td>" for cell in row)
                   + "</tr>")
    out.append("</tbody>")
    return f'<div class="tablewrap"><table>{"".join(out)}</table></div>'


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
            if prop.get("deprecated"):
                description = ('<span class="badge warn">deprecated</span> '
                               + description)
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
                if entity.get("deprecated"):
                    badge += ' <span class="badge warn">deprecated</span>'
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
        if method.get("deprecated"):
            parts.append('<p class="deprecated"><strong>Deprecated.</strong> '
                         "Still served, but it will be removed in the next "
                         "major version of the API. The description says what "
                         "to use instead.</p>")
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

    def build_prose_page(self, source, title):
        """Render a hand-written document from docs/jsonrpc into the site.

        The markdown file is the source of truth: it is what a reviewer reads
        in the pull request and what GitHub renders. Links between the two
        documents are rewritten to the pages they become here.
        """
        text = (DOCS_DIR / source).read_text(encoding="utf-8")
        rewrite = {name: name[:-len(".md")] + ".html"
                   for name in PROSE_DOCUMENTS}
        body = md_to_html(text, link_rewrite=rewrite)
        self.page(f"{self.vdir}/{source[:-len('.md')]}.html",
                  f"{title} - Kodi JSON-RPC API", body)

    def build_landing_page(self):
        v = self.vdir
        # Counted from the schema rather than stated, so that adding or
        # removing a runtime enum cannot leave the prose behind saying
        # something that was true once
        runtime_enums = sorted(name for name, schema
                               in self.service["types"].items()
                               if schema.get("x-kodi-runtime-enum"))
        runtime_enum_count = len(runtime_enums)
        runtime_enum_names = ", ".join(f"<code>{esc(name)}</code>"
                                       for name in runtime_enums)
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
            "<li><strong>WebSocket</strong> - default port 9090; carries "
            "request/response and server-initiated notifications.</li>"
            "<li><strong>Raw TCP</strong> - port 9090; a newline-free "
            "stream of JSON objects over a plain socket, carrying the same "
            "two.</li>"
            "<li><strong>HTTP</strong> - POST the request envelope to "
            "<code>/jsonrpc</code> (default port 8080). Request/response "
            "only: notifications are never delivered over HTTP, so a client "
            "on this transport can only learn that something happened by "
            "asking again. Requires the 'Allow remote control via HTTP' "
            "setting to be enabled.</li>"
            "</ul>",
            "<p><strong>Which to use.</strong> Prefer a socket transport for "
            "anything that outlives a single call. Kodi announces what it is "
            "doing - playback starting, a library item changing, the skin "
            "finishing its load - and a client holding a socket open is told, "
            "while a client on HTTP has to poll for the same thing and will "
            "miss anything that starts and finishes between two polls. HTTP "
            "suits one-shot calls and environments where keeping a connection "
            "open is impractical.</p>",
            "<p>Preferring a socket does not mean turning the web server off. "
            "It also serves the artwork and file endpoints "
            "(<code>/image/</code>, <code>/vfs/</code>) that several methods "
            "hand back URLs for, so a client doing its calls over a socket "
            "still fetches those over HTTP.</p>",
            "<p>There is a single endpoint: the method is selected by the "
            "<code>method</code> member of the request envelope, not by the "
            "URL. Authentication over HTTP is basic auth when a username and "
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
            "<h2>Discovering the API at runtime</h2>",
            "<p>This site and the artifacts below describe a release. "
            f'<a href="{v}/methods/JSONRPC.Introspect.html">'
            "JSONRPC.Introspect</a> describes the instance you are connected "
            "to. Call it for three things.</p>",

            "<p><strong>The values of a runtime enumeration.</strong> "
            f"{runtime_enum_count} types carry no values here "
            f"({runtime_enum_names}); the running instance holds them. Read "
            "them from Introspect to build a filter, or to activate a window "
            "by name.</p>",

            "<p><strong>What your connection may call.</strong> Introspect "
            "reports the methods your permissions and your transport allow, "
            "not every method that exists. A method missing from the answer "
            f'returns <a href="{v}/errors.html">MethodNotFound</a> if you '
            "call it anyway, as does one that does not exist at all and one "
            "that is not served over the transport you used.</p>",

            "<p><strong>Which version you are talking to.</strong> Call "
            "<code>JSONRPC.Version</code>, then Introspect if you need the "
            "shape as well as the number. Do this before assuming any "
            "behaviour described here.</p>",

            "<p>For code generation, offline tooling and comparing one "
            "release against another, use the artifacts below instead.</p>",

            "<p>Introspect answers with the whole description by default, "
            "which is large. Narrow it:</p>",
            pre_json({
                "jsonrpc": "2.0",
                "id": 1,
                "method": "JSONRPC.Introspect",
                "params": {
                    "filter": {"type": "type", "id": "GUI.Window"},
                    "getdescriptions": False,
                },
            }),
            "<p><code>filter.type</code> accepts <code>method</code>, "
            "<code>namespace</code>, <code>type</code>, "
            "<code>notification</code> and <code>error</code>. "
            "<code>getdescriptions</code> and <code>getmetadata</code> strip "
            "the documentation out of the answer; a method's "
            "<code>deprecated</code> note is reported either way.</p>",

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

            "<h2>Upgrading</h2>",
            f'<p class="deprecated"><strong>Version {esc(self.version)} is a '
            "breaking release.</strong> A client written against version 13 "
            "is not guaranteed to work unchanged. The migration guide lists "
            "every break and what to do about each.</p>",
            "<ul>"
            f'<li><a href="{v}/MIGRATING-v13-to-v14.html">Migrating from 13 '
            "to 14</a></li>"
            f'<li><a href="{v}/CHANGELOG.html">Changelog</a></li>'
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
        self.write("style.css", STYLESHEET_PATH.read_text(encoding="utf-8"))
        self.write(".nojekyll", "")
        for source, title in PROSE_DOCUMENTS.items():
            self.build_prose_page(source, title)
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
