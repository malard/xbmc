/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "JSONRPCTestUtils.h"
#include "ServiceDescription.h"
#include "interfaces/IAnnouncer.h"
#include "settings/lib/SettingLevel.h"
#include "utils/Variant.h"

#include <array>
#include <set>
#include <string>

#include <gtest/gtest.h>

using namespace JSONRPC;

namespace
{

constexpr std::array<SettingLevel, 4> VIEWER_LEVELS{SettingLevel::Basic, SettingLevel::Standard,
                                                    SettingLevel::Advanced, SettingLevel::Expert};

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

  EXPECT_EQ(named, EnumValues(ShippedType("Setting.Level")));
}

/*!
 The namespace is the announcement flag's name as IJSONRPCAnnouncer pastes it in front of the
 message, so the flag and the schema entry must agree.
 */
TEST(TestSettingsAnnouncementFlag, ItsNameIsTheNotificationNamespace)
{
  const std::string prefix{ANNOUNCEMENT::AnnouncementFlagToString(ANNOUNCEMENT::Settings)};

  EXPECT_EQ("Settings", prefix);
  ShippedNotification(prefix + ".OnLevelChanged");
}

//! \brief A flag left out of ANNOUNCE_ALL is announced to a client that never configured itself
TEST(TestSettingsAnnouncementFlag, ItIsDeliveredWithoutBeingAskedFor)
{
  EXPECT_EQ(ANNOUNCEMENT::Settings, ANNOUNCEMENT::ANNOUNCE_ALL & ANNOUNCEMENT::Settings);
}
