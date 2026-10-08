/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "FileItem.h"
#include "interfaces/AnnouncementEvents.h"
#include "settings/lib/SettingLevel.h"
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

TEST(TestAnnouncementEvents, GUIEvents)
{
  using namespace EVENT::GUI;
  EXPECT_EQ(GUI, FlagOf(GUIEvent{SkinLoaded{}}));
  EXPECT_STREQ("OnScreensaverActivated", MessageOf(GUIEvent{ScreensaverActivated{}}));
  EXPECT_STREQ("OnScreensaverDeactivated", MessageOf(GUIEvent{ScreensaverDeactivated{}}));
  EXPECT_STREQ("OnDPMSActivated", MessageOf(GUIEvent{DPMSActivated{}}));
  EXPECT_STREQ("OnDPMSDeactivated", MessageOf(GUIEvent{DPMSDeactivated{}}));
  EXPECT_STREQ("OnSkinUnloading", MessageOf(GUIEvent{SkinUnloading{}}));
  EXPECT_STREQ("OnSkinLoaded", MessageOf(GUIEvent{SkinLoaded{}}));
  EXPECT_STREQ("OnSkinLoadFailed", MessageOf(GUIEvent{SkinLoadFailed{}}));
  EXPECT_STREQ("WindowFocused", MessageOf(GUIEvent{WindowFocused{}}));
  EXPECT_STREQ("WindowUnfocused", MessageOf(GUIEvent{WindowUnfocused{}}));

  ExpectData(GUIEvent{ScreensaverDeactivated{true}}, R"({"shuttingdown":true})");
  ExpectData(GUIEvent{ScreensaverDeactivated{false}}, R"({"shuttingdown":false})");
  EXPECT_TRUE(LegacyDataOf(GUIEvent{ScreensaverActivated{}}).isNull());
  EXPECT_TRUE(LegacyDataOf(GUIEvent{SkinLoaded{}}).isNull());
}

TEST(TestAnnouncementEvents, SystemEvents)
{
  using namespace EVENT::SYSTEM;
  EXPECT_EQ(System, FlagOf(SystemEvent{Wake{}}));
  EXPECT_STREQ("OnQuit", MessageOf(SystemEvent{Quit{}}));
  EXPECT_STREQ("OnRestart", MessageOf(SystemEvent{Restart{}}));
  EXPECT_STREQ("OnSleep", MessageOf(SystemEvent{EVENT::SYSTEM::Sleep{}}));
  EXPECT_STREQ("OnWake", MessageOf(SystemEvent{Wake{}}));
  EXPECT_STREQ("OnLowBattery", MessageOf(SystemEvent{LowBattery{}}));

  ExpectData(SystemEvent{Quit{64}}, R"({"exitcode":64})");
  EXPECT_TRUE(LegacyDataOf(SystemEvent{Restart{}}).isNull());
}

TEST(TestAnnouncementEvents, LibraryEventsKeepTheirMessages)
{
  using namespace EVENT::LIBRARY;
  EXPECT_EQ(VideoLibrary, FlagOf(VideoLibraryEvent{ScanStarted{}}));
  EXPECT_EQ(AudioLibrary, FlagOf(AudioLibraryEvent{ScanStarted{}}));
  EXPECT_STREQ("OnScanStarted", MessageOf(VideoLibraryEvent{ScanStarted{}}));
  EXPECT_STREQ("OnScanFinished", MessageOf(AudioLibraryEvent{ScanFinished{}}));
  EXPECT_STREQ("OnCleanStarted", MessageOf(VideoLibraryEvent{CleanStarted{}}));
  EXPECT_STREQ("OnCleanFinished", MessageOf(AudioLibraryEvent{CleanFinished{}}));
  EXPECT_STREQ("OnUpdate", MessageOf(VideoLibraryEvent{Update{}}));
  EXPECT_STREQ("OnRemove", MessageOf(AudioLibraryEvent{Remove{}}));
  EXPECT_STREQ("OnExport", MessageOf(VideoLibraryEvent{Export{}}));
  EXPECT_STREQ("OnRefresh", MessageOf(VideoLibraryEvent{Refresh{}}));

  EXPECT_TRUE(LegacyDataOf(VideoLibraryEvent{ScanStarted{}}).isNull());
  EXPECT_TRUE(LegacyDataOf(AudioLibraryEvent{CleanFinished{}}).isNull());
  EXPECT_TRUE(LegacyDataOf(VideoLibraryEvent{Refresh{}}).isNull());
}

TEST(TestAnnouncementEvents, LibraryUpdateData)
{
  using namespace EVENT::LIBRARY;
  using KODI::MEDIA::MediaType;
  ExpectData(VideoLibraryEvent{Update{.type = MediaType::MOVIE, .id = 7}},
             R"({"type":"movie","id":7})");
  ExpectData(AudioLibraryEvent{Update{
                 .type = MediaType::SONG, .id = 2, .transaction = true, .added = true}},
             R"({"type":"song","id":2,"transaction":true,"added":true})");
  ExpectData(VideoLibraryEvent{Update{.type = MediaType::NONE, .id = -1}},
             R"({"type":"","id":-1})");

  // An item names itself; the announcement manager adds it to the data.
  const auto item = std::make_shared<CFileItem>("/movies/film.mkv", false);
  ExpectData(VideoLibraryEvent{Update{.item = item, .transaction = true, .playCount = 2}},
             R"({"transaction":true,"playcount":2})");
  ExpectData(VideoLibraryEvent{Update{.item = item, .added = true}}, R"({"added":true})");

  CVariant properties;
  properties["title"] = "Film";
  ExpectData(AudioLibraryEvent{Update{.type = MediaType::ALBUM, .id = 4, .properties = properties}},
             R"({"type":"album","id":4,"properties":{"title":"Film"}})");
}

TEST(TestAnnouncementEvents, LibraryRemoveAndExportData)
{
  using namespace EVENT::LIBRARY;
  using KODI::MEDIA::MediaType;
  ExpectData(VideoLibraryEvent{Remove{MediaType::EPISODE, 5, false}},
             R"({"type":"episode","id":5})");
  ExpectData(AudioLibraryEvent{Remove{MediaType::ARTIST, 9, true}},
             R"({"type":"artist","id":9,"transaction":true})");

  ExpectData(VideoLibraryEvent{Export{"/export/", "/export/videodb.xml", 3}},
             R"({"root":"/export/","file":"/export/videodb.xml","failcount":3})");
  ExpectData(AudioLibraryEvent{Export{.file = "/export/kodi_musicdb.xml"}},
             R"({"file":"/export/kodi_musicdb.xml"})");
  EXPECT_TRUE(LegacyDataOf(VideoLibraryEvent{Export{}}).isNull());
}

TEST(TestAnnouncementEvents, OnlyScansAndCleansAreTransactions)
{
  using namespace EVENT::LIBRARY;
  EXPECT_TRUE(IsTransaction(VideoLibraryEvent{Update{.transaction = true}}));
  EXPECT_TRUE(IsTransaction(AudioLibraryEvent{Remove{.transaction = true}}));
  EXPECT_FALSE(IsTransaction(VideoLibraryEvent{Update{}}));
  EXPECT_FALSE(IsTransaction(VideoLibraryEvent{ScanFinished{}}));
}

TEST(TestAnnouncementEvents, InputEvents)
{
  using namespace EVENT::INPUT;
  using enum Requested::Kind;
  EXPECT_EQ(Input, FlagOf(InputEvent{Finished{}}));
  EXPECT_STREQ("OnInputRequested", MessageOf(InputEvent{Requested{}}));
  EXPECT_STREQ("OnInputFinished", MessageOf(InputEvent{Finished{}}));

  ExpectData(InputEvent{Requested{Keyboard, "Search", "abc"}},
             R"({"type":"keyboard","title":"Search","value":"abc"})");
  ExpectData(InputEvent{Requested{Password, "PIN", ""}},
             R"({"type":"password","title":"PIN","value":""})");
  ExpectData(InputEvent{Requested{NumericPassword, std::nullopt, "12"}},
             R"({"type":"numericpassword","value":"12"})");
  ExpectData(InputEvent{Requested{Number, std::nullopt, "7"}}, R"({"type":"number","value":"7"})");
  ExpectData(InputEvent{Requested{Date, std::nullopt, "01/02/2026"}},
             R"({"type":"date","value":"01/02/2026"})");
  ExpectData(InputEvent{Requested{Time, std::nullopt, "12:30"}},
             R"({"type":"time","value":"12:30"})");
  ExpectData(InputEvent{Requested{Seconds, std::nullopt, "90"}},
             R"({"type":"seconds","value":"90"})");
  ExpectData(InputEvent{Requested{IPAddress, std::nullopt, "10.0.0.1"}},
             R"({"type":"ip","value":"10.0.0.1"})");
  EXPECT_TRUE(LegacyDataOf(InputEvent{Finished{}}).isNull());
}

TEST(TestAnnouncementEvents, PVREvents)
{
  using namespace EVENT::PVR;
  EXPECT_EQ(ANNOUNCEMENT::PVR, FlagOf(PVREvent{RadioClock{}}));
  EXPECT_STREQ("RDSRadioTA", MessageOf(PVREvent{RadioTrafficAnnouncement{}}));
  EXPECT_STREQ("RDSRadioRTC", MessageOf(PVREvent{RadioClock{}}));
  EXPECT_STREQ("RDSRadioTMC", MessageOf(PVREvent{RadioTrafficMessage{}}));

  ExpectData(PVREvent{RadioTrafficAnnouncement{true}}, R"({"on":true})");
  ExpectData(PVREvent{RadioClock{"Thu, 08 Oct 2026 18:00:00 GMT"}},
             R"({"dateTime":"Thu, 08 Oct 2026 18:00:00 GMT"})");
  ExpectData(PVREvent{RadioClock{""}}, R"({"dateTime":""})");
  ExpectData(PVREvent{RadioTrafficMessage{"Radio 1", 0x1234, 0x80, 1, 0x0203, 0x0405}},
             R"({"channel":"Radio 1","ident":4660,"flags":128,"x":1,"y":515,"z":1029})");
}

TEST(TestAnnouncementEvents, InfoSourcesAndSettingsEvents)
{
  EXPECT_EQ(Info, FlagOf(InfoEvent{EVENT::INFO::Changed{}}));
  EXPECT_STREQ("OnChanged", MessageOf(InfoEvent{EVENT::INFO::Changed{}}));
  EXPECT_TRUE(LegacyDataOf(InfoEvent{EVENT::INFO::Changed{}}).isNull());

  using namespace EVENT::SOURCES;
  EXPECT_EQ(Sources, FlagOf(SourcesEvent{Added{}}));
  EXPECT_STREQ("OnAdded", MessageOf(SourcesEvent{Added{}}));
  EXPECT_STREQ("OnRemoved", MessageOf(SourcesEvent{Removed{}}));
  EXPECT_STREQ("OnUpdated", MessageOf(SourcesEvent{Updated{}}));
  ExpectData(SourcesEvent{Added{"upnp://"}}, R"("upnp://")");
  ExpectData(SourcesEvent{Updated{"zeroconf://"}}, R"("zeroconf://")");

  EXPECT_EQ(Settings, FlagOf(SettingsEvent{EVENT::SETTINGS::LevelChanged{}}));
  EXPECT_STREQ("OnLevelChanged", MessageOf(SettingsEvent{EVENT::SETTINGS::LevelChanged{}}));
  ExpectData(SettingsEvent{EVENT::SETTINGS::LevelChanged{SettingLevel::Advanced}},
             R"({"level":"advanced"})");
  ExpectData(SettingsEvent{EVENT::SETTINGS::LevelChanged{SettingLevel::Basic}},
             R"({"level":"basic"})");
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
