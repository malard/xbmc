/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "JSONRPCTestUtils.h"
#include "ServiceDescription.h"
#include "utils/StringUtils.h"
#include "utils/Variant.h"

#include <algorithm>
#include <array>
#include <functional>
#include <map>
#include <regex>
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

/*!
 \brief Deprecated methods a replacement covers under a different signature

 A caller has to rewrite the request rather than only the method name, so these
 are held apart from the renames above: what they share is that the old name
 still works and still names somewhere to go.
 */
constexpr std::array<Supersession, 4> SUPERSEDED_METHODS{{
    {"VideoLibrary.RefreshMovie", "VideoLibrary.Refresh"},
    {"VideoLibrary.RefreshTVShow", "VideoLibrary.Refresh"},
    {"VideoLibrary.RefreshEpisode", "VideoLibrary.Refresh"},
    {"VideoLibrary.RefreshMusicVideo", "VideoLibrary.Refresh"},
}};

//! \brief Every deprecated method, however its replacement is reached
std::vector<Supersession> DeprecatedMethods()
{
  return {SUPERSEDED_METHODS.begin(), SUPERSEDED_METHODS.end()};
}

//! \brief Deprecated properties, as type name, property, and what replaces it
struct DeprecatedProperty
{
  const char* type;
  const char* property;
  const char* replacement;
};

constexpr std::array<DeprecatedProperty, 3> DEPRECATED_PROPERTIES{{
    {"PVR.Details.Broadcast", "seasonnum", "season"},
    {"PVR.Details.Broadcast", "episodenum", "episode"},
    {"PVR.Details.Broadcast", "isplayable", "PVR.GetBroadcastIsPlayable"},
}};

//! \brief Deprecated members of a method parameter, as "Method(parameter).member" and what
//! replaces them
constexpr std::array<Supersession, 3> DEPRECATED_PARAMETER_MEMBERS{{
    {"Player.Open(item).random", "shuffled"},
    {"AudioLibrary.GetArtists(filter).genreid", "songgenreid"},
    {"AudioLibrary.GetArtists(filter).genre", "songgenre"},
}};

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
    const CVariant& properties = type["properties"];
    for (auto property = properties.begin_map(); property != properties.end_map(); ++property)
    {
      if (property->second["deprecated"].asBoolean(false))
        found.push_back(name + "." + property->first);
    }
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
  for (const auto& [deprecated, replacement] : DeprecatedMethods())
    expected.emplace_back(deprecated);
  for (const auto& [type, property, replacement] : DEPRECATED_PROPERTIES)
    expected.emplace_back(std::string(type) + "." + property);
  for (const auto& [member, replacement] : DEPRECATED_PARAMETER_MEMBERS)
    expected.emplace_back(member);

  std::vector<std::string> declared{DeclaredDeprecations()};

  std::sort(expected.begin(), expected.end());
  std::sort(declared.begin(), declared.end());
  EXPECT_EQ(expected, declared);
}

TEST(TestDeprecatedMethodSchema, ADeprecatedMethodNamesAReplacementThatExists)
{
  const std::map<std::string, CVariant> methods{ShippedMethods()};

  for (const auto& [deprecated, replacement] : DeprecatedMethods())
  {
    ASSERT_TRUE(methods.contains(deprecated)) << deprecated;
    EXPECT_TRUE(methods.contains(replacement)) << replacement << " does not exist";

    const std::string description{methods.at(deprecated)["description"].asString()};
    EXPECT_NE(std::string::npos, description.find(replacement))
        << deprecated << " does not name " << replacement << " in its description";
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

    const std::string description{properties[property]["description"].asString()};
    EXPECT_NE(std::string::npos, description.find(replacement))
        << property << " does not name " << replacement << " in its description";
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

  for (const auto& [deprecated, replacement] : DeprecatedMethods())
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

/*!
 A removal schedule is revised between releases, so it lives in the API
 documentation rather than in the schema a client reads over the wire.
 */
TEST(TestDeprecatedMethodSchema, TheSchemaDoesNotDateItsOwnRemovals)
{
  const std::map<std::string, CVariant> methods{ShippedMethods()};

  for (const auto& [deprecated, replacement] : DeprecatedMethods())
  {
    const std::string description{methods.at(deprecated)["description"].asString()};
    EXPECT_EQ(std::string::npos, description.find("Kodi 2"))
        << deprecated << " names a Kodi version in its description";
    EXPECT_FALSE(std::regex_search(description, std::regex{"version [0-9]"}))
        << deprecated << " names an API version in its description";
  }
}

TEST(TestDeprecatedMethodSchema, ADeprecatedParameterMemberNamesItsReplacement)
{
  std::map<std::string, std::string> descriptions;
  for (const auto& [name, method] : ShippedMethods())
  {
    const CVariant& params{method["params"]};
    for (auto param = params.begin_array(); param != params.end_array(); ++param)
    {
      const std::string prefix{name + "(" + (*param)["name"].asString() + ")."};
      ForEachMember((*param)["schema"],
                    [&descriptions, &prefix](const std::string& member, const CVariant& schema)
                    { descriptions[prefix + member] = schema["description"].asString(); });
    }
  }

  for (const auto& [member, replacement] : DEPRECATED_PARAMETER_MEMBERS)
  {
    ASSERT_TRUE(descriptions.contains(member)) << member;
    EXPECT_NE(std::string::npos, descriptions.at(member).find(replacement))
        << member << " does not name " << replacement << " in its description";
  }
}

/*!
 Clients read the annotation, not the description, so a deprecation stated only in prose is
 one they never see.
 */
TEST(TestDeprecatedMethodSchema, NoDeprecationIsStatedOnlyInProse)
{
  const auto check = [](const std::string& where, const CVariant& schema)
  {
    EXPECT_FALSE(StringUtils::StartsWithNoCase(schema["description"].asString(), "deprecated"))
        << where << " is deprecated in its description only";
  };

  for (const auto& [name, method] : ShippedMethods())
  {
    check(name, method);
    const CVariant& params{method["params"]};
    for (auto param = params.begin_array(); param != params.end_array(); ++param)
    {
      const std::string prefix{name + "(" + (*param)["name"].asString() + ")."};
      check(prefix, *param);
      ForEachMember((*param)["schema"],
                    [&check, &prefix](const std::string& member, const CVariant& schema)
                    { check(prefix + member, schema); });
    }
  }

  for (const auto& [name, type] : ShippedTypes())
  {
    check(name, type);
    ForEachMember(type, [&check, &name](const std::string& member, const CVariant& schema)
                  { check(name + "." + member, schema); });
  }
}
