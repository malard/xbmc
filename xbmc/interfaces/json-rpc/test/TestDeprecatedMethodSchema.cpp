/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "JSONRPCTestUtils.h"
#include "ServiceDescription.h"
#include "utils/Variant.h"

#include <algorithm>
#include <array>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace JSONRPC;

namespace
{

/*!
 \brief What a deprecated definition is superseded by

 A caller moved off the deprecated name has to land on something that exists,
 or the schema is telling them to break their client.
 */
struct Supersession
{
  const char* deprecated;
  const char* replacement;
};

constexpr std::array<Supersession, 4> SUPERSEDED_METHODS{{
    {"VideoLibrary.RefreshMovie", "VideoLibrary.Refresh"},
    {"VideoLibrary.RefreshTVShow", "VideoLibrary.Refresh"},
    {"VideoLibrary.RefreshEpisode", "VideoLibrary.Refresh"},
    {"VideoLibrary.RefreshMusicVideo", "VideoLibrary.Refresh"},
}};

//! \brief Deprecated properties, as type name, property, and what replaces it
struct DeprecatedProperty
{
  const char* type;
  const char* property;
  const char* replacement;
};

constexpr std::array<DeprecatedProperty, 3> DEPRECATED_PROPERTIES{{
    {"PVR.Details.Broadcast", "seasonNum", "season"},
    {"PVR.Details.Broadcast", "episodeNum", "episode"},
    {"PVR.Details.Broadcast", "isPlayable", "PVR.GetBroadcastIsPlayable"},
}};

//! \brief Deprecated members of a method parameter, as "Method(parameter).member", and of a type
//! at any depth, as "Type.member"
constexpr std::array<const char*, 3> DEPRECATED_MEMBERS{
    "Player.Open(item).random",
    "Audio.Filter.Artists.genreId",
    "Audio.Filter.Artists.genre",
};

//! \brief Calls \p visit with every member a schema declares, however deeply it is nested
void ForEachMember(const CVariant& schema,
                   const std::function<void(const std::string&, const CVariant&)>& visit)
{
  if (schema.isArray())
  {
    for (auto entry = schema.begin_array(); entry != schema.end_array(); ++entry)
      ForEachMember(*entry, visit);
    return;
  }
  if (!schema.isObject())
    return;

  const CVariant& properties{schema["properties"]};
  for (auto property = properties.begin_map(); property != properties.end_map(); ++property)
  {
    visit(property->first, property->second);
    ForEachMember(property->second, visit);
  }
  for (const char* key : {"anyOf", "allOf", "items"})
    ForEachMember(schema[key], visit);
}

//! \brief Every deprecated definition the schema declares, as "name" or "Type.property"
std::vector<std::string> DeclaredDeprecations()
{
  std::vector<std::string> found;

  for (const auto& [name, method] : ShippedMethods())
  {
    if (method["deprecated"].asBoolean(false))
      found.push_back(name);

    const CVariant& params{method["params"]};
    for (auto param = params.begin_array(); param != params.end_array(); ++param)
    {
      const std::string prefix{name + "(" + (*param)["name"].asString() + ")."};
      ForEachMember((*param)["schema"],
                    [&found, &prefix](const std::string& member, const CVariant& schema)
                    {
                      if (schema["deprecated"].asBoolean(false))
                        found.push_back(prefix + member);
                    });
    }
  }

  for (const auto& [name, type] : ShippedTypes())
  {
    const std::string prefix{name + "."};
    ForEachMember(type,
                  [&found, &prefix](const std::string& member, const CVariant& schema)
                  {
                    if (schema["deprecated"].asBoolean(false))
                      found.push_back(prefix + member);
                  });
  }

  return found;
}

} // unnamed namespace

/*!
 Every deprecation the schema declares must be listed here, so the tests below
 cover the whole schema rather than whatever they happen to name.
 */
TEST(TestDeprecatedMethodSchema, EveryDeprecationIsAccountedFor)
{
  std::vector<std::string> expected;
  for (const auto& [deprecated, replacement] : SUPERSEDED_METHODS)
    expected.emplace_back(deprecated);
  for (const auto& [type, property, replacement] : DEPRECATED_PROPERTIES)
    expected.emplace_back(std::string(type) + "." + property);
  for (const char* member : DEPRECATED_MEMBERS)
    expected.emplace_back(member);

  std::vector<std::string> declared{DeclaredDeprecations()};

  std::sort(expected.begin(), expected.end());
  std::sort(declared.begin(), declared.end());
  EXPECT_EQ(expected, declared);
}

TEST(TestDeprecatedMethodSchema, ADeprecatedMethodNamesAReplacementThatExists)
{
  const std::map<std::string, CVariant> methods{ShippedMethods()};

  for (const auto& [deprecated, replacement] : SUPERSEDED_METHODS)
  {
    ASSERT_TRUE(methods.contains(deprecated)) << deprecated;
    EXPECT_TRUE(methods.contains(replacement)) << replacement << " does not exist";
  }
}

TEST(TestDeprecatedMethodSchema, ADeprecatedPropertyNamesAReplacementThatExists)
{
  const std::map<std::string, CVariant> types{ShippedTypes()};
  const std::map<std::string, CVariant> methods{ShippedMethods()};

  for (const auto& [typeName, property, replacement] : DEPRECATED_PROPERTIES)
  {
    ASSERT_TRUE(types.contains(typeName)) << typeName;
    const CVariant& properties = types.at(typeName)["properties"];

    ASSERT_TRUE(properties.isMember(property)) << property;
    EXPECT_TRUE(properties.isMember(replacement) || methods.contains(replacement))
        << replacement << " is neither a member of " << typeName << " nor a method";
  }
}

/*!
 The replacement is what callers are being sent to, so deprecating it as well
 would leave the schema pointing at a dead end.
 */
TEST(TestDeprecatedMethodSchema, AReplacementIsNotItselfDeprecated)
{
  const std::map<std::string, CVariant> methods{ShippedMethods()};
  const std::map<std::string, CVariant> types{ShippedTypes()};

  for (const auto& [deprecated, replacement] : SUPERSEDED_METHODS)
  {
    ASSERT_TRUE(methods.contains(replacement)) << replacement;
    EXPECT_FALSE(methods.at(replacement)["deprecated"].asBoolean(false)) << replacement;
  }

  for (const auto& [typeName, property, replacement] : DEPRECATED_PROPERTIES)
  {
    ASSERT_TRUE(types.contains(typeName)) << typeName;
    const CVariant& properties = types.at(typeName)["properties"];
    EXPECT_FALSE(properties[replacement]["deprecated"].asBoolean(false)) << replacement;
  }
}
