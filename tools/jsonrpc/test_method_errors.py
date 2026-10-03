#!/usr/bin/env python3
#
#  Copyright (C) 2026 Team Kodi
#  This file is part of Kodi - https://kodi.tv
#
#  SPDX-License-Identifier: GPL-2.0-or-later
#  See LICENSES/README.md for more information.
#
"""Tests for the per-method error derivation.

The declarations in methods.json are checked against the real handler
sources; when they drift, run method_errors.py --write and commit the result.
"""

import tempfile
import textwrap
import unittest
from pathlib import Path

import kodi_schema
import method_errors


class TestDeclarations(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.methods = kodi_schema.load_service()["methods"]
        cls.taxonomy = {error["name"]
                        for error in kodi_schema.load_error_taxonomy()}
        cls.reasons = {reason["name"]: reason
                       for reason in kodi_schema.load_reason_taxonomy()}
        cls.derived = method_errors.derive()
        cls.derived_reasons = method_errors.derive_reasons()

    def test_pre_dispatch_names_are_in_the_taxonomy(self):
        self.assertTrue(set(method_errors.PRE_DISPATCH) <= self.taxonomy)

    def test_derivation_covers_every_method(self):
        self.assertEqual(set(self.derived), set(self.methods))

    def test_declarations_match_the_handlers(self):
        for name, errors in self.derived.items():
            with self.subTest(method=name):
                self.assertEqual(self.methods[name]["errors"], errors)

    def test_declared_reasons_match_the_handlers(self):
        for name, reasons in self.derived_reasons.items():
            with self.subTest(method=name):
                self.assertEqual(self.methods[name]["reasons"], reasons)

    def test_every_reason_is_used(self):
        used = {reason for method in self.methods.values()
                for reasons in method["reasons"].values() for reason in reasons}
        self.assertEqual(set(self.reasons), used)

    def test_an_unresolved_items_reason_is_one_playlist_add_can_fail_with(self):
        types = kodi_schema.load_service()["types"]
        declared = types["Playlist.UnresolvedItem"]["properties"]["reason"]["enum"]
        reasons = {reason for reasons in self.derived_reasons["Playlist.Add"].values()
                   for reason in reasons}
        self.assertEqual(set(declared), reasons)


class TestDerivation(unittest.TestCase):

    SOURCE = textwrap.dedent("""
        // return NotFound in a comment does not count
        JSONRPC_STATUS Helper(const CVariant& value)
        {
          // return AccessDenied in a comment does not count
          const char* text = "return Unavailable";
          return value.isNull() ? NotFound : OK;
        }

        JSONRPC_STATUS CBase::Inherited(const CVariant& value)
        {
          return value.isNull() ? BadPermission : OK;
        }

        JSONRPC_STATUS CTest::Inner(const CVariant& value)
        {
          if (!Helper(value))
            return InternalError;
          return OK;
        }

        JSONRPC_STATUS CTest::Outer(const std::string& method, ITransportLayer* transport,
                                    IClient* client, const CVariant& parameterObject,
                                    CVariant& result)
        {
          CDatabase database;
          if (!database.Open())
            return FailedToExecute;
          const JSONRPC_STATUS inherited = Inherited(parameterObject);
          if (inherited != OK)
            return inherited;
          return Inner(parameterObject);
        }

        JSONRPC_STATUS CTest::Open(const std::string& method, ITransportLayer* transport,
                                   IClient* client, const CVariant& parameterObject,
                                   CVariant& result)
        {
          return Unavailable;
        }

        JSONRPC_STATUS CTest::Declared(const CVariant& value);

        template<typename Getter>
        JSONRPC_STATUS Collect(const CVariant& value, const Getter& get)
        {
          return get(value);
        }

        JSONRPC_STATUS CTest::Forward(const CVariant& parameterObject, CVariant& result)
        {
          return Collect(parameterObject, Helper);
        }

        template<Kind K, Listing From>
        JSONRPC_STATUS CTest::List(const CVariant& parameterObject, CVariant& result)
        {
          return Inner(parameterObject);
        }

        JSONRPC_STATUS CTest::Refuse(const CVariant& parameterObject, CVariant& result)
        {
          // Reason::NotSeekable in a comment does not count
          return Fail(result, FailedToExecute, Reason::NothingPlaying);
        }

        JSONRPC_STATUS CTest::Delegate(const CVariant& parameterObject, CVariant& result)
        {
          if (parameterObject.isNull())
            return Fail(result, parameterObject.isArray() ? NotFound : Unavailable,
                        Reason::Unreachable, Target("path", parameterObject));
          return Refuse(parameterObject, result);
        }
        """)

    METHOD_MAP = textwrap.dedent("""
        JsonRpcMethodMap CJSONServiceDescription::m_methodMaps[] = {
          { "Test.Outer", CTest::Outer },
          { "Test.Open",  CTest::Open },
          { "Test.Forward", CTest::Forward },
          { "Test.Preset", CTest::List<Kind::Movie, Listing::RecentlyAdded> },
          { "Test.Refuse", CTest::Refuse },
          { "Test.Delegate", CTest::Delegate },
        };
        """)

    def setUp(self):
        self.tempdir = tempfile.TemporaryDirectory()
        source_dir = Path(self.tempdir.name)
        (source_dir / "Test.cpp").write_text(self.SOURCE, encoding="utf-8")
        (source_dir / method_errors.METHOD_MAP.name).write_text(
            self.METHOD_MAP, encoding="utf-8")
        self.derived = method_errors.derive(source_dir)
        self.derived_reasons = method_errors.derive_reasons(source_dir)

    def tearDown(self):
        self.tempdir.cleanup()

    def test_statuses_travel_through_called_functions(self):
        self.assertEqual(self.derived["Test.Outer"],
                         ["InternalError", "FailedToExecute", "BadPermission", "NotFound"])

    def test_a_function_passed_by_name_reaches_its_statuses(self):
        self.assertEqual(self.derived["Test.Forward"], ["NotFound"])

    def test_a_bare_call_reaches_an_inherited_static(self):
        self.assertIn("BadPermission", self.derived["Test.Outer"])

    def test_a_member_call_is_not_a_handler_call(self):
        # database.Open() must not pull in CTest::Open's Unavailable
        self.assertNotIn("Unavailable", self.derived["Test.Outer"])

    def test_a_template_handler_has_the_statuses_of_its_template(self):
        self.assertEqual(self.derived["Test.Preset"], ["InternalError", "NotFound"])

    def test_comments_and_strings_are_ignored(self):
        self.assertNotIn("AccessDenied", self.derived["Test.Outer"])
        self.assertNotIn("Unavailable", self.derived["Test.Outer"])
        self.assertEqual(self.derived["Test.Open"], ["Unavailable"])

    def test_a_reason_is_declared_under_the_status_its_call_names(self):
        self.assertEqual(self.derived_reasons["Test.Refuse"],
                         {"FailedToExecute": ["nothing-playing"]})
        self.assertEqual(self.derived["Test.Refuse"], ["FailedToExecute"])

    def test_a_reason_can_come_with_more_than_one_status(self):
        self.assertEqual(self.derived_reasons["Test.Delegate"],
                         {"FailedToExecute": ["nothing-playing"],
                          "NotFound": ["unreachable"],
                          "Unavailable": ["unreachable"]})
        self.assertEqual(self.derived["Test.Delegate"],
                         ["FailedToExecute", "NotFound", "Unavailable"])

    def test_a_method_naming_no_reason_derives_none(self):
        self.assertEqual(self.derived_reasons["Test.Outer"], {})

    def test_a_reason_outside_a_fail_call_is_an_error(self):
        source_dir = Path(self.tempdir.name)
        (source_dir / "Bad.cpp").write_text(textwrap.dedent("""
            JSONRPC_STATUS CBad::Refuse(const CVariant& parameterObject, CVariant& result)
            {
              const Reason reason = Reason::NothingPlaying;
              return Fail(result, FailedToExecute, reason);
            }
            """), encoding="utf-8")
        with self.assertRaises(ValueError):
            method_errors.derive(source_dir)

    def test_an_undescribed_reason_is_an_error(self):
        source_dir = Path(self.tempdir.name)
        (source_dir / "Bad.cpp").write_text(textwrap.dedent("""
            JSONRPC_STATUS CBad::Refuse(const CVariant& parameterObject, CVariant& result)
            {
              return Fail(result, NotFound, Reason::NoSuchReason);
            }
            """), encoding="utf-8")
        with self.assertRaises(ValueError):
            method_errors.derive(source_dir)


if __name__ == "__main__":
    unittest.main()
