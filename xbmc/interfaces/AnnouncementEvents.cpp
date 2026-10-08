/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "AnnouncementEvents.h"

#include "FileItem.h"
#include "interfaces/AnnouncementManager.h"
#include "interfaces/PlaybackValues.h"
#include "music/MusicDatabase.h"
#include "music/tags/MusicInfoTag.h"
#include "pvr/channels/PVRChannel.h"
#include "settings/lib/SettingLevel.h"
#include "utils/DatabaseUtils.h"
#include "utils/StringUtils.h"
#include "utils/log.h"
#include "video/VideoDatabase.h"
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

const char* InputNameOf(const EVENT::INPUT::Requested& requested)
{
  if (!requested.numeric)
    return requested.hidden ? "password" : "keyboard";

  using enum KODI::DIALOGS::NUMERIC_MODE;
  switch (*requested.numeric)
  {
    case TIME:
      return "time";
    case DATE:
      return "date";
    case IP_ADDRESS:
      return "ip";
    case PASSWORD:
      return "numericpassword";
    case NUMBER:
      return "number";
    case TIME_SECONDS:
      break;
  }
  return "seconds";
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

CVariant DataOfEvent(const PlayerEvent& event)
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

CVariant DataOfEvent(const PlaylistEvent& event)
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

CVariant DataOfEvent(const GUIEvent& event)
{
  if (const auto* deactivated = std::get_if<EVENT::GUI::ScreensaverDeactivated>(&event))
  {
    CVariant data(CVariant::VariantTypeObject);
    data["shuttingdown"] = deactivated->shuttingDown;
    return data;
  }
  return CVariant{};
}

CVariant DataOfEvent(const SystemEvent& event)
{
  if (const auto* quit = std::get_if<EVENT::SYSTEM::Quit>(&event))
  {
    CVariant data(CVariant::VariantTypeObject);
    data["exitcode"] = quit->exitCode;
    return data;
  }
  return CVariant{};
}

CVariant DataOfEvent(const LibraryEvent& event)
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

CVariant DataOfEvent(const InputEvent& event)
{
  CVariant data;
  if (const auto* requested = std::get_if<EVENT::INPUT::Requested>(&event))
  {
    data["type"] = InputNameOf(*requested);
    if (requested->title)
      data["title"] = *requested->title;
    data["value"] = requested->value;
  }
  return data;
}

CVariant DataOfEvent(const PVREvent& event)
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

CVariant DataOfEvent(const InfoEvent&)
{
  return CVariant{};
}

CVariant DataOfEvent(const SourcesEvent& event)
{
  return std::visit([](const auto& e) { return CVariant{e.path}; },
                    static_cast<const SourcesEvent::variant&>(event));
}

CVariant DataOfEvent(const SettingsEvent& event)
{
  CVariant data(CVariant::VariantTypeObject);
  data["level"] = SettingLevelToString(std::get<EVENT::SETTINGS::LevelChanged>(event).level);
  return data;
}

CVariant DataOfEvent(const OtherEvent& event)
{
  return event.data;
}

void CopyPVRTagInfoToObject(const PVR::CPVRChannel& channel, CVariant& object)
{
  auto& objItem = object["item"];

  objItem["type"] = "channel";
  objItem["title"] = channel.ChannelName();
  objItem["channelType"] = channel.IsRadio() ? "radio" : "tv";

  objItem["id"] = channel.ChannelID();
}

void CopyVideoTagInfoToObject(const CFileItem& item, CVariant& object)
{
  const CVideoInfoTag& tag = *item.GetVideoInfoTag();

  auto& objItem = object["item"];
  const int id = tag.GetDatabaseId();

  if (!tag.m_type.empty())
    objItem["type"] = tag.m_type;
  else
    objItem["type"] =
        NameOf(DatabaseUtils::MediaTypeFromVideoContentType(item.GetVideoContentType()));

  if (id <= 0)
  {
    std::string title = tag.m_strTitle;
    if (title.empty())
      title = item.GetLabel();
    objItem["title"] = title;

    using enum VideoDbContentType;
    switch (item.GetVideoContentType())
    {
      case MOVIES:
        if (tag.HasYear())
          objItem["year"] = tag.GetYear();
        break;
      case EPISODES:
        if (tag.m_iEpisode >= 0)
          objItem["episode"] = tag.m_iEpisode;
        if (tag.m_iSeason >= 0)
          objItem["season"] = tag.m_iSeason;
        if (!tag.m_strShowTitle.empty())
          objItem["showTitle"] = tag.m_strShowTitle;
        break;
      case MUSICVIDEOS:
        if (!tag.m_strAlbum.empty())
          objItem["album"] = tag.m_strAlbum;
        if (!tag.m_artist.empty())
          objItem["artist"] = StringUtils::Join(tag.m_artist, " / ");
        break;
      default:
        break;
    }
  }
  else
  {
    objItem["id"] = id;
  }
}

void CopyMusicTagInfoToObject(const CFileItem& item, CVariant& object)
{
  const MUSIC_INFO::CMusicInfoTag& tag = *item.GetMusicInfoTag();

  auto& objItem = object["item"];
  const int id = tag.GetDatabaseId();
  objItem["type"] = KODI::MEDIA::NameOf(KODI::MEDIA::TYPE::SONG);

  if (id <= 0)
  {
    objItem["title"] = tag.GetTitle();
    if (objItem["title"].empty())
      objItem["title"] = item.GetLabel();

    if (tag.GetTrackNumber() > 0)
      objItem["track"] = tag.GetTrackNumber();
    if (!tag.GetAlbum().empty())
      objItem["album"] = tag.GetAlbum();
    if (!tag.GetArtist().empty())
      objItem["artist"] = tag.GetArtist();
  }
  else
  {
    objItem["id"] = id;
  }
}

CVariant CreateDataObjectFromItem(const CFileItem& item, const CVariant& data)
{
  CVariant object;
  if (data.isNull() || data.isObject())
    object = data;
  else
    object = CVariant::VariantTypeObject;

  if (item.HasPVRChannelInfoTag())
  {
    CopyPVRTagInfoToObject(*item.GetPVRChannelInfoTag(), object);
  }
  else if (item.HasVideoInfoTag() && !item.HasPVRRecordingInfoTag())
  {
    CopyVideoTagInfoToObject(item, object);
  }
  else if (item.HasMusicInfoTag())
  {
    CopyMusicTagInfoToObject(item, object);
  }
  else if (KODI::VIDEO::IsVideo(item))
  {
    // video item but has no video info tag.
    object["item"]["type"] = "movie";
    object["item"]["title"] = item.GetLabel();
  }
  else if (item.HasPictureInfoTag())
  {
    object["item"]["type"] = "picture";
    object["item"]["file"] = item.GetPath();
  }
  else
    object["item"]["type"] = "unknown";

  return object;
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
      [](const auto& flagEvent) -> const char*
      {
        using FlagEvent = std::decay_t<decltype(flagEvent)>;
        if constexpr (std::is_same_v<FlagEvent, OtherEvent>)
          return flagEvent.message.c_str();
        else
          return std::visit([](const auto& event)
                            { return std::decay_t<decltype(event)>::MESSAGE; },
                            static_cast<const typename FlagEvent::variant&>(flagEvent));
      },
      static_cast<const Announcement::variant&>(announcement));
}

const std::string& SenderOf(const Announcement& announcement)
{
  if (const auto* other = std::get_if<OtherEvent>(&announcement))
    return other->sender;
  return CAnnouncementManager::ANNOUNCEMENT_SENDER;
}

std::shared_ptr<const CFileItem> ItemOf(const Announcement& announcement)
{
  return std::visit(
      [](const auto& flagEvent) -> std::shared_ptr<const CFileItem>
      {
        using FlagEvent = std::decay_t<decltype(flagEvent)>;
        if constexpr (std::is_same_v<FlagEvent, OtherEvent>)
          return nullptr;
        else
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

CVariant EventDataOf(const Announcement& announcement)
{
  return std::visit([](const auto& event) { return DataOfEvent(event); },
                    static_cast<const Announcement::variant&>(announcement));
}

CVariant NotificationDataOf(const Announcement& announcement)
{
  const std::shared_ptr<const CFileItem> item = ItemOf(announcement);
  return item ? CreateDataObjectFromItem(*item, EventDataOf(announcement))
              : EventDataOf(announcement);
}

Announcement WithLibraryDetails(Announcement announcement)
{
  const std::shared_ptr<const CFileItem> item = ItemOf(announcement);
  if (!item || item->GetPath().empty() || item->HasPVRChannelInfoTag())
    return announcement;

  //! @todo Can be removed once this is properly handled when starting playback of a file
  if (item->HasVideoInfoTag() && !item->HasPVRRecordingInfoTag())
  {
    if (item->GetVideoInfoTag()->GetDatabaseId() > 0)
      return announcement;

    CVideoDatabase videodatabase;
    if (!videodatabase.Open())
    {
      CLog::LogFC(LOGWARNING, LOGANNOUNCE,
                  "Unable to open video database. Can not load video tag for announcement!");
      return announcement;
    }
    CVideoInfoTag tag = *item->GetVideoInfoTag();
    const std::string path = StringUtils::StartsWith(tag.m_strFileNameAndPath, "removable://")
                                 ? tag.m_strFileNameAndPath
                                 : item->GetPath();
    if (!videodatabase.LoadVideoInfo(path, tag))
      return announcement;

    auto loaded = std::make_shared<CFileItem>(*item);
    *loaded->GetVideoInfoTag() = std::move(tag);
    return WithItem(std::move(announcement), std::move(loaded));
  }

  if (item->HasMusicInfoTag())
  {
    if (item->GetMusicInfoTag()->GetDatabaseId() > 0)
      return announcement;

    CMusicDatabase musicdatabase;
    if (!musicdatabase.Open())
    {
      CLog::LogFC(LOGWARNING, LOGANNOUNCE,
                  "Unable to open music database. Can not load song tag for announcement!");
      return announcement;
    }
    CSong song;
    if (!musicdatabase.GetSongByFileName(item->GetPath(), song, item->GetStartOffset()))
      return announcement;

    auto loaded = std::make_shared<CFileItem>(*item);
    loaded->GetMusicInfoTag()->SetSong(song);
    return WithItem(std::move(announcement), std::move(loaded));
  }

  return announcement;
}

Announcement WithItem(Announcement announcement, std::shared_ptr<const CFileItem> item)
{
  std::visit(
      [&item](auto& flagEvent)
      {
        using FlagEvent = std::decay_t<decltype(flagEvent)>;
        if constexpr (!std::is_same_v<FlagEvent, OtherEvent>)
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
