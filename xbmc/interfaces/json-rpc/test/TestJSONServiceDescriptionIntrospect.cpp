/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "JSONRPCTestUtils.h"
#include "ServiceDescription.h"

#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace JSONRPC;

namespace
{

/*!
 The names CJSONRPC::Initialize registers at runtime via AddEnum. Only the
 names matter for reference resolution; the values are runtime data.
 */
const std::vector<std::string> RUNTIME_ENUMS = {
    "Addon.Types",
    "Input.Action",
    "GUI.Window",
    "List.Filter.Operators",
    "List.Filter.Fields.Movies",
    "List.Filter.Fields.TVShows",
    "List.Filter.Fields.Episodes",
    "List.Filter.Fields.MusicVideos",
    "List.Filter.Fields.Artists",
    "List.Filter.Fields.Albums",
    "List.Filter.Fields.Songs",
    "List.Filter.Fields.Textures",
};

} // unnamed namespace

class TestJSONServiceDescriptionIntrospect : public JSONServiceDescriptionTestBase
{
};

/*!
 Feeds the full generated service description through the parser and asserts
 that every definition survives to the Introspect output. A definition that a
 parser gate rejects without queueing a missing reference vanishes silently;
 the exact counts here are the net that catches that.
 */
TEST_F(TestJSONServiceDescriptionIntrospect, EveryDefinitionSurvivesToIntrospect)
{
  for (const std::string& name : RUNTIME_ENUMS)
    ASSERT_TRUE(CJSONServiceDescription::AddEnum(name, std::vector<std::string>{"placeholder"}));

  const size_t typeCount = std::size(JSONRPC_SERVICE_TYPES);
  for (unsigned int index = 0; index < typeCount; index++)
    CJSONServiceDescription::AddType(JSONRPC_SERVICE_TYPES[index]);

  const size_t methodCount = std::size(JSONRPC_SERVICE_METHODS);
  for (unsigned int index = 0; index < methodCount; index++)
    CJSONServiceDescription::AddBuiltinMethod(JSONRPC_SERVICE_METHODS[index]);

  const size_t notificationCount = std::size(JSONRPC_SERVICE_NOTIFICATIONS);
  for (unsigned int index = 0; index < notificationCount; index++)
    CJSONServiceDescription::AddNotification(JSONRPC_SERVICE_NOTIFICATIONS[index]);

  CJSONServiceDescription::ResolveReferences();

  CVariant result;
  ASSERT_EQ(OK, CJSONServiceDescription::Print(result, &m_transport, &m_client, true, true, false));

  EXPECT_EQ(typeCount + RUNTIME_ENUMS.size(), result["types"].size());
  EXPECT_EQ(methodCount, result["methods"].size());
  EXPECT_EQ(notificationCount, result["notifications"].size());
  EXPECT_EQ(JSONRPC_STATUS_DESCRIPTIONS.size(), result["errors"].size());

  for (auto method = result["methods"].begin_map(); method != result["methods"].end_map(); ++method)
  {
    EXPECT_TRUE(method->second["errors"].isArray()) << method->first << " declares no errors";
  }

  // With KODI_TEST_DUMP_INTROSPECT set to a path, the full description is
  // written there so that schema changes can be diffed on the wire format
  const char* dumpPath = getenv("KODI_TEST_DUMP_INTROSPECT");
  if (dumpPath != nullptr)
  {
    std::ofstream dump(dumpPath, std::ofstream::trunc);
    dump << ToJson(result);
  }
}

//! \brief The header of an Introspect answer identifies the API, and Kodi has not been called
//! XBMC since 2014
TEST_F(TestJSONServiceDescriptionIntrospect, TheServiceHeaderIsNotBranded)
{
  CVariant result;
  ASSERT_EQ(OK, CJSONServiceDescription::Print(result, &m_transport, &m_client, true, true, false));

  EXPECT_EQ(std::string::npos, result["id"].asString().find("xbmc"))
      << "service id: " << result["id"].asString();
  EXPECT_EQ(std::string::npos, result["description"].asString().find("XBMC"))
      << "service description: " << result["description"].asString();
}

//! \brief A method's declared errors are served under it, and an answer filtered to the
//! method carries their descriptions as it does the types it references
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

TEST_F(TestJSONServiceDescriptionIntrospect, AnUnknownErrorNameIsRejected)
{
  EXPECT_FALSE(CJSONServiceDescription::AddMethod(R"({"Test.Unknown": {
    "type": "method", "description": "test", "transport": "Response", "permission": "ReadData",
    "params": [], "returns": "string", "errors": ["NoSuchError"]
  }})",
                                                  StubMethod));
}
