#!/usr/bin/env python3
#
#  Copyright (C) 2026 Team Kodi
#  This file is part of Kodi - https://kodi.tv
#
#  SPDX-License-Identifier: GPL-2.0-or-later
#  See LICENSES/README.md for more information.
#
"""Smoke tests for the static documentation site generator.

Generates the site into a temporary directory and checks the page
inventory, that every internal link resolves to an emitted file, that the
machine-readable artifacts are byte-identical copies of the docs/jsonrpc
originals, and that no href/src attribute uses an absolute path.
"""

import json
import posixpath
import sys
import tempfile
import unittest
from html.parser import HTMLParser
from pathlib import Path

TOOLS_DIR = Path(__file__).resolve().parent
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

import generate_site
import kodi_schema


class LinkCollector(HTMLParser):

    def __init__(self):
        super().__init__()
        self.links = []

    def handle_starttag(self, tag, attrs):
        for key, value in attrs:
            if key in ("href", "src") and value is not None:
                self.links.append(value)


def is_external(link):
    return link.startswith(("http://", "https://", "mailto:"))


class TestSiteGeneration(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.tempdir = tempfile.TemporaryDirectory()
        cls.out = Path(cls.tempdir.name) / "site"
        cls.written = generate_site.generate(cls.out)
        cls.vdir = "v" + kodi_schema.load_version().split(".")[0]
        cls.service = kodi_schema.load_service()

    @classmethod
    def tearDownClass(cls):
        cls.tempdir.cleanup()

    def html_names(self, folder):
        directory = self.out / self.vdir / folder
        return {path.name for path in directory.glob("*.html")}

    def test_method_page_count(self):
        names = self.html_names("methods")
        self.assertIn("index.html", names)
        self.assertEqual(len(names - {"index.html"}), len(self.service["methods"]))

    def test_notification_page_count(self):
        names = self.html_names("notifications")
        self.assertIn("index.html", names)
        self.assertEqual(
            len(names - {"index.html"}), len(self.service["notifications"]))

    def test_type_page_count(self):
        names = self.html_names("types")
        self.assertIn("index.html", names)
        self.assertEqual(len(names - {"index.html"}), len(self.service["types"]))

    def test_core_files_exist(self):
        for relative in ("index.html", "style.css", ".nojekyll",
                         f"{self.vdir}/errors.html",
                         f"{self.vdir}/openrpc.json",
                         f"{self.vdir}/asyncapi.json"):
            with self.subTest(relative=relative):
                self.assertTrue((self.out / relative).is_file())

    def test_artifact_copies_are_byte_identical(self):
        for artifact in ("openrpc.json", "asyncapi.json"):
            with self.subTest(artifact=artifact):
                original = (generate_site.DOCS_DIR / artifact).read_bytes()
                copy = (self.out / self.vdir / artifact).read_bytes()
                self.assertEqual(original, copy)

    def test_internal_links_resolve_and_are_relative(self):
        pages = sorted(self.out.rglob("*.html"))
        self.assertTrue(pages)
        for page in pages:
            relative = page.relative_to(self.out).as_posix()
            collector = LinkCollector()
            collector.feed(page.read_text(encoding="utf-8"))
            for link in collector.links:
                if is_external(link):
                    continue
                with self.subTest(page=relative, link=link):
                    self.assertFalse(link.startswith("/"),
                                     "absolute path in href/src")
                    self.assertNotIn(":", link.split("/")[0].split("#")[0],
                                     "unexpected scheme or drive letter")
                    target = link.split("#", 1)[0]
                    if not target:
                        continue
                    resolved = posixpath.normpath(
                        posixpath.join(posixpath.dirname(relative), target))
                    self.assertFalse(resolved.startswith(".."),
                                     "link escapes the site root")
                    self.assertTrue((self.out / resolved).is_file(),
                                    f"broken link to {resolved}")

    def test_runtime_enum_pages_mention_introspect(self):
        service = kodi_schema.load_service()
        placeholders = [name for name, schema in service["types"].items()
                        if schema.get("x-kodi-runtime-enum")]
        # not an exact count: the guard is that the flag still selects
        # something, so that a renamed key cannot make the loop vacuous
        self.assertGreater(len(placeholders), 0)
        for name in placeholders:
            with self.subTest(type=name):
                text = (self.out / self.vdir / "types"
                        / f"{name}.html").read_text(encoding="utf-8")
                self.assertIn("JSONRPC.Introspect", text)
                self.assertIn("runtime", text)

    def test_landing_page_names_every_runtime_enum(self):
        """The runtime-enum list is the reason to call Introspect at all, so
        it is derived from the schema rather than written out."""
        text = (self.out / "index.html").read_text(encoding="utf-8")
        service = kodi_schema.load_service()
        placeholders = [name for name, schema in service["types"].items()
                        if schema.get("x-kodi-runtime-enum")]
        self.assertGreater(len(placeholders), 0)
        self.assertIn(f"{len(placeholders)} types carry no values", text)
        for name in placeholders:
            with self.subTest(type=name):
                self.assertIn(f"<code>{generate_site.esc(name)}</code>", text)

    def test_landing_page_explains_introspect_against_the_artifacts(self):
        text = (self.out / "index.html").read_text(encoding="utf-8")
        self.assertIn("Discovering the API at runtime", text)
        # the three things only the live call can answer
        for claim in ("runtime enumeration", "What your connection may call",
                      "Which version you are talking to"):
            with self.subTest(claim=claim):
                self.assertIn(claim, text)

    def test_landing_page_renders_every_example(self):
        text = (self.out / "index.html").read_text(encoding="utf-8")
        examples = sorted(generate_site.EXAMPLES_DIR.glob("*.json"))
        self.assertEqual(len(examples), 7)
        curl_count = text.count("curl -X POST http://localhost:8080/jsonrpc")
        method_examples = 0
        for path in examples:
            with open(path, encoding="utf-8") as handle:
                example = json.load(handle)
            with self.subTest(example=path.name):
                self.assertIn(generate_site.esc(example["title"]), text)
            if "method" in example:
                method_examples += 1
        self.assertEqual(curl_count, method_examples)

    def test_errors_page_lists_whole_taxonomy(self):
        text = (self.out / self.vdir
                / "errors.html").read_text(encoding="utf-8")
        for error in kodi_schema.load_error_taxonomy():
            with self.subTest(error=error["name"]):
                self.assertIn(f"<code>{error['code']}</code>", text)
                self.assertIn(generate_site.esc(error["name"]), text)

    def test_output_is_deterministic(self):
        with tempfile.TemporaryDirectory() as second:
            again = Path(second) / "site"
            generate_site.generate(again)
            first_files = sorted(path.relative_to(self.out).as_posix()
                                 for path in self.out.rglob("*")
                                 if path.is_file())
            second_files = sorted(path.relative_to(again).as_posix()
                                  for path in again.rglob("*")
                                  if path.is_file())
            self.assertEqual(first_files, second_files)
            for relative in first_files:
                with self.subTest(file=relative):
                    self.assertEqual((self.out / relative).read_bytes(),
                                     (again / relative).read_bytes())


if __name__ == "__main__":
    unittest.main()
