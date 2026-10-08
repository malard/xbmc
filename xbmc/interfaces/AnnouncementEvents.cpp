/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "AnnouncementEvents.h"

#include "FileItem.h"
#include "interfaces/PlaybackValues.h"
#include "settings/lib/SettingLevel.h"
#include "video/VideoFileItemClassify.h"

#include <type_traits>

namespace ANNOUNCEMENT
{

namespace
{
template<class... Ts>
struct Overloaded : Ts...
{
  using Ts::operator()...;
};

CVariant PlayersOf(KODI::MEDIA::Streams streams)
{
  CVariant list(CVariant::VariantTypeArray);
  if (KODI::MEDIA::HasVideo(streams))
    list.push_back("video");
  if (KODI::MEDIA::HasAudio(streams))
    list.push_back("audio");
  return list;
}

CVariant SpeedAndPlayers(int speed, KODI::MEDIA::Streams streams)
{
  CVariant data;
  data["player"]["speed"] = speed;
  data["player"]["players"] = PlayersOf(streams);
  return data;
}

const char* NameOf(EVENT::INPUT::Requested::Kind kind)
{
  using enum EVENT::INPUT::Requested::Kind;
  switch (kind)
  {
    case Keyboard:
      return "keyboard";
    case Password:
      return "password";
    case Number:
      return "number";
    case NumericPassword:
      return "numericpassword";
    case Date:
      return "date";
    case Time:
      return "time";
    case Seconds:
      return "seconds";
    case IPAddress:
      return "ip";
  }
  return "keyboard";
}

std::string ListNameOf(const std::optional<KODI::PLAYLIST::Type>& list)
{
  return std::string{list ? KODI::PLAYLIST::NameOf(*list) : KODI::PLAYLIST::PICTURE_NAME};
}

CVariant PropertiesOf(const EVENT::PLAYER::PropertiesChanged& changed)
{
  CVariant properties(CVariant::VariantTypeObject);
  if (changed.volume)
    properties["volume"] = *changed.volume;
  if (changed.muted)
    properties["muted"] = *changed.muted;
  if (changed.partyMode)
    properties["partyMode"] = *changed.partyMode;
  if (changed.subtitleEnabled)
    properties["subtitleEnabled"] = *changed.subtitleEnabled;
  if (changed.currentSubtitle)
    properties["currentSubtitle"] = *changed.currentSubtitle;
  if (changed.currentAudioStream)
    properties["currentAudioStream"] = *changed.currentAudioStream;
  if (changed.currentVideoStream)
    properties["currentVideoStream"] = *changed.currentVideoStream;
  return properties;
}

CVariant LegacyDataOfEvent(const PlayerEvent& event)
{
  using namespace EVENT::PLAYER;
  return std::visit(
      Overloaded{[](const Play& e) { return SpeedAndPlayers(e.speed, e.streams); },
                 [](const AVStart& e) { return SpeedAndPlayers(1, e.streams); },
                 [](const AVChange& e) { return SpeedAndPlayers(1, e.streams); },
                 [](const Pause& e) { return SpeedAndPlayers(0, e.streams); },
                 [](const Resume& e) { return SpeedAndPlayers(1, e.streams); },
                 [](const SpeedChanged& e) { return SpeedAndPlayers(e.speed, e.streams); },
                 [](const Seek& e)
                 {
                   CVariant data = SpeedAndPlayers(e.speed, e.streams);
                   KODI::INTERFACES::MillisecondsToTimeObject(static_cast<int>(e.time.count()),
                                                              data["player"]["time"]);
                   KODI::INTERFACES::MillisecondsToTimeObject(
                       static_cast<int>(e.seekOffset.count()), data["player"]["seekoffset"]);
                   return data;
                 },
                 [](const Stop& e)
                 {
                   CVariant data(CVariant::VariantTypeObject);
                   if (e.streams)
                     data["player"]["players"] = PlayersOf(*e.streams);
                   data["end"] = e.end;
                   return data;
                 },
                 [](const PlaybackFailed& e)
                 {
                   if (!e.reason)
                     return CVariant{};
                   CVariant data(CVariant::VariantTypeObject);
                   data["reason"] = std::string{KODI::PLAYLIST::NameOf(*e.reason)};
                   return data;
                 },
                 [](const PropertiesChanged& e)
                 {
                   CVariant data(CVariant::VariantTypeObject);
                   data["properties"] = PropertiesOf(e);
                   if (e.streams)
                     data["player"]["players"] = PlayersOf(*e.streams);
                   return data;
                 },
                 [](const ContentGeometryChange& e)
                 {
                   CVariant data = e.geometry;
                   data["player"]["players"] = PlayersOf(e.streams);
                   return data;
                 },
                 [](const Commercial& e) { return CVariant{e.time}; },
                 [](const ToggleSkipCommercials& e) { return CVariant{e.skip}; },
                 [](const ProcessInfo&) { return CVariant{}; }, [](const Menu&)
                 { return CVariant{}; }, [](const BlurayMenuError&) { return CVariant{}; },
                 [](const BlurayEncryptedError&) { return CVariant{}; },
                 [](const SourceSlow&) { return CVariant{}; }},
      static_cast<const PlayerEvent::variant&>(event));
}

CVariant LegacyDataOfEvent(const PlaylistEvent& event)
{
  using namespace EVENT::PLAYLIST;
  return std::visit(Overloaded{[](const Add& e)
                               {
                                 CVariant data;
                                 data["playlist"] = ListNameOf(e.playList);
                                 data["position"] = e.position;
                                 return data;
                               },
                               [](const Remove& e)
                               {
                                 CVariant data;
                                 data["playlist"] = ListNameOf(e.playList);
                                 data["position"] = e.position;
                                 return data;
                               },
                               [](const Clear& e)
                               {
                                 CVariant data;
                                 data["playlist"] = ListNameOf(e.playList);
                                 return data;
                               },
                               [](const PropertiesChanged& e)
                               {
                                 CVariant data;
                                 data["playlist"] = ListNameOf(e.playList);
                                 if (e.shuffled)
                                   data["properties"]["shuffled"] = *e.shuffled;
                                 if (e.repeat)
                                   data["properties"]["repeat"] =
                                       std::string{KODI::PLAYLIST::NameOf(*e.repeat)};
                                 return data;
                               }},
                    static_cast<const PlaylistEvent::variant&>(event));
}

CVariant LegacyDataOfEvent(const GUIEvent& event)
{
  if (const auto* deactivated = std::get_if<EVENT::GUI::ScreensaverDeactivated>(&event))
  {
    CVariant data(CVariant::VariantTypeObject);
    data["shuttingdown"] = deactivated->shuttingDown;
    return data;
  }
  return CVariant{};
}

CVariant LegacyDataOfEvent(const SystemEvent& event)
{
  if (const auto* quit = std::get_if<EVENT::SYSTEM::Quit>(&event))
  {
    CVariant data(CVariant::VariantTypeObject);
    data["exitcode"] = quit->exitCode;
    return data;
  }
  return CVariant{};
}

CVariant LegacyDataOfEvent(const LibraryEvent& event)
{
  using namespace EVENT::LIBRARY;
  CVariant data;
  if (const auto* update = std::get_if<Update>(&event))
  {
    if (!update->item)
    {
      data["type"] = KODI::MEDIA::NameOf(update->type);
      data["id"] = update->id;
    }
    if (update->transaction)
      data["transaction"] = true;
    if (update->added)
      data["added"] = true;
    if (update->playCount)
      data["playcount"] = *update->playCount;
    if (update->properties)
      data["properties"] = *update->properties;
  }
  else if (const auto* remove = std::get_if<Remove>(&event))
  {
    data["type"] = KODI::MEDIA::NameOf(remove->type);
    data["id"] = remove->id;
    if (remove->transaction)
      data["transaction"] = true;
  }
  else if (const auto* exported = std::get_if<Export>(&event))
  {
    if (exported->root)
      data["root"] = *exported->root;
    if (exported->file)
      data["file"] = *exported->file;
    if (exported->failCount)
      data["failcount"] = *exported->failCount;
  }
  return data;
}

CVariant LegacyDataOfEvent(const InputEvent& event)
{
  CVariant data;
  if (const auto* requested = std::get_if<EVENT::INPUT::Requested>(&event))
  {
    data["type"] = NameOf(requested->kind);
    if (requested->title)
      data["title"] = *requested->title;
    data["value"] = requested->value;
  }
  return data;
}

CVariant LegacyDataOfEvent(const PVREvent& event)
{
  using namespace EVENT::PVR;
  CVariant data(CVariant::VariantTypeObject);
  std::visit(Overloaded{[&data](const RadioTrafficAnnouncement& e) { data["on"] = e.on; },
                        [&data](const RadioClock& e) { data["dateTime"] = e.dateTime; },
                        [&data](const RadioTrafficMessage& e)
                        {
                          data["channel"] = e.channel;
                          data["ident"] = e.ident;
                          data["flags"] = e.flags;
                          data["x"] = e.x;
                          data["y"] = e.y;
                          data["z"] = e.z;
                        }},
             static_cast<const PVREvent::variant&>(event));
  return data;
}

CVariant LegacyDataOfEvent(const InfoEvent&)
{
  return CVariant{};
}

CVariant LegacyDataOfEvent(const SourcesEvent& event)
{
  return std::visit([](const auto& e) { return CVariant{e.path}; },
                    static_cast<const SourcesEvent::variant&>(event));
}

CVariant LegacyDataOfEvent(const SettingsEvent& event)
{
  CVariant data(CVariant::VariantTypeObject);
  data["level"] = SettingLevelToString(std::get<EVENT::SETTINGS::LevelChanged>(event).level);
  return data;
}

} // unnamed namespace

bool EVENT::PLAYER::IsPicture(const CFileItem* item)
{
  return item && !item->HasPVRChannelInfoTag() &&
         !(item->HasVideoInfoTag() && !item->HasPVRRecordingInfoTag()) &&
         !item->HasMusicInfoTag() && !KODI::VIDEO::IsVideo(*item) && item->HasPictureInfoTag();
}

bool IsTransaction(const LibraryEvent& event)
{
  if (const auto* update = std::get_if<EVENT::LIBRARY::Update>(&event))
    return update->transaction;
  if (const auto* remove = std::get_if<EVENT::LIBRARY::Remove>(&event))
    return remove->transaction;
  return false;
}

AnnouncementFlag FlagOf(const Announcement& announcement)
{
  return std::visit([](const auto& flagEvent) { return std::decay_t<decltype(flagEvent)>::FLAG; },
                    static_cast<const Announcement::variant&>(announcement));
}

const char* MessageOf(const Announcement& announcement)
{
  return std::visit(
      [](const auto& flagEvent)
      {
        using FlagEvent = std::decay_t<decltype(flagEvent)>;
        return std::visit([](const auto& event) { return std::decay_t<decltype(event)>::MESSAGE; },
                          static_cast<const typename FlagEvent::variant&>(flagEvent));
      },
      static_cast<const Announcement::variant&>(announcement));
}

std::shared_ptr<const CFileItem> ItemOf(const Announcement& announcement)
{
  return std::visit(
      [](const auto& flagEvent)
      {
        using FlagEvent = std::decay_t<decltype(flagEvent)>;
        return std::visit(
            [](const auto& event) -> std::shared_ptr<const CFileItem>
            {
              if constexpr (requires { event.item; })
                return event.item;
              else
                return nullptr;
            },
            static_cast<const typename FlagEvent::variant&>(flagEvent));
      },
      static_cast<const Announcement::variant&>(announcement));
}

CVariant LegacyDataOf(const Announcement& announcement)
{
  return std::visit([](const auto& event) { return LegacyDataOfEvent(event); },
                    static_cast<const Announcement::variant&>(announcement));
}

Announcement WithItem(Announcement announcement, std::shared_ptr<const CFileItem> item)
{
  std::visit(
      [&item](auto& flagEvent)
      {
        using FlagEvent = std::decay_t<decltype(flagEvent)>;
        std::visit(
            [&item](auto& event)
            {
              if constexpr (requires { event.item; })
                event.item = std::move(item);
            },
            static_cast<typename FlagEvent::variant&>(flagEvent));
      },
      static_cast<Announcement::variant&>(announcement));
  return announcement;
}

} // namespace ANNOUNCEMENT
