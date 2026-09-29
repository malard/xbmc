/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "interfaces/json-rpc/IJSONRPCAnnouncer.h"
#include "utils/JSONVariantParser.h"
#include "utils/Variant.h"

#include <string>

#include <gtest/gtest.h>

using namespace JSONRPC;

namespace
{
class CTestAnnouncer : public IJSONRPCAnnouncer
{
public:
  void Announce(ANNOUNCEMENT::AnnouncementFlag flag,
                const std::string& sender,
                const std::string& message,
                const CVariant& data) override
  {
  }

  static CVariant Notification(ANNOUNCEMENT::AnnouncementFlag flag,
                               const std::string& message,
                               const CVariant& data)
  {
    CVariant notification;
    CJSONVariantParser::Parse(AnnouncementToJSONRPC(flag, "xbmc", message, data, true),
                              notification);
    return notification;
  }
};

CVariant PlaybackData(int speed)
{
  CVariant data;
  data["item"]["type"] = "movie";
  data["player"]["players"] = CVariant(CVariant::VariantTypeArray);
  data["player"]["players"].push_back("video");
  data["player"]["speed"] = speed;
  return data;
}
} // unnamed namespace

TEST(TestAnnouncementToJSONRPC, APauseIsASpeedChange)
{
  const CVariant notification =
      CTestAnnouncer::Notification(ANNOUNCEMENT::Player, "OnPause", PlaybackData(0));

  EXPECT_EQ("Player.OnPropertiesChanged", notification["method"].asString());
  const CVariant& data = notification["params"]["data"];
  EXPECT_EQ(0, data["properties"]["speed"].asInteger());
  EXPECT_EQ("video", data["player"]["players"][0].asString());
  EXPECT_FALSE(data.isMember("item"));
}

TEST(TestAnnouncementToJSONRPC, AResumeAndASpeedChangeAreSpeedChanges)
{
  for (const char* message : {"OnResume", "OnSpeedChanged"})
  {
    const CVariant notification =
        CTestAnnouncer::Notification(ANNOUNCEMENT::Player, message, PlaybackData(2));

    EXPECT_EQ("Player.OnPropertiesChanged", notification["method"].asString()) << message;
    EXPECT_EQ(2, notification["params"]["data"]["properties"]["speed"].asInteger()) << message;
  }
}

TEST(TestAnnouncementToJSONRPC, ASeekIsATimeChange)
{
  CVariant data = PlaybackData(1);
  data["player"]["time"]["minutes"] = 12;
  data["player"]["seekoffset"]["seconds"] = 30;

  const CVariant notification = CTestAnnouncer::Notification(ANNOUNCEMENT::Player, "OnSeek", data);

  EXPECT_EQ("Player.OnPropertiesChanged", notification["method"].asString());
  const CVariant& properties = notification["params"]["data"]["properties"];
  EXPECT_EQ(12, properties["time"]["minutes"].asInteger());
  EXPECT_FALSE(properties.isMember("speed"));
}

TEST(TestAnnouncementToJSONRPC, AnEventKeepsItsName)
{
  const CVariant notification =
      CTestAnnouncer::Notification(ANNOUNCEMENT::Player, "OnPlay", PlaybackData(1));

  EXPECT_EQ("Player.OnPlay", notification["method"].asString());
  EXPECT_EQ("movie", notification["params"]["data"]["item"]["type"].asString());
}
