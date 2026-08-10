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
