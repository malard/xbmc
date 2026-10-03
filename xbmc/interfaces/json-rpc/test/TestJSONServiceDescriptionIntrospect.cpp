/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "JSONRPCTestUtils.h"
#include "ServiceDescription.h"

#include <iterator>
#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace JSONRPC;

class TestJSONServiceDescriptionIntrospect : public JSONServiceDescriptionTestBase
{
};

/*!
 A definition a parser gate rejects vanishes silently, so the counts of what
 reaches the Introspect output are exact.
 */
TEST_F(TestJSONServiceDescriptionIntrospect, EveryDefinitionSurvivesToIntrospect)
{
  AddShippedServiceDescription();

  const size_t typeCount = std::size(JSONRPC_SERVICE_TYPES);
  const size_t methodCount = std::size(JSONRPC_SERVICE_METHODS);
  const size_t notificationCount = std::size(JSONRPC_SERVICE_NOTIFICATIONS);

  CVariant result;
  ASSERT_EQ(OK, CJSONServiceDescription::Print(result, &m_transport, &m_client, true, true, false));

  EXPECT_EQ(typeCount + RuntimeEnumNames().size(), result["types"].size());
  EXPECT_EQ(methodCount, result["methods"].size());
  EXPECT_EQ(notificationCount, result["notifications"].size());
  EXPECT_EQ(JSONRPC_STATUS_DESCRIPTIONS.size(), result["errors"].size());

  for (auto method = result["methods"].begin_map(); method != result["methods"].end_map(); ++method)
  {
    EXPECT_TRUE(method->second["errors"].isArray()) << method->first << " declares no errors";
    EXPECT_TRUE(method->second["reasons"].isObject()) << method->first << " declares no reasons";
  }
}

//! \brief A method's declared errors are served under it, with their descriptions
TEST_F(TestJSONServiceDescriptionIntrospect, DeclaredErrorsAreServedWithTheMethod)
{
  ASSERT_TRUE(CJSONServiceDescription::AddMethod(R"({"Test.Errors": {
    "type": "method", "description": "test", "transport": "Response", "permission": "ReadData",
    "params": [], "returns": "string", "errors": ["NotFound", "Unavailable"]
  }})",
                                                 StubMethod));

  CVariant result;
  ASSERT_EQ(OK, CJSONServiceDescription::Print(result, &m_transport, &m_client, true, true, false,
                                               "Test.Errors", "method"));

  const CVariant& errors = result["methods"]["Test.Errors"]["errors"];
  ASSERT_EQ(2u, errors.size());
  EXPECT_EQ("NotFound", errors[0].asString());
  EXPECT_EQ("Unavailable", errors[1].asString());

  EXPECT_EQ(2u, result["errors"].size());
  EXPECT_TRUE(result["errors"].isMember("NotFound"));
  EXPECT_TRUE(result["errors"].isMember("Unavailable"));
}

TEST_F(TestJSONServiceDescriptionIntrospect, AMethodDeclaringNoErrorsServesAnEmptyList)
{
  ASSERT_TRUE(CJSONServiceDescription::AddMethod(R"({"Test.NoErrors": {
    "type": "method", "description": "test", "transport": "Response", "permission": "ReadData",
    "params": [], "returns": "string", "errors": []
  }})",
                                                 StubMethod));

  CVariant result;
  ASSERT_EQ(OK, CJSONServiceDescription::Print(result, &m_transport, &m_client, true, true, false,
                                               "Test.NoErrors", "method"));

  const CVariant& errors = result["methods"]["Test.NoErrors"]["errors"];
  EXPECT_TRUE(errors.isArray());
  EXPECT_EQ(0u, errors.size());
  EXPECT_EQ(0u, result["errors"].size());
}

//! \brief A method's declared reasons are served under it, under the error each comes with
TEST_F(TestJSONServiceDescriptionIntrospect, DeclaredReasonsAreServedWithTheMethod)
{
  ASSERT_TRUE(CJSONServiceDescription::AddMethod(R"({"Test.Reasons": {
    "type": "method", "description": "test", "transport": "Response", "permission": "ReadData",
    "params": [], "returns": "string", "errors": ["FailedToExecute", "NotFound"],
    "reasons": {"FailedToExecute": ["nothing-playing", "not-seekable"],
                "NotFound": ["nothing-playing"]}
  }})",
                                                 StubMethod));

  CVariant result;
  ASSERT_EQ(OK, CJSONServiceDescription::Print(result, &m_transport, &m_client, true, true, false,
                                               "Test.Reasons", "method"));

  const CVariant& reasons = result["methods"]["Test.Reasons"]["reasons"];
  ASSERT_EQ(2u, reasons.size());
  ASSERT_EQ(2u, reasons["FailedToExecute"].size());
  EXPECT_EQ("nothing-playing", reasons["FailedToExecute"][0].asString());
  EXPECT_EQ("not-seekable", reasons["FailedToExecute"][1].asString());
  ASSERT_EQ(1u, reasons["NotFound"].size());
  EXPECT_EQ("nothing-playing", reasons["NotFound"][0].asString());

  EXPECT_FALSE(result.isMember("reasons"));
}

TEST_F(TestJSONServiceDescriptionIntrospect, AMethodDeclaringNoReasonsServesAnEmptyMap)
{
  ASSERT_TRUE(CJSONServiceDescription::AddMethod(R"({"Test.NoReasons": {
    "type": "method", "description": "test", "transport": "Response", "permission": "ReadData",
    "params": [], "returns": "string", "errors": ["FailedToExecute"], "reasons": {}
  }})",
                                                 StubMethod));

  CVariant result;
  ASSERT_EQ(OK, CJSONServiceDescription::Print(result, &m_transport, &m_client, true, true, false,
                                               "Test.NoReasons", "method"));

  const CVariant& reasons = result["methods"]["Test.NoReasons"]["reasons"];
  EXPECT_TRUE(reasons.isObject());
  EXPECT_EQ(0u, reasons.size());
}

TEST_F(TestJSONServiceDescriptionIntrospect, AnUnknownReasonIsRejected)
{
  EXPECT_FALSE(CJSONServiceDescription::AddMethod(R"({"Test.UnknownReason": {
    "type": "method", "description": "test", "transport": "Response", "permission": "ReadData",
    "params": [], "returns": "string", "errors": ["FailedToExecute"],
    "reasons": {"FailedToExecute": ["no-such-reason"]}
  }})",
                                                  StubMethod));
}

TEST_F(TestJSONServiceDescriptionIntrospect, AReasonUnderAnUndeclaredErrorIsRejected)
{
  EXPECT_FALSE(CJSONServiceDescription::AddMethod(R"({"Test.UndeclaredError": {
    "type": "method", "description": "test", "transport": "Response", "permission": "ReadData",
    "params": [], "returns": "string", "errors": ["FailedToExecute"],
    "reasons": {"NotFound": ["nothing-playing"]}
  }})",
                                                  StubMethod));
}

TEST_F(TestJSONServiceDescriptionIntrospect, AnUnknownErrorNameIsRejected)
{
  EXPECT_FALSE(CJSONServiceDescription::AddMethod(R"({"Test.Unknown": {
    "type": "method", "description": "test", "transport": "Response", "permission": "ReadData",
    "params": [], "returns": "string", "errors": ["NoSuchError"]
  }})",
                                                  StubMethod));
}
