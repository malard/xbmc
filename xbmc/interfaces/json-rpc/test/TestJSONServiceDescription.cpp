/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "JSONRPCTestUtils.h"

#include <string>

#include <gtest/gtest.h>

using namespace JSONRPC;

namespace
{

void ExpectVariantEq(const CVariant& expected, const CVariant& actual)
{
  EXPECT_TRUE(expected == actual) << "expected: " << ToJson(expected)
                                  << "\n  actual: " << ToJson(actual);
}

/*!
 Scenario helpers take the schema definitions as strings so that the same
 behavioural expectations can be asserted for more than one schema dialect.
 */

void CheckMissingRequiredParameter(JSONServiceDescriptionTestBase& fixture,
                                   const std::string& methodJson)
{
  ASSERT_TRUE(CJSONServiceDescription::AddMethod(methodJson, StubMethod));

  CVariant output;
  EXPECT_EQ(InvalidParams, fixture.Call("Test.Required", "{}", output));
  ExpectVariantEq(ParseJson(R"({
    "method": "Test.Required",
    "stack": { "name": "value", "type": "string", "message": "Missing parameter" }
  })"),
                  output);
}

void CheckObjectParameter(JSONServiceDescriptionTestBase& fixture, const std::string& methodJson)
{
  ASSERT_TRUE(CJSONServiceDescription::AddMethod(methodJson, StubMethod));

  // A missing required property names the property and its type in the error data
  CVariant output;
  EXPECT_EQ(InvalidParams, fixture.Call("Test.Object", R"({"opts": {}})", output));
  ExpectVariantEq(ParseJson(R"({
    "method": "Test.Object",
    "stack": {
      "name": "opts",
      "type": "object",
      "property": { "name": "path", "type": "string" },
      "message": "Missing property"
    }
  })"),
                  output);

  // A property of the wrong type carries the message on the property frame
  EXPECT_EQ(InvalidParams, fixture.Call("Test.Object", R"({"opts": {"path": 7}})", output));
  ExpectVariantEq(ParseJson(R"({
    "method": "Test.Object",
    "stack": {
      "name": "opts",
      "type": "object",
      "property": { "name": "path", "type": "string", "message": "Invalid type integer received" }
    }
  })"),
                  output);

  // Optional properties and parameters are filled from their defaults
  EXPECT_EQ(OK, fixture.Call("Test.Object", R"({"opts": {"path": "/x"}})", output));
  ExpectVariantEq(ParseJson(R"({ "opts": { "mode": "fast", "path": "/x" }, "speed": 5 })"), output);
}

void CheckUnionParameter(JSONServiceDescriptionTestBase& fixture, const std::string& methodJson)
{
  ASSERT_TRUE(CJSONServiceDescription::AddMethod(methodJson, StubMethod));

  CVariant output;
  EXPECT_EQ(OK, fixture.Call("Test.Union", R"({"target": {"movieid": 3}})", output));
  ExpectVariantEq(ParseJson(R"({ "target": { "movieid": 3 }, "when": null, "flag": null })"),
                  output);

  EXPECT_EQ(OK,
            fixture.Call("Test.Union",
                         R"({"target": {"songid": 7}, "when": "later", "flag": true})", output));
  ExpectVariantEq(ParseJson(R"({ "target": { "songid": 7 }, "when": "later", "flag": true })"),
                  output);

  // No union branch accepts the value: the error carries the OR'd type list
  EXPECT_EQ(InvalidParams, fixture.Call("Test.Union", R"({"target": {"foo": 1}})", output));
  ExpectVariantEq(ParseJson(R"({
    "method": "Test.Union",
    "stack": {
      "name": "target",
      "type": "object",
      "message": "Received value does not match any of the union type definitions"
    }
  })"),
                  output);

  // A value matching the type mask but no branch constraint fails the union check
  EXPECT_EQ(InvalidParams,
            fixture.Call("Test.Union", R"({"target": {"movieid": 3}, "when": "never"})", output));
  ExpectVariantEq(ParseJson(R"({
    "method": "Test.Union",
    "stack": {
      "name": "when",
      "type": ["null", "string"],
      "message": "Received value does not match any of the union type definitions"
    }
  })"),
                  output);

  // A value outside the OR'd type mask fails before any branch is tried
  EXPECT_EQ(InvalidParams,
            fixture.Call("Test.Union", R"({"target": {"movieid": 3}, "when": 5})", output));
  ExpectVariantEq(ParseJson(R"({
    "method": "Test.Union",
    "stack": {
      "name": "when",
      "type": ["null", "string"],
      "message": "Invalid type integer received"
    }
  })"),
                  output);

  // Pure string unions behave identically
  EXPECT_EQ(InvalidParams,
            fixture.Call("Test.Union", R"({"target": {"movieid": 3}, "flag": 1})", output));
  ExpectVariantEq(ParseJson(R"({
    "method": "Test.Union",
    "stack": {
      "name": "flag",
      "type": ["null", "boolean"],
      "message": "Invalid type integer received"
    }
  })"),
                  output);
}

void CheckExtendedType(JSONServiceDescriptionTestBase& fixture,
                       const std::string& baseJson,
                       const std::string& derivedJson,
                       const std::string& methodJson)
{
  ASSERT_TRUE(CJSONServiceDescription::AddType(baseJson));
  ASSERT_TRUE(CJSONServiceDescription::AddType(derivedJson));
  ASSERT_TRUE(CJSONServiceDescription::AddMethod(methodJson, StubMethod));
  CJSONServiceDescription::ResolveReferences();

  // The base type is validated first and fills the output first; the derived
  // type's defaults overwrite the base's for same-named optional properties
  CVariant output;
  EXPECT_EQ(OK, fixture.Call("Test.Extends", R"({"data": {"a": "x", "b": true}})", output));
  ExpectVariantEq(ParseJson(R"({ "data": { "a": "x", "b": true, "shared": 2 } })"), output);

  // A value failing the base type reports the extended type by name
  EXPECT_EQ(InvalidParams, fixture.Call("Test.Extends", R"({"data": {"b": true}})", output));
  ExpectVariantEq(ParseJson(R"({
    "method": "Test.Extends",
    "stack": {
      "name": "Base.A",
      "type": "object",
      "property": { "name": "a", "type": "string" },
      "message": "value does not match extended type Base.A"
    }
  })"),
                  output);
}

void CheckForwardReferences(JSONServiceDescriptionTestBase& fixture,
                            const std::string& baseJson,
                            const std::string& containerJson,
                            const std::string& methodJson)
{
  // The container references a type that arrives later: it parks in the
  // deferred queue, is replayed against the stub when the base appears, and
  // ResolveReferences re-copies the completed base into the reference site.
  // Methods are registered only after all types, which is the ordering the
  // initialization sequence guarantees. (Forward references via "extends" are
  // NOT healed this way; types.json is topologically ordered for extends.)
  EXPECT_FALSE(CJSONServiceDescription::AddType(containerJson));
  EXPECT_TRUE(CJSONServiceDescription::AddType(baseJson));
  CJSONServiceDescription::ResolveReferences();

  EXPECT_NE(nullptr, CJSONServiceDescription::GetType("C.Container"));
  EXPECT_NE(nullptr, CJSONServiceDescription::GetType("C.Base"));
  EXPECT_TRUE(CJSONServiceDescription::AddMethod(methodJson, StubMethod));

  CVariant output;
  EXPECT_EQ(OK, fixture.Call("Test.Forward", R"({"data": {"inner": {"x": 1}}})", output));
  ExpectVariantEq(ParseJson(R"({ "data": { "inner": { "x": 1, "y": 9 } } })"), output);
}

void CheckReferenceWithLocalDefault(JSONServiceDescriptionTestBase& fixture,
                                    const std::string& typeJson,
                                    const std::string& methodJson)
{
  ASSERT_TRUE(CJSONServiceDescription::AddType(typeJson));
  ASSERT_TRUE(CJSONServiceDescription::AddMethod(methodJson, StubMethod));
  CJSONServiceDescription::ResolveReferences();

  // The parameter's own default overrides the referenced type's default
  CVariant output;
  EXPECT_EQ(OK, fixture.Call("Test.Ref", "{}", output));
  ExpectVariantEq(ParseJson(R"({ "level": 7 })"), output);

  // Constraints of the referenced type still apply, message text included
  EXPECT_EQ(InvalidParams, fixture.Call("Test.Ref", R"({"level": 12})", output));
  ExpectVariantEq(ParseJson(R"({
    "method": "Test.Ref",
    "stack": {
      "name": "level",
      "type": "integer",
      "message": "Value between 0 (inclusive) and 10 (inclusive) expected but 12 received"
    }
  })"),
                  output);

  // Resolving references again must not change behaviour
  CJSONServiceDescription::ResolveReferences();
  CJSONServiceDescription::ResolveReferences();
  EXPECT_EQ(OK, fixture.Call("Test.Ref", "{}", output));
  ExpectVariantEq(ParseJson(R"({ "level": 7 })"), output);
}

void CheckEnumParameter(JSONServiceDescriptionTestBase& fixture, const std::string& methodJson)
{
  ASSERT_TRUE(CJSONServiceDescription::AddMethod(methodJson, StubMethod));

  // An enum type without an explicit default defaults to its first value
  CVariant output;
  EXPECT_EQ(OK, fixture.Call("Test.Enum", "{}", output));
  ExpectVariantEq(ParseJson(R"({ "mode": "one" })"), output);

  EXPECT_EQ(InvalidParams, fixture.Call("Test.Enum", R"({"mode": "three"})", output));
  ExpectVariantEq(ParseJson(R"({
    "method": "Test.Enum",
    "stack": {
      "name": "mode",
      "type": "string",
      "message": "Received value does not match any of the defined enum values"
    }
  })"),
                  output);
}

} // unnamed namespace

class TestJSONServiceDescription : public JSONServiceDescriptionTestBase
{
};

TEST_F(TestJSONServiceDescription, MissingRequiredParameterDraft03)
{
  CheckMissingRequiredParameter(*this, R"({"Test.Required": {
    "type": "method", "description": "test", "transport": "Response", "permission": "ReadData",
    "params": [ { "name": "value", "type": "string", "required": true } ],
    "returns": "string"
  }})");
}

TEST_F(TestJSONServiceDescription, ObjectParameterDraft03)
{
  CheckObjectParameter(*this, R"({"Test.Object": {
    "type": "method", "description": "test", "transport": "Response", "permission": "ReadData",
    "params": [
      { "name": "opts", "type": "object", "required": true,
        "properties": {
          "path": { "type": "string", "required": true },
          "mode": { "type": "string", "default": "fast" }
        },
        "additionalProperties": false },
      { "name": "speed", "type": "integer", "default": 5 }
    ],
    "returns": "string"
  }})");
}

TEST_F(TestJSONServiceDescription, UnionParameterDraft03)
{
  CheckUnionParameter(*this, R"({"Test.Union": {
    "type": "method", "description": "test", "transport": "Response", "permission": "ReadData",
    "params": [
      { "name": "target", "required": true, "type": [
          { "type": "object", "required": true,
            "properties": { "movieid": { "type": "integer", "required": true } },
            "additionalProperties": false },
          { "type": "object", "required": true,
            "properties": { "songid": { "type": "integer", "required": true } },
            "additionalProperties": false }
        ] },
      { "name": "when", "type": [
          "null",
          { "type": "string", "enum": ["now", "later"], "required": true }
        ], "default": null },
      { "name": "flag", "type": ["null", "boolean"], "default": null }
    ],
    "returns": "string"
  }})");
}

TEST_F(TestJSONServiceDescription, ExtendedTypeDraft03)
{
  CheckExtendedType(*this,
                    R"({"Base.A": {
    "type": "object",
    "properties": {
      "a": { "type": "string", "required": true },
      "shared": { "type": "integer", "default": 1 }
    }
  }})",
                    R"({"Derived.B": {
    "extends": "Base.A",
    "properties": {
      "b": { "type": "boolean", "required": true },
      "shared": { "type": "integer", "default": 2 }
    }
  }})",
                    R"({"Test.Extends": {
    "type": "method", "description": "test", "transport": "Response", "permission": "ReadData",
    "params": [ { "name": "data", "$ref": "Derived.B", "required": true } ],
    "returns": "string"
  }})");
}

TEST_F(TestJSONServiceDescription, ForwardReferencesDraft03)
{
  CheckForwardReferences(*this,
                         R"({"C.Base": {
    "type": "object",
    "properties": {
      "x": { "type": "integer", "required": true },
      "y": { "type": "integer", "default": 9 }
    }
  }})",
                         R"({"C.Container": {
    "type": "object",
    "properties": { "inner": { "$ref": "C.Base", "required": true } }
  }})",
                         R"({"Test.Forward": {
    "type": "method", "description": "test", "transport": "Response", "permission": "ReadData",
    "params": [ { "name": "data", "$ref": "C.Container", "required": true } ],
    "returns": "string"
  }})");
}

TEST_F(TestJSONServiceDescription, ReferenceWithLocalDefaultDraft03)
{
  CheckReferenceWithLocalDefault(*this,
                                 R"({"Level.T": {
    "type": "integer", "minimum": 0, "maximum": 10, "default": 5
  }})",
                                 R"({"Test.Ref": {
    "type": "method", "description": "test", "transport": "Response", "permission": "ReadData",
    "params": [ { "name": "level", "$ref": "Level.T", "default": 7 } ],
    "returns": "string"
  }})");
}

TEST_F(TestJSONServiceDescription, EnumParameterDraft03)
{
  CheckEnumParameter(*this, R"({"Test.Enum": {
    "type": "method", "description": "test", "transport": "Response", "permission": "ReadData",
    "params": [ { "name": "mode", "type": "string", "enum": ["one", "two"] } ],
    "returns": "string"
  }})");
}

TEST_F(TestJSONServiceDescription, PrintEmitsDraft03Shape)
{
  ASSERT_TRUE(CJSONServiceDescription::AddType(
      R"({"Print.Enum": { "type": "string", "enum": ["a", "b"] }})"));
  ASSERT_TRUE(CJSONServiceDescription::AddType(
      R"({"Print.Base": {
        "type": "object",
        "properties": { "p": { "type": "string", "required": true } }
      }})"));
  ASSERT_TRUE(CJSONServiceDescription::AddType(
      R"({"Print.Derived": {
        "extends": "Print.Base",
        "properties": { "q": { "type": "integer" } }
      }})"));
  ASSERT_TRUE(CJSONServiceDescription::AddType(
      R"({"Print.Union": { "type": [
        { "type": "string", "required": true },
        { "type": "integer", "required": true }
      ] }})"));

  CVariant result;
  ASSERT_EQ(OK, CJSONServiceDescription::Print(result, &m_transport, &m_client, true, true, false));

  const CVariant& types = result["types"];
  ASSERT_TRUE(types.isMember("Print.Enum"));

  // Introspect historically emits "enums" although the parser reads "enum"
  ExpectVariantEq(ParseJson(R"(["a", "b"])"), types["Print.Enum"]["enums"]);
  EXPECT_FALSE(types["Print.Enum"].isMember("enum"));

  ExpectVariantEq(CVariant("Print.Base"), types["Print.Derived"]["extends"]);

  ASSERT_TRUE(types["Print.Union"]["type"].isArray());
  EXPECT_EQ(2U, types["Print.Union"]["type"].size());
  EXPECT_TRUE(types["Print.Union"]["type"][0].isObject());

  ExpectVariantEq(CVariant(true), types["Print.Base"]["properties"]["p"]["required"]);
  ExpectVariantEq(CVariant("Print.Base"), types["Print.Base"]["id"]);
}
