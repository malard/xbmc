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
#include "pictures/PictureInfoTag.h"
#include "playlists/PlayList.h"
#include "pvr/channels/PVRChannel.h"
#include "utils/Variant.h"
#include "video/VideoFileItemClassify.h"

#include <chrono>
#include <utility>

using namespace KODI;
using namespace KODI::PLAYLIST;
using ANNOUNCEMENT::PlayerEvent;
using ANNOUNCEMENT::PlaylistEvent;
using ANNOUNCEMENT::EVENT::PLAYER::Players;

namespace
{
constexpr Players SLIDESHOW_PLAYERS{.video = true, .audio = false};

void PublishToAnnouncementManager(const ANNOUNCEMENT::Announcement& announcement)
{
  const auto announcer = CServiceBroker::GetAnnouncementManager();
  if (announcer)
    announcer->Announce(announcement);
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
  using namespace ANNOUNCEMENT::EVENT::PLAYER;
  const std::shared_ptr<const CFileItem> item = m_playLists->GetCurrentItem();
  switch (message.GetMessage())
  {
    case GUI_MSG_PLAYBACK_AVSTARTED:
      m_sink(PlayerEvent{AVStart{item, GetPlayers(item.get(), false)}});
      break;
    case GUI_MSG_PLAYBACK_AVCHANGE:
      m_sink(PlayerEvent{AVChange{item, GetPlayers(item.get(), false)}});
      break;
    case GUI_MSG_PLAYBACK_PAUSED:
      m_sink(PlayerEvent{Pause{item, GetPlayers(item.get(), false)}});
      break;
    case GUI_MSG_PLAYBACK_RESUMED:
      m_sink(PlayerEvent{Resume{item, GetPlayers(item.get(), false)}});
      break;
    case GUI_MSG_PLAYBACK_SPEED_CHANGED:
      m_sink(PlayerEvent{SpeedChanged{item, message.GetParam1(), GetPlayers(item.get(), false)}});
      break;
    case GUI_MSG_PLAYBACK_SEEKED:
    {
      const auto appPlayer = CServiceBroker::GetAppComponents().GetComponent<CApplicationPlayer>();
      m_sink(PlayerEvent{Seek{item, static_cast<int>(appPlayer->GetPlaySpeed()),
                              std::chrono::milliseconds{message.GetParam1AsI64()},
                              std::chrono::milliseconds{message.GetParam2AsI64()},
                              GetPlayers(item.get(), false)}});
      break;
    }
    case GUI_MSG_PLAYBACK_STOPPED:
    case GUI_MSG_PLAYBACK_ENDED:
      m_sink(PlayerEvent{Stop{item, message.GetMessage() == GUI_MSG_PLAYBACK_ENDED, std::nullopt}});
      break;
    default:
      break;
  }
  return false;
}

void CPlaybackAnnouncer::OnStarted(const std::shared_ptr<CFileItem>& started)
{
  if (!started)
    return;
  m_sink(
      PlayerEvent{ANNOUNCEMENT::EVENT::PLAYER::Play{started, 1, GetPlayers(started.get(), true)}});
}

Players CPlaybackAnnouncer::GetPlayers(const CFileItem* item, bool claimed) const
{
  Players players;
  const auto appPlayer = CServiceBroker::GetAppComponents().GetComponent<CApplicationPlayer>();
  if (!claimed && (appPlayer->HasVideo() || appPlayer->HasAudio()))
  {
    players.video = appPlayer->HasVideo();
    players.audio = appPlayer->HasAudio();
  }
  else if (item && item->HasPVRChannelInfoTag())
  {
    players.audio = true;
    players.video = !item->GetPVRChannelInfoTag()->IsRadio();
  }
  else if (const std::optional<Holds> holds = m_playLists->GetPlayingHolds(); holds)
  {
    players.video = *holds != Holds::Audio;
    players.audio = *holds != Holds::Video;
  }
  else
  {
    players.audio = true;
    players.video = !m_playLists->IsPlayingAsAudio();
  }
  return players;
}

void CPlaybackAnnouncer::Announce(PlayerProperty property, const CVariant& value) const
{
  if (!CServiceBroker::GetAppComponents().GetComponent<CApplicationPlayer>()->IsPlaying())
    return;
  if (value.isNull())
    return;

  ANNOUNCEMENT::EVENT::PLAYER::PropertiesChanged changed;
  changed.players = GetPlayers(nullptr, false);
  using enum PlayerProperty;
  switch (property)
  {
    case PartyMode:
      changed.partyMode = value.asBoolean();
      break;
    case Shuffled:
      changed.shuffled = value.asBoolean();
      break;
    case Repeat:
      changed.repeat = value.asString();
      break;
    case SubtitleEnabled:
      changed.subtitleEnabled = value.asBoolean();
      break;
    case CurrentSubtitle:
      changed.currentSubtitle = value;
      break;
    case CurrentAudioStream:
      changed.currentAudioStream = value;
      break;
    case CurrentVideoStream:
      changed.currentVideoStream = value;
      break;
  }
  m_sink(PlayerEvent{std::move(changed)});
}

void CPlaybackAnnouncer::OnSlideShow(SlideShowEvent event,
                                     const std::shared_ptr<const CFileItem>& slide,
                                     bool running)
{
  // a slide is announced as a picture even before its tag has been read
  std::shared_ptr<const CFileItem> announced = slide;
  if (slide && !slide->HasPictureInfoTag() && !VIDEO::IsVideo(*slide))
  {
    auto tagged = std::make_shared<CFileItem>(*slide);
    tagged->GetPictureInfoTag();
    announced = std::move(tagged);
  }

  using namespace ANNOUNCEMENT::EVENT::PLAYER;
  switch (event)
  {
    case SlideShowEvent::Play:
      m_playLists->SetSlideShowRunning(true);
      m_sink(PlayerEvent{Play{announced, running ? 1 : 0, SLIDESHOW_PLAYERS}});
      break;
    case SlideShowEvent::Pause:
      m_playLists->SetSlideShowRunning(true);
      m_sink(PlayerEvent{Pause{announced, SLIDESHOW_PLAYERS}});
      break;
    case SlideShowEvent::Stop:
      m_playLists->SetSlideShowRunning(false);
      m_sink(PlayerEvent{Stop{announced, true, SLIDESHOW_PLAYERS}});
      break;
  }
}

void CPlaybackAnnouncer::OnContentGeometryChanged(CVariant data) const
{
  m_sink(PlayerEvent{ANNOUNCEMENT::EVENT::PLAYER::ContentGeometryChange{
      std::move(data), GetPlayers(nullptr, false)}});
}

void CPlaybackAnnouncer::OnSlideShowShuffled() const
{
  m_sink(PlaylistEvent{ANNOUNCEMENT::EVENT::PLAYLIST::PropertiesChanged{
      std::string{PLAYLIST::PICTURE_NAME}, true, std::nullopt}});
}

void CPlaybackAnnouncer::OnSlideShowListChanged(const PlayListChange& change) const
{
  PublishListChange(PLAYLIST::PICTURE_NAME, change);
}

void CPlaybackAnnouncer::OnListChanged(Type type, const PlayListChange& change)
{
  PublishListChange(PLAYLIST::NameOf(type), change);
}

void CPlaybackAnnouncer::OnShuffled(Type type, bool shuffled)
{
  m_sink(PlaylistEvent{ANNOUNCEMENT::EVENT::PLAYLIST::PropertiesChanged{
      std::string{PLAYLIST::NameOf(type)}, shuffled, std::nullopt}});
}

void CPlaybackAnnouncer::OnFeed(bool playing)
{
  // what skins and remotes call party mode
  Announce(PlayerProperty::PartyMode, playing);
}

void CPlaybackAnnouncer::OnFailed(const std::shared_ptr<const CFileItem>& item,
                                  CApplicationPlayLists::FailReason reason)
{
  using Reason = ANNOUNCEMENT::EVENT::PLAYER::PlaybackFailed::Reason;
  Reason announced = Reason::None;
  switch (reason)
  {
    using enum CApplicationPlayLists::FailReason;
    case Unplayable:
      announced = Reason::Unplayable;
      break;
    case Unresolved:
      announced = Reason::Unresolved;
      break;
    case Locked:
      announced = Reason::Locked;
      break;
    case Error:
      announced = Reason::Error;
      break;
  }
  m_sink(PlayerEvent{ANNOUNCEMENT::EVENT::PLAYER::PlaybackFailed{item, announced}});
}

void CPlaybackAnnouncer::OnRepeat(Type type, CApplicationPlayLists::Repeat repeat)
{
  m_sink(PlaylistEvent{ANNOUNCEMENT::EVENT::PLAYLIST::PropertiesChanged{
      std::string{PLAYLIST::NameOf(type)}, std::nullopt,
      std::string{CApplicationPlayLists::RepeatName(repeat)}}});
}

void CPlaybackAnnouncer::PublishListChange(std::string_view playList,
                                           const PlayListChange& change) const
{
  using namespace ANNOUNCEMENT::EVENT::PLAYLIST;
  switch (change.type)
  {
    case PlayListChange::Type::Added:
      m_sink(PlaylistEvent{Add{std::string{playList}, change.position, change.item}});
      break;
    case PlayListChange::Type::Removed:
      m_sink(PlaylistEvent{Remove{std::string{playList}, change.position}});
      break;
    case PlayListChange::Type::Cleared:
      m_sink(PlaylistEvent{Clear{std::string{playList}}});
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
