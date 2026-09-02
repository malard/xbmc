/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "ServiceDescription.h"
#include "interfaces/IAnnouncer.h"
#include "settings/lib/SettingLevel.h"
#include "utils/JSONVariantParser.h"
#include "utils/Variant.h"

#include <array>
#include <set>
#include <string>

#include <gtest/gtest.h>

namespace
{

constexpr std::array<SettingLevel, 4> VIEWER_LEVELS{SettingLevel::Basic, SettingLevel::Standard,
                                                    SettingLevel::Advanced, SettingLevel::Expert};

//! \brief One definition out of the shipped service description, by name
CVariant Definition(const char* const entries[], size_t count, const std::string& name)
{
  for (size_t i = 0; i < count; ++i)
  {
    // Each entry is one definition without its enclosing braces
    CVariant parsed;
    if (!CJSONVariantParser::Parse("{" + std::string(entries[i]) + "}", parsed))
    {
      continue;
    }

    if (parsed.isMember(name))
    {
      return parsed[name];
    }
  }

  ADD_FAILURE() << name << " is not declared in the service description";
  return {};
}

CVariant Method(const std::string& name)
{
  return Definition(JSONRPC::JSONRPC_SERVICE_METHODS, std::size(JSONRPC::JSONRPC_SERVICE_METHODS),
                    name);
}

CVariant Notification(const std::string& name)
{
  return Definition(JSONRPC::JSONRPC_SERVICE_NOTIFICATIONS,
                    std::size(JSONRPC::JSONRPC_SERVICE_NOTIFICATIONS), name);
}

CVariant Type(const std::string& name)
{
  return Definition(JSONRPC::JSONRPC_SERVICE_TYPES, std::size(JSONRPC::JSONRPC_SERVICE_TYPES),
                    name);
}

std::set<std::string> RequiredMembers(const CVariant& object)
{
  std::set<std::string> required;
  const CVariant& values{object["required"]};
  for (auto value = values.begin_array(); value != values.end_array(); ++value)
  {
    required.insert(value->asString());
  }

  return required;
}

const CVariant* Param(const CVariant& method, const std::string& name)
{
  const CVariant& params{method["params"]};
  for (auto param = params.begin_array(); param != params.end_array(); ++param)
  {
    if (param->isMember("name") && (*param)["name"].asString() == name)
    {
      return &(*param);
    }
  }

  return nullptr;
}

} // unnamed namespace

TEST(TestSettingLevelName, EveryLevelAViewerCanBeAtHasAName)
{
  for (const auto level : VIEWER_LEVELS)
  {
    EXPECT_NE(nullptr, SettingLevelToString(level)) << "value " << static_cast<int>(level);
  }
}

//! \brief Internal is never a level the viewer is at, and having no name keeps it out of an answer
TEST(TestSettingLevelName, InternalHasNoName)
{
  EXPECT_EQ(nullptr, SettingLevelToString(SettingLevel::Internal));
}

//! \brief The names are a wire format, so the enum and the schema have to agree term for term
TEST(TestSettingLevelName, TheNamesAreExactlyTheSchemaEnum)
{
  std::set<std::string> named;
  for (const auto level : VIEWER_LEVELS)
  {
    named.insert(SettingLevelToString(level));
  }

  std::set<std::string> declared;
  const CVariant& values{Type("Setting.Level")["enum"]};
  for (auto value = values.begin_array(); value != values.end_array(); ++value)
  {
    declared.insert(value->asString());
  }

  EXPECT_EQ(named, declared);
}

TEST(TestSettingsLevelSchema, TheLevelInForceCanBeRead)
{
  const CVariant method{Method("Settings.GetLevel")};

  EXPECT_EQ("ReadData", method["permission"].asString());
  EXPECT_EQ("#/$defs/Setting.Level", method["returns"]["properties"]["level"]["$ref"].asString());
  EXPECT_EQ(std::set<std::string>{"level"}, RequiredMembers(method["returns"]));
}

TEST(TestSettingsLevelSchema, TheLevelInForceCanBeSet)
{
  const CVariant method{Method("Settings.SetLevel")};

  const CVariant* const level{Param(method, "level")};
  ASSERT_NE(nullptr, level);
  EXPECT_TRUE((*level)["required"].asBoolean());

  // The profile's settings lock can refuse the level asked for, so the answer names the one that
  // ended up in force rather than echoing the request
  EXPECT_EQ(std::set<std::string>{"level"}, RequiredMembers(method["returns"]));
}

/*!
 A listing is filtered at the level the caller passed, not the level the viewer is at; the
 answer names it so a client can tell the two apart.
 */
TEST(TestSettingsLevelSchema, AListingNamesTheLevelItFilteredAt)
{
  for (const char* const name :
       {"Settings.GetSections", "Settings.GetCategories", "Settings.GetSettings"})
  {
    const CVariant returns{Method(name)["returns"]};
    EXPECT_TRUE(returns["properties"].isMember("level")) << name;
    EXPECT_TRUE(RequiredMembers(returns).contains("level")) << name;
  }
}

TEST(TestSettingsLevelSchema, TheLevelChangeIsAnnounced)
{
  const CVariant data{Notification("Settings.OnLevelChanged")["params"][1]};

  EXPECT_EQ("data", data["name"].asString());
  EXPECT_TRUE(data["schema"]["properties"].isMember("level"));
  EXPECT_EQ(std::set<std::string>{"level"}, RequiredMembers(data["schema"]));
}

/*!
 The namespace is the announcement flag's name as IJSONRPCAnnouncer pastes it in front of the
 message, so the flag and the schema entry must agree.
 */
TEST(TestSettingsAnnouncementFlag, ItsNameIsTheNotificationNamespace)
{
  const std::string prefix{ANNOUNCEMENT::AnnouncementFlagToString(ANNOUNCEMENT::Settings)};

  EXPECT_EQ("Settings", prefix);
  Notification(prefix + ".OnLevelChanged");
}

//! \brief A flag left out of ANNOUNCE_ALL is announced to a client that never configured itself
TEST(TestSettingsAnnouncementFlag, ItIsDeliveredWithoutBeingAskedFor)
{
  EXPECT_EQ(ANNOUNCEMENT::Settings, ANNOUNCEMENT::ANNOUNCE_ALL & ANNOUNCEMENT::Settings);
}
