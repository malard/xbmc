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

  static std::string Text(ANNOUNCEMENT::AnnouncementFlag flag,
                          const std::string& message,
                          const CVariant& data)
  {
    return AnnouncementToJSONRPC(flag, "xbmc", message, data, true);
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

TEST(TestAnnouncementToJSONRPC, AnUpdatedLibraryItemIsThatItemsPropertiesChanging)
{
  CVariant data;
  data["id"] = 7;
  data["type"] = "movie";

  const CVariant notification =
      CTestAnnouncer::Notification(ANNOUNCEMENT::VideoLibrary, "OnUpdate", data);

  EXPECT_EQ("VideoLibrary.OnItemPropertiesChanged", notification["method"].asString());
  const CVariant& changed = notification["params"]["data"];
  EXPECT_EQ("movie", changed["item"]["kind"].asString());
  EXPECT_EQ(7, changed["item"]["id"].asInteger());
  EXPECT_FALSE(changed.isMember("properties"));
  EXPECT_FALSE(changed.isMember("id"));
  EXPECT_FALSE(changed.isMember("type"));
}

TEST(TestAnnouncementToJSONRPC, AnItemsPlayCountIsCarriedUnderItsPropertyName)
{
  CVariant data;
  data["item"]["id"] = 12;
  data["item"]["type"] = "episode";
  data["playcount"] = 1;
  data["transaction"] = true;

  const CVariant notification =
      CTestAnnouncer::Notification(ANNOUNCEMENT::VideoLibrary, "OnUpdate", data);

  const CVariant& changed = notification["params"]["data"];
  EXPECT_EQ("episode", changed["item"]["kind"].asString());
  EXPECT_EQ(12, changed["item"]["id"].asInteger());
  EXPECT_EQ(1, changed["properties"]["playCount"].asInteger());
  EXPECT_TRUE(changed["transaction"].asBoolean());
  EXPECT_FALSE(changed.isMember("playcount"));
}

TEST(TestAnnouncementToJSONRPC, TheChangedPropertiesAnUpdateNamesAreCarried)
{
  CVariant data;
  data["id"] = 3;
  data["type"] = "song";
  data["properties"]["title"] = "Retitled";

  const CVariant notification =
      CTestAnnouncer::Notification(ANNOUNCEMENT::AudioLibrary, "OnUpdate", data);

  EXPECT_EQ("AudioLibrary.OnItemPropertiesChanged", notification["method"].asString());
  const CVariant& changed = notification["params"]["data"];
  EXPECT_EQ("song", changed["item"]["kind"].asString());
  EXPECT_EQ("Retitled", changed["properties"]["title"].asString());
}

TEST(TestAnnouncementToJSONRPC, AnAddedItemIsItsOwnEvent)
{
  CVariant scanned;
  scanned["item"]["id"] = 9;
  scanned["item"]["type"] = "movie";
  scanned["added"] = true;
  scanned["transaction"] = true;

  const CVariant movie =
      CTestAnnouncer::Notification(ANNOUNCEMENT::VideoLibrary, "OnUpdate", scanned);

  EXPECT_EQ("VideoLibrary.OnItemAdded", movie["method"].asString());
  const CVariant& added = movie["params"]["data"];
  EXPECT_EQ("movie", added["item"]["kind"].asString());
  EXPECT_EQ(9, added["item"]["id"].asInteger());
  EXPECT_TRUE(added["transaction"].asBoolean());
  EXPECT_FALSE(added.isMember("added"));
  EXPECT_FALSE(added.isMember("properties"));

  CVariant song;
  song["id"] = 4;
  song["type"] = "song";
  song["added"] = true;

  const CVariant notification =
      CTestAnnouncer::Notification(ANNOUNCEMENT::AudioLibrary, "OnUpdate", song);

  EXPECT_EQ("AudioLibrary.OnItemAdded", notification["method"].asString());
  EXPECT_EQ("song", notification["params"]["data"]["item"]["kind"].asString());
  EXPECT_FALSE(notification["params"]["data"].isMember("transaction"));
}

TEST(TestAnnouncementToJSONRPC, AnUpdateThatAddsNothingCarriesNoAddedMarker)
{
  CVariant data;
  data["id"] = 5;
  data["type"] = "episode";
  data["added"] = false;

  const CVariant notification =
      CTestAnnouncer::Notification(ANNOUNCEMENT::VideoLibrary, "OnUpdate", data);

  EXPECT_EQ("VideoLibrary.OnItemPropertiesChanged", notification["method"].asString());
  EXPECT_FALSE(notification["params"]["data"].isMember("added"));
}

TEST(TestAnnouncementToJSONRPC, AnUpdateToNoLibraryItemIsNotSent)
{
  CVariant data;
  data["item"]["type"] = "movie";
  data["item"]["title"] = "Played from a file";
  data["playcount"] = 1;

  EXPECT_TRUE(CTestAnnouncer::Text(ANNOUNCEMENT::VideoLibrary, "OnUpdate", data).empty());

  CVariant unknown;
  unknown["id"] = -1;
  unknown["type"] = "";
  EXPECT_TRUE(CTestAnnouncer::Text(ANNOUNCEMENT::VideoLibrary, "OnUpdate", unknown).empty());

  unknown["added"] = true;
  EXPECT_TRUE(CTestAnnouncer::Text(ANNOUNCEMENT::VideoLibrary, "OnUpdate", unknown).empty());
}

TEST(TestAnnouncementToJSONRPC, ARemovedLibraryItemIsOnItemRemoved)
{
  CVariant cleaned;
  cleaned["id"] = 7;
  cleaned["type"] = "movie";
  cleaned["transaction"] = true;

  const CVariant movie =
      CTestAnnouncer::Notification(ANNOUNCEMENT::VideoLibrary, "OnRemove", cleaned);

  EXPECT_EQ("VideoLibrary.OnItemRemoved", movie["method"].asString());
  const CVariant& removed = movie["params"]["data"];
  EXPECT_EQ("movie", removed["item"]["kind"].asString());
  EXPECT_EQ(7, removed["item"]["id"].asInteger());
  EXPECT_TRUE(removed["transaction"].asBoolean());
  EXPECT_FALSE(removed.isMember("id"));
  EXPECT_FALSE(removed.isMember("type"));
  EXPECT_FALSE(removed.isMember("properties"));

  CVariant song;
  song["id"] = 4;
  song["type"] = "song";

  const CVariant notification =
      CTestAnnouncer::Notification(ANNOUNCEMENT::AudioLibrary, "OnRemove", song);

  EXPECT_EQ("AudioLibrary.OnItemRemoved", notification["method"].asString());
  EXPECT_EQ("song", notification["params"]["data"]["item"]["kind"].asString());
  EXPECT_FALSE(notification["params"]["data"].isMember("transaction"));
}

TEST(TestAnnouncementToJSONRPC, ANotificationNamingNoLibraryItemIsNotSent)
{
  CVariant unknown;
  unknown["id"] = -1;
  unknown["type"] = "movie";
  EXPECT_TRUE(CTestAnnouncer::Text(ANNOUNCEMENT::VideoLibrary, "OnRemove", unknown).empty());

  // a movie's version is an asset of the movie, not an item a client can address
  CVariant version;
  version["id"] = 12;
  version["type"] = "videoversion";
  EXPECT_TRUE(CTestAnnouncer::Text(ANNOUNCEMENT::VideoLibrary, "OnRemove", version).empty());
  EXPECT_TRUE(CTestAnnouncer::Text(ANNOUNCEMENT::VideoLibrary, "OnUpdate", version).empty());

  CVariant movie;
  movie["id"] = 12;
  movie["type"] = "movie";
  EXPECT_TRUE(CTestAnnouncer::Text(ANNOUNCEMENT::AudioLibrary, "OnRemove", movie).empty());
}

TEST(TestAnnouncementToJSONRPC, APlaylistRemovalKeepsItsEvent)
{
  CVariant data;
  data["playlist"] = "video";
  data["position"] = 2;

  const CVariant notification =
      CTestAnnouncer::Notification(ANNOUNCEMENT::Playlist, "OnRemove", data);

  EXPECT_EQ("Playlist.OnRemove", notification["method"].asString());
  EXPECT_EQ(2, notification["params"]["data"]["position"].asInteger());
}
