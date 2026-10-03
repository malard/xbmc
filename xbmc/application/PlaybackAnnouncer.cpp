/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "PlaybackAnnouncer.h"

#include "FileItem.h"
#include "GUIUserMessages.h"
#include "ServiceBroker.h"
#include "application/ApplicationComponents.h"
#include "application/ApplicationPlayer.h"
#include "guilib/GUIMessage.h"
#include "interfaces/AnnouncementManager.h"
#include "interfaces/PlaybackValues.h"
#include "pictures/PictureInfoTag.h"
#include "playlists/PlayList.h"
#include "pvr/channels/PVRChannel.h"
#include "utils/Variant.h"
#include "video/VideoFileItemClassify.h"

#include <utility>

using namespace KODI;
using namespace KODI::PLAYLIST;

namespace
{
std::string PropertyName(CPlaybackAnnouncer::PlayerProperty property)
{
  using enum CPlaybackAnnouncer::PlayerProperty;
  switch (property)
  {
    case PartyMode:
      return "partyMode";
    case Shuffled:
      return "shuffled";
    case Repeat:
      return "repeat";
    case SubtitleEnabled:
      return "subtitleEnabled";
    case CurrentSubtitle:
      return "currentSubtitle";
    case CurrentAudioStream:
      return "currentAudioStream";
    case CurrentVideoStream:
      return "currentVideoStream";
  }
  return {};
}

CVariant Speed(int speed)
{
  CVariant data;
  data["player"]["speed"] = speed;
  return data;
}

CVariant SlideShowPlayers()
{
  return CVariant(std::vector<std::string>{"video"});
}

void PublishToAnnouncementManager(ANNOUNCEMENT::AnnouncementFlag flag,
                                  const std::string& message,
                                  const std::shared_ptr<const CFileItem>& item,
                                  const CVariant& data)
{
  const auto announcer = CServiceBroker::GetAnnouncementManager();
  if (announcer)
    announcer->Announce(flag, message, item, data);
}
} // namespace

CPlaybackAnnouncer::CPlaybackAnnouncer(std::shared_ptr<CApplicationPlayLists> playLists,
                                       Sink sink /* = {} */)
  : m_playLists(std::move(playLists)),
    m_sink(sink ? std::move(sink) : Sink{PublishToAnnouncementManager})
{
  m_playLists->SetObserver(this);
}

CPlaybackAnnouncer::~CPlaybackAnnouncer()
{
  m_playLists->SetObserver(nullptr);
}

bool CPlaybackAnnouncer::OnMessage(CGUIMessage& message)
{
  const std::shared_ptr<const CFileItem> item = m_playLists->GetCurrentItem();
  switch (message.GetMessage())
  {
    case GUI_MSG_PLAYBACK_AVSTARTED:
      Publish("OnAVStart", item, Speed(1));
      break;
    case GUI_MSG_PLAYBACK_AVCHANGE:
      Publish("OnAVChange", item, Speed(1));
      break;
    case GUI_MSG_PLAYBACK_PAUSED:
      Publish("OnPause", item, Speed(0));
      break;
    case GUI_MSG_PLAYBACK_RESUMED:
      Publish("OnResume", item, Speed(1));
      break;
    case GUI_MSG_PLAYBACK_SPEED_CHANGED:
      Publish("OnSpeedChanged", item, Speed(message.GetParam1()));
      break;
    case GUI_MSG_PLAYBACK_SEEKED:
    {
      const auto appPlayer = CServiceBroker::GetAppComponents().GetComponent<CApplicationPlayer>();
      CVariant data = Speed(static_cast<int>(appPlayer->GetPlaySpeed()));
      INTERFACES::MillisecondsToTimeObject(static_cast<int>(message.GetParam1AsI64()),
                                           data["player"]["time"]);
      INTERFACES::MillisecondsToTimeObject(static_cast<int>(message.GetParam2AsI64()),
                                           data["player"]["seekoffset"]);
      Publish("OnSeek", item, data);
      break;
    }
    case GUI_MSG_PLAYBACK_STOPPED:
    case GUI_MSG_PLAYBACK_ENDED:
    {
      CVariant data(CVariant::VariantTypeObject);
      data["end"] = message.GetMessage() == GUI_MSG_PLAYBACK_ENDED;
      m_sink(ANNOUNCEMENT::Player, "OnStop", item, data);
      break;
    }
    default:
      break;
  }
  return false;
}

void CPlaybackAnnouncer::OnStarted(const std::shared_ptr<CFileItem>& started)
{
  if (!started)
    return;
  CVariant data = Speed(1);
  data["player"]["players"] = GetPlayers(started.get(), true);
  m_sink(ANNOUNCEMENT::Player, "OnPlay", started, data);
}

CVariant CPlaybackAnnouncer::GetPlayers(const CFileItem* item, bool claimed) const
{
  bool video = false;
  bool audio = false;
  const auto appPlayer = CServiceBroker::GetAppComponents().GetComponent<CApplicationPlayer>();
  if (!claimed && (appPlayer->HasVideo() || appPlayer->HasAudio()))
  {
    video = appPlayer->HasVideo();
    audio = appPlayer->HasAudio();
  }
  else if (item && item->HasPVRChannelInfoTag())
  {
    audio = true;
    video = !item->GetPVRChannelInfoTag()->IsRadio();
  }
  else if (const std::optional<Type> type = m_playLists->GetPlayingType(); type)
  {
    const Holds holds = m_playLists->GetHolds(*type, m_playLists->GetPlayList(*type).GetCurrent());
    video = holds != Holds::Audio;
    audio = holds != Holds::Video;
  }
  else
  {
    audio = true;
    video = !m_playLists->IsPlayingAsAudio();
  }

  CVariant players(CVariant::VariantTypeArray);
  if (video)
    players.push_back("video");
  if (audio)
    players.push_back("audio");
  return players;
}

void CPlaybackAnnouncer::Announce(PlayerProperty property, const CVariant& value) const
{
  if (!CServiceBroker::GetAppComponents().GetComponent<CApplicationPlayer>()->IsPlaying())
    return;
  PublishProperty(GetPlayers(nullptr, false), property, value);
}

void CPlaybackAnnouncer::OnSlideShow(SlideShowEvent event,
                                     const std::shared_ptr<const CFileItem>& slide,
                                     bool running)
{
  CVariant data;
  data["player"]["players"] = SlideShowPlayers();
  std::string message;
  switch (event)
  {
    case SlideShowEvent::Play:
      m_playLists->SetSlideShowRunning(true);
      data["player"]["speed"] = running ? 1 : 0;
      message = "OnPlay";
      break;
    case SlideShowEvent::Pause:
      m_playLists->SetSlideShowRunning(true);
      data["player"]["speed"] = 0;
      message = "OnPause";
      break;
    case SlideShowEvent::Stop:
      m_playLists->SetSlideShowRunning(false);
      data["end"] = true;
      message = "OnStop";
      break;
  }
  // a slide is announced as a picture even before its tag has been read
  std::shared_ptr<const CFileItem> announced = slide;
  if (slide && !slide->HasPictureInfoTag() && !VIDEO::IsVideo(*slide))
  {
    auto tagged = std::make_shared<CFileItem>(*slide);
    tagged->GetPictureInfoTag();
    announced = std::move(tagged);
  }
  m_sink(ANNOUNCEMENT::Player, message, announced, data);
}

void CPlaybackAnnouncer::OnContentGeometryChanged(CVariant data) const
{
  Publish("OnContentGeometryChange", nullptr, std::move(data));
}

void CPlaybackAnnouncer::OnSlideShowShuffled() const
{
  PublishPlayListProperty("picture", PlayerProperty::Shuffled, true);
}

void CPlaybackAnnouncer::OnSlideShowListChanged(const PlayListChange& change) const
{
  PublishListChange("picture", change);
}

void CPlaybackAnnouncer::OnListChanged(Type type, const PlayListChange& change)
{
  PublishListChange(PLAYLIST::NameOf(type), change);
}

void CPlaybackAnnouncer::OnShuffled(Type type, bool shuffled)
{
  PublishPlayListProperty(PLAYLIST::NameOf(type), PlayerProperty::Shuffled, shuffled);
}

void CPlaybackAnnouncer::OnFeed(bool playing)
{
  // what skins and remotes call party mode
  Announce(PlayerProperty::PartyMode, playing);
}

void CPlaybackAnnouncer::OnFailed(const std::shared_ptr<const CFileItem>& item,
                                  CApplicationPlayLists::FailReason reason)
{
  using enum CApplicationPlayLists::FailReason;
  CVariant data{CVariant::VariantTypeObject};
  switch (reason)
  {
    case Unplayable:
      data["reason"] = "unplayable";
      break;
    case Unresolved:
      data["reason"] = "unresolved";
      break;
    case Locked:
      data["reason"] = "locked";
      break;
    case Error:
      data["reason"] = "error";
      break;
  }
  m_sink(ANNOUNCEMENT::Player, "OnPlaybackFailed", item, data);
}

void CPlaybackAnnouncer::OnRepeat(Type type, CApplicationPlayLists::Repeat repeat)
{
  const std::string name{CApplicationPlayLists::RepeatName(repeat)};
  PublishPlayListProperty(PLAYLIST::NameOf(type), PlayerProperty::Repeat, name);
}

void CPlaybackAnnouncer::Publish(const std::string& message,
                                 const std::shared_ptr<const CFileItem>& item,
                                 CVariant data) const
{
  data["player"]["players"] = GetPlayers(item.get(), false);
  m_sink(ANNOUNCEMENT::Player, message, item, data);
}

void CPlaybackAnnouncer::PublishProperty(const CVariant& players,
                                         PlayerProperty property,
                                         const CVariant& value) const
{
  if (value.isNull())
    return;
  CVariant data;
  data["properties"][PropertyName(property)] = value;
  data["player"]["players"] = players;
  m_sink(ANNOUNCEMENT::Player, "OnPropertiesChanged", nullptr, data);
}

void CPlaybackAnnouncer::PublishPlayListProperty(std::string_view playList,
                                                 PlayerProperty property,
                                                 const CVariant& value) const
{
  CVariant data;
  data["playlist"] = std::string{playList};
  data["properties"][PropertyName(property)] = value;
  m_sink(ANNOUNCEMENT::Playlist, "OnPropertiesChanged", nullptr, data);
}

void CPlaybackAnnouncer::PublishListChange(std::string_view playList,
                                           const PlayListChange& change) const
{
  CVariant data;
  data["playlist"] = std::string{playList};
  switch (change.type)
  {
    case PlayListChange::Type::Added:
      data["position"] = change.position;
      m_sink(ANNOUNCEMENT::Playlist, "OnAdd", change.item, data);
      break;
    case PlayListChange::Type::Removed:
      data["position"] = change.position;
      m_sink(ANNOUNCEMENT::Playlist, "OnRemove", nullptr, data);
      break;
    case PlayListChange::Type::Cleared:
      m_sink(ANNOUNCEMENT::Playlist, "OnClear", nullptr, data);
      break;
    case PlayListChange::Type::Moved:
    case PlayListChange::Type::Shuffled:
    case PlayListChange::Type::Repeat:
    case PlayListChange::Type::Wrap:
    case PlayListChange::Type::Current:
    case PlayListChange::Type::Feed:
      break;
  }
}
