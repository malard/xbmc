/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "AnnouncementEvents.h"

#include "FileItem.h"
#include "interfaces/AnnouncementMessages.h"
#include "interfaces/PlaybackValues.h"
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

CVariant PlayersOf(const EVENT::PLAYER::Players& players)
{
  CVariant list(CVariant::VariantTypeArray);
  if (players.video)
    list.push_back("video");
  if (players.audio)
    list.push_back("audio");
  return list;
}

CVariant SpeedAndPlayers(int speed, const EVENT::PLAYER::Players& players)
{
  CVariant data;
  data["player"]["speed"] = speed;
  data["player"]["players"] = PlayersOf(players);
  return data;
}

const char* ReasonOf(EVENT::PLAYER::PlaybackFailed::Reason reason)
{
  using enum EVENT::PLAYER::PlaybackFailed::Reason;
  switch (reason)
  {
    case Unplayable:
      return "unplayable";
    case Unresolved:
      return "unresolved";
    case Locked:
      return "locked";
    case Error:
      return "error";
    case None:
      break;
  }
  return nullptr;
}

const char* MessageOfPlayer(const PlayerEvent& event)
{
  using namespace EVENT::PLAYER;
  return std::visit(
      Overloaded{[](const Play&) { return MESSAGE::ON_PLAY; },
                 [](const AVStart&) { return MESSAGE::ON_AV_START; }, [](const AVChange&)
                 { return MESSAGE::ON_AV_CHANGE; }, [](const Pause&) { return MESSAGE::ON_PAUSE; },
                 [](const Resume&) { return MESSAGE::ON_RESUME; },
                 [](const SpeedChanged&) { return MESSAGE::ON_SPEED_CHANGED; }, [](const Seek&)
                 { return MESSAGE::ON_SEEK; }, [](const Stop&) { return MESSAGE::ON_STOP; },
                 [](const PlaybackFailed&) { return MESSAGE::ON_PLAYBACK_FAILED; },
                 [](const PropertiesChanged&) { return MESSAGE::ON_PROPERTIES_CHANGED; },
                 [](const ContentGeometryChange&) { return MESSAGE::ON_CONTENT_GEOMETRY_CHANGE; },
                 [](const Commercial&) { return MESSAGE::ON_COMMERCIAL; },
                 [](const ToggleSkipCommercials&) { return MESSAGE::ON_TOGGLE_SKIP_COMMERCIALS; },
                 [](const ProcessInfo&) { return MESSAGE::ON_PROCESS_INFO; },
                 [](const Menu&) { return MESSAGE::ON_MENU; },
                 [](const BlurayMenuError&) { return MESSAGE::ON_BLURAY_MENU_ERROR; },
                 [](const BlurayEncryptedError&) { return MESSAGE::ON_BLURAY_ENCRYPTED_ERROR; },
                 [](const SourceSlow&) { return MESSAGE::SOURCE_SLOW; }},
      static_cast<const PlayerEvent::variant&>(event));
}

const char* MessageOfPlaylist(const PlaylistEvent& event)
{
  using namespace EVENT::PLAYLIST;
  return std::visit(Overloaded{[](const Add&) { return MESSAGE::ON_ADD; },
                               [](const Remove&) { return MESSAGE::ON_REMOVE; }, [](const Clear&)
                               { return MESSAGE::ON_CLEAR; }, [](const PropertiesChanged&)
                               { return MESSAGE::ON_PROPERTIES_CHANGED; }},
                    static_cast<const PlaylistEvent::variant&>(event));
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
  if (changed.shuffled)
    properties["shuffled"] = *changed.shuffled;
  if (changed.repeat)
    properties["repeat"] = *changed.repeat;
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

CVariant LegacyDataOfPlayer(const PlayerEvent& event)
{
  using namespace EVENT::PLAYER;
  return std::visit(
      Overloaded{[](const Play& e) { return SpeedAndPlayers(e.speed, e.players); },
                 [](const AVStart& e) { return SpeedAndPlayers(1, e.players); },
                 [](const AVChange& e) { return SpeedAndPlayers(1, e.players); },
                 [](const Pause& e) { return SpeedAndPlayers(0, e.players); },
                 [](const Resume& e) { return SpeedAndPlayers(1, e.players); },
                 [](const SpeedChanged& e) { return SpeedAndPlayers(e.speed, e.players); },
                 [](const Seek& e)
                 {
                   CVariant data = SpeedAndPlayers(e.speed, e.players);
                   KODI::INTERFACES::MillisecondsToTimeObject(static_cast<int>(e.time.count()),
                                                              data["player"]["time"]);
                   KODI::INTERFACES::MillisecondsToTimeObject(
                       static_cast<int>(e.seekOffset.count()), data["player"]["seekoffset"]);
                   return data;
                 },
                 [](const Stop& e)
                 {
                   CVariant data(CVariant::VariantTypeObject);
                   if (e.players)
                     data["player"]["players"] = PlayersOf(*e.players);
                   data["end"] = e.end;
                   return data;
                 },
                 [](const PlaybackFailed& e)
                 {
                   const char* reason = ReasonOf(e.reason);
                   if (!reason)
                     return CVariant{};
                   CVariant data(CVariant::VariantTypeObject);
                   data["reason"] = reason;
                   return data;
                 },
                 [](const PropertiesChanged& e)
                 {
                   CVariant data(CVariant::VariantTypeObject);
                   data["properties"] = PropertiesOf(e);
                   if (e.players)
                     data["player"]["players"] = PlayersOf(*e.players);
                   return data;
                 },
                 [](const ContentGeometryChange& e)
                 {
                   CVariant data = e.geometry;
                   data["player"]["players"] = PlayersOf(e.players);
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

CVariant LegacyDataOfPlaylist(const PlaylistEvent& event)
{
  using namespace EVENT::PLAYLIST;
  return std::visit(Overloaded{[](const Add& e)
                               {
                                 CVariant data;
                                 data["playlist"] = e.playList;
                                 data["position"] = e.position;
                                 return data;
                               },
                               [](const Remove& e)
                               {
                                 CVariant data;
                                 data["playlist"] = e.playList;
                                 data["position"] = e.position;
                                 return data;
                               },
                               [](const Clear& e)
                               {
                                 CVariant data;
                                 data["playlist"] = e.playList;
                                 return data;
                               },
                               [](const PropertiesChanged& e)
                               {
                                 CVariant data;
                                 data["playlist"] = e.playList;
                                 if (e.shuffled)
                                   data["properties"]["shuffled"] = *e.shuffled;
                                 if (e.repeat)
                                   data["properties"]["repeat"] = *e.repeat;
                                 return data;
                               }},
                    static_cast<const PlaylistEvent::variant&>(event));
}

} // unnamed namespace

bool EVENT::PLAYER::IsPicture(const CFileItem* item)
{
  return item && !item->HasPVRChannelInfoTag() &&
         !(item->HasVideoInfoTag() && !item->HasPVRRecordingInfoTag()) &&
         !item->HasMusicInfoTag() && !KODI::VIDEO::IsVideo(*item) && item->HasPictureInfoTag();
}

AnnouncementFlag FlagOf(const Announcement& announcement)
{
  return std::visit(Overloaded{[](const PlayerEvent&) { return Player; },
                               [](const PlaylistEvent&) { return Playlist; }},
                    static_cast<const Announcement::variant&>(announcement));
}

const char* MessageOf(const Announcement& announcement)
{
  return std::visit(Overloaded{[](const PlayerEvent& e) { return MessageOfPlayer(e); },
                               [](const PlaylistEvent& e) { return MessageOfPlaylist(e); }},
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
  return std::visit(Overloaded{[](const PlayerEvent& e) { return LegacyDataOfPlayer(e); },
                               [](const PlaylistEvent& e) { return LegacyDataOfPlaylist(e); }},
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
