/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "ServiceDescription.h"
#include "utils/JSONVariantParser.h"
#include "utils/Variant.h"

#include <algorithm>
#include <array>
#include <map>
#include <string>

#include <gtest/gtest.h>

using namespace JSONRPC;

namespace
{

/*!
 \brief The methods of the shipped service description, by name

 Read out of what actually ships rather than restated here, so that a method
 which never reaches the schema fails rather than passes.
 */
std::map<std::string, CVariant> ShippedMethods()
{
  std::map<std::string, CVariant> methods;

  for (const char* const entry : JSONRPC_SERVICE_METHODS)
  {
    // Each entry is one definition without its enclosing braces
    CVariant parsed;
    if (!CJSONVariantParser::Parse("{" + std::string(entry) + "}", parsed))
      continue;

    for (auto member = parsed.begin_map(); member != parsed.end_map(); ++member)
      methods.emplace(member->first, member->second);
  }

  return methods;
}

/*!
 \brief What a deprecated method is superseded by, and what it must agree with

 A caller moved off the deprecated name has to land on something that takes
 the same request and gives the same answer, or the note is telling them to
 break their client.
 */
struct Supersession
{
  const char* deprecated;
  const char* replacement;
};

constexpr std::array<Supersession, 2> SUPERSESSIONS{{
    {"XBMC.GetInfoLabels", "GUI.GetInfoLabels"},
    {"XBMC.GetInfoBooleans", "GUI.GetInfoBooleans"},
}};

} // unnamed namespace

/*!
 The deprecation note is the only warning a client gets, and it is served
 through JSONRPC.Introspect rather than written in a release note, so it has to
 name the replacement it tells the caller to move to.
 */
TEST(TestDeprecatedMethodSchema, EveryDeprecatedMethodNamesAReplacementThatExists)
{
  const std::map<std::string, CVariant> methods{ShippedMethods()};
  ASSERT_FALSE(methods.empty());

  for (const auto& [name, method] : methods)
  {
    if (!method.isMember("deprecated"))
      continue;

    const std::string note{method["deprecated"].asString()};
    EXPECT_FALSE(note.empty()) << name << " is deprecated without saying anything";

    const auto supersession = std::find_if(SUPERSESSIONS.begin(), SUPERSESSIONS.end(),
                                           [&name = name](const Supersession& entry)
                                           { return name == entry.deprecated; });
    ASSERT_NE(SUPERSESSIONS.end(), supersession)
        << name << " is deprecated but this test does not know what supersedes it";

    EXPECT_NE(std::string::npos, note.find(supersession->replacement))
        << name << " does not name " << supersession->replacement << " in its deprecation note";
    EXPECT_TRUE(methods.contains(supersession->replacement))
        << supersession->replacement << " does not exist";
  }
}

/*!
 The two are the same implementation under two names, so a client that follows
 the note must not find the request or the answer has changed underneath it.
 */
TEST(TestDeprecatedMethodSchema, ADeprecatedMethodAgreesWithItsReplacement)
{
  const std::map<std::string, CVariant> methods{ShippedMethods()};

  for (const auto& [deprecated, replacement] : SUPERSESSIONS)
  {
    ASSERT_TRUE(methods.contains(deprecated)) << deprecated;
    ASSERT_TRUE(methods.contains(replacement)) << replacement;

    const CVariant& from{methods.at(deprecated)};
    const CVariant& to{methods.at(replacement)};

    EXPECT_TRUE(from["params"] == to["params"])
        << deprecated << " and " << replacement << " do not take the same parameters";
    EXPECT_TRUE(from["returns"] == to["returns"])
        << deprecated << " and " << replacement << " do not return the same thing";
    EXPECT_EQ(from["permission"].asString(), to["permission"].asString())
        << deprecated << " and " << replacement << " do not require the same permission";
  }
}

/*!
 The replacement is the name callers are being sent to, so deprecating it as
 well would leave the note pointing at a dead end.
 */
TEST(TestDeprecatedMethodSchema, AReplacementIsNotItselfDeprecated)
{
  const std::map<std::string, CVariant> methods{ShippedMethods()};

  for (const auto& [deprecated, replacement] : SUPERSESSIONS)
  {
    ASSERT_TRUE(methods.contains(replacement)) << replacement;
    EXPECT_FALSE(methods.at(replacement).isMember("deprecated")) << replacement;
  }
}
