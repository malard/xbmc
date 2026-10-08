/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "FileItem.h"
#include "interfaces/AnnouncementEvents.h"
#include "utils/JSONVariantParser.h"
#include "utils/JSONVariantWriter.h"
#include "utils/Variant.h"

#include <chrono>
#include <memory>
#include <string>

#include <gtest/gtest.h>

using namespace ANNOUNCEMENT;
using namespace std::chrono_literals;

/*
 * Python add-ons receive LegacyDataOf() as onNotification data, so these expectations are a
 * contract: an addition is allowed, but nothing may move, be renamed, be removed or change type.
 */
namespace
{
using EVENT::PLAYER::Players;

constexpr Players VIDEO{.video = true, .audio = false};
constexpr Players VIDEO_AND_AUDIO{.video = true, .audio = true};

//! Compared as JSON text, the form Python receives; the parser reads numbers as unsigned.
void ExpectData(const Announcement& announcement, const std::string& expected)
{
  CVariant parsed;
  ASSERT_TRUE(CJSONVariantParser::Parse(expected, parsed)) << "invalid test JSON: " << expected;
  std::string expectedJson;
  std::string actualJson;
  CJSONVariantWriter::Write(parsed, expectedJson, true);
  CJSONVariantWriter::Write(LegacyDataOf(announcement), actualJson, true);
  EXPECT_EQ(expectedJson, actualJson);
}
} // unnamed namespace

TEST(TestAnnouncementEvents, PlayerEventsKeepTheirMessages)
{
  EXPECT_STREQ("OnPlay", MessageOf(PlayerEvent{EVENT::PLAYER::Play{}}));
  EXPECT_STREQ("OnStop", MessageOf(PlayerEvent{EVENT::PLAYER::Stop{}}));
  EXPECT_STREQ("OnSeek", MessageOf(PlayerEvent{EVENT::PLAYER::Seek{}}));
  EXPECT_STREQ("OnPropertiesChanged", MessageOf(PlayerEvent{EVENT::PLAYER::PropertiesChanged{}}));
  EXPECT_STREQ("OnContentGeometryChange",
               MessageOf(PlayerEvent{EVENT::PLAYER::ContentGeometryChange{}}));
  EXPECT_EQ(Player, FlagOf(PlayerEvent{EVENT::PLAYER::Play{}}));
  EXPECT_EQ(Playlist, FlagOf(PlaylistEvent{EVENT::PLAYLIST::Clear{}}));
}

TEST(TestAnnouncementEvents, PlayerTransportData)
{
  using namespace EVENT::PLAYER;
  ExpectData(PlayerEvent{Play{nullptr, 1, VIDEO_AND_AUDIO}},
             R"({"player":{"speed":1,"players":["video","audio"]}})");
  ExpectData(PlayerEvent{AVStart{nullptr, VIDEO}}, R"({"player":{"speed":1,"players":["video"]}})");
  ExpectData(PlayerEvent{AVChange{nullptr, VIDEO}},
             R"({"player":{"speed":1,"players":["video"]}})");
  ExpectData(PlayerEvent{Pause{nullptr, VIDEO}}, R"({"player":{"speed":0,"players":["video"]}})");
  ExpectData(PlayerEvent{Resume{nullptr, VIDEO}}, R"({"player":{"speed":1,"players":["video"]}})");
  ExpectData(PlayerEvent{SpeedChanged{nullptr, 4, VIDEO}},
             R"({"player":{"speed":4,"players":["video"]}})");
  ExpectData(PlayerEvent{Seek{nullptr, 1, 3723004ms, 1000ms, VIDEO}},
             R"({"player":{"speed":1,"players":["video"],
                 "time":{"hours":1,"minutes":2,"seconds":3,"milliseconds":4},
                 "seekoffset":{"hours":0,"minutes":0,"seconds":1,"milliseconds":0}}})");
}

TEST(TestAnnouncementEvents, PlayerStopData)
{
  using namespace EVENT::PLAYER;
  ExpectData(PlayerEvent{Stop{nullptr, false, std::nullopt}}, R"({"end":false})");
  ExpectData(PlayerEvent{Stop{nullptr, true, VIDEO}},
             R"({"end":true,"player":{"players":["video"]}})");
}

TEST(TestAnnouncementEvents, PlaybackFailedData)
{
  using namespace EVENT::PLAYER;
  ExpectData(PlayerEvent{PlaybackFailed{nullptr, PlaybackFailed::Reason::Unplayable}},
             R"({"reason":"unplayable"})");
  ExpectData(PlayerEvent{PlaybackFailed{nullptr, PlaybackFailed::Reason::Unresolved}},
             R"({"reason":"unresolved"})");
  ExpectData(PlayerEvent{PlaybackFailed{nullptr, PlaybackFailed::Reason::Locked}},
             R"({"reason":"locked"})");
  ExpectData(PlayerEvent{PlaybackFailed{nullptr, PlaybackFailed::Reason::Error}},
             R"({"reason":"error"})");
  EXPECT_TRUE(LegacyDataOf(PlayerEvent{PlaybackFailed{}}).isNull());
}

TEST(TestAnnouncementEvents, PlayerPropertiesData)
{
  using namespace EVENT::PLAYER;
  PropertiesChanged volume;
  volume.volume = 42;
  ExpectData(PlayerEvent{volume}, R"({"properties":{"volume":42}})");

  PropertiesChanged muted;
  muted.muted = true;
  ExpectData(PlayerEvent{muted}, R"({"properties":{"muted":true}})");

  PropertiesChanged subtitles;
  subtitles.players = VIDEO_AND_AUDIO;
  subtitles.subtitleEnabled = false;
  ExpectData(PlayerEvent{subtitles},
             R"({"properties":{"subtitleEnabled":false},"player":{"players":["video","audio"]}})");

  PropertiesChanged stream;
  stream.players = VIDEO;
  CVariant audio;
  audio["index"] = 1;
  audio["language"] = "eng";
  stream.currentAudioStream = audio;
  ExpectData(PlayerEvent{stream},
             R"({"properties":{"currentAudioStream":{"index":1,"language":"eng"}},
                 "player":{"players":["video"]}})");

  PropertiesChanged partyMode;
  partyMode.players = VIDEO;
  partyMode.partyMode = true;
  ExpectData(PlayerEvent{partyMode},
             R"({"properties":{"partyMode":true},"player":{"players":["video"]}})");
}

TEST(TestAnnouncementEvents, PlayerOtherData)
{
  using namespace EVENT::PLAYER;
  CVariant geometry;
  geometry["aspect"] = 2.39;
  ExpectData(PlayerEvent{ContentGeometryChange{geometry, VIDEO}},
             R"({"aspect":2.39,"player":{"players":["video"]}})");
  ExpectData(PlayerEvent{Commercial{"01:30"}}, R"("01:30")");
  ExpectData(PlayerEvent{ToggleSkipCommercials{true}}, "true");
  EXPECT_TRUE(LegacyDataOf(PlayerEvent{ProcessInfo{}}).isNull());
  EXPECT_TRUE(LegacyDataOf(PlayerEvent{Menu{}}).isNull());
  EXPECT_TRUE(LegacyDataOf(PlayerEvent{BlurayMenuError{}}).isNull());
  EXPECT_TRUE(LegacyDataOf(PlayerEvent{BlurayEncryptedError{}}).isNull());
}

TEST(TestAnnouncementEvents, PlaylistData)
{
  using namespace EVENT::PLAYLIST;
  ExpectData(PlaylistEvent{Add{"video", 3, nullptr}}, R"({"playlist":"video","position":3})");
  ExpectData(PlaylistEvent{Remove{"audio", 0}}, R"({"playlist":"audio","position":0})");
  ExpectData(PlaylistEvent{Clear{"picture"}}, R"({"playlist":"picture"})");
  ExpectData(PlaylistEvent{PropertiesChanged{"audio", true, std::nullopt}},
             R"({"playlist":"audio","properties":{"shuffled":true}})");
  ExpectData(PlaylistEvent{PropertiesChanged{"video", std::nullopt, "all"}},
             R"({"playlist":"video","properties":{"repeat":"all"}})");
}

TEST(TestAnnouncementEvents, AnItemCanBeReplaced)
{
  const auto original = std::make_shared<CFileItem>("/music/song.flac", false);
  const auto copy = std::make_shared<CFileItem>("/music/song.flac", false);
  const Announcement announcement{PlayerEvent{EVENT::PLAYER::Play{original, 1, VIDEO}}};

  EXPECT_EQ(original, ItemOf(announcement));
  EXPECT_EQ(copy, ItemOf(WithItem(announcement, copy)));
  EXPECT_EQ(nullptr, ItemOf(PlayerEvent{EVENT::PLAYER::Menu{}}));
}
