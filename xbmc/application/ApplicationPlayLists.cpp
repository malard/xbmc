/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "ApplicationPlayLists.h"

#include "FileItem.h"
#include "FileItemList.h"
#include "GUIPassword.h"
#include "GUIUserMessages.h"
#include "PartyModeManager.h"
#include "ServiceBroker.h"
#include "URL.h"
#include "application/Application.h"
#include "application/ApplicationComponents.h"
#include "application/ApplicationPlayer.h"
#include "application/ApplicationPowerHandling.h"
#include "dialogs/GUIDialogKaiToast.h"
#include "filesystem/PluginDirectory.h"
#include "filesystem/VideoDatabaseFile.h"
#include "guilib/GUIComponent.h"
#include "guilib/GUIWindowManager.h"
#include "input/actions/Action.h"
#include "input/actions/ActionIDs.h"
#include "interfaces/AnnouncementManager.h"
#include "messaging/ApplicationMessenger.h"
#include "messaging/helpers/DialogOKHelper.h"
#include "music/MusicFileItemClassify.h"
#include "playlists/PlayList.h"
#include "playlists/PlayListFileItemClassify.h"
#include "pvr/channels/PVRChannel.h"
#include "resources/LocalizeStrings.h"
#include "resources/ResourcesComponent.h"
#include "settings/AdvancedSettings.h"
#include "settings/SettingsComponent.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/Variant.h"
#include "utils/log.h"
#include "video/VideoFileItemClassify.h"
#include "video/VideoInfoTag.h"

#include <mutex>

using namespace KODI;
using namespace KODI::MESSAGING;
using namespace KODI::PLAYLIST;

namespace
{
size_t Index(Type type)
{
  return type == PLAYLIST::Video ? 0 : 1;
}

const std::string& Localize(uint32_t code)
{
  return CServiceBroker::GetResourcesComponent().GetLocalizeStrings().Get(code);
}

std::string PropertyName(CApplicationPlayLists::PlayerProperty property)
{
  using enum CApplicationPlayLists::PlayerProperty;
  switch (property)
  {
    case PartyMode:
      return "partymode";
    case Shuffled:
      return "shuffled";
    case Repeat:
      return "repeat";
    case SubtitleEnabled:
      return "subtitleenabled";
    case CurrentSubtitle:
      return "currentsubtitle";
    case CurrentAudioStream:
      return "currentaudiostream";
    case CurrentVideoStream:
      return "currentvideostream";
  }
  return {};
}

std::string EventName(CApplicationPlayLists::PlayerEvent event)
{
  using enum CApplicationPlayLists::PlayerEvent;
  switch (event)
  {
    case Play:
      return "OnPlay";
    case AVStart:
      return "OnAVStart";
    case AVChange:
      return "OnAVChange";
    case Pause:
      return "OnPause";
    case Resume:
      return "OnResume";
    case Seek:
      return "OnSeek";
    case SpeedChanged:
      return "OnSpeedChanged";
    case Stop:
      return "OnStop";
  }
  return {};
}

void SendPlayListChanged()
{
  if (CGUIComponent* gui = CServiceBroker::GetGUI(); gui)
  {
    CGUIMessage msg(GUI_MSG_PLAYLIST_CHANGED, 0, 0);
    gui->GetWindowManager().SendThreadMessage(msg);
  }
}
} // namespace

CApplicationPlayLists::CApplicationPlayLists()
  : m_failedSongsStart(std::chrono::steady_clock::now())
{
  for (const Type type : {PLAYLIST::Video, PLAYLIST::Audio})
  {
    auto& playList = m_playLists[Index(type)];
    playList = std::make_unique<CPlayList>();
    playList->SetObserver([this, type](const std::vector<PlayListChange>& changes)
                          { OnPlayListChanged(type, changes); });
  }
}

CApplicationPlayLists::~CApplicationPlayLists()
{
  for (const auto& playList : m_playLists)
    playList->SetObserver(nullptr);
}

CPlayList& CApplicationPlayLists::GetPlayList(Type type)
{
  return *m_playLists[Index(type)];
}

const CPlayList& CApplicationPlayLists::GetPlayList(Type type) const
{
  return *m_playLists[Index(type)];
}

void CApplicationPlayLists::OnPlayListChanged(Type type, const std::vector<PlayListChange>& changes)
{
  const auto announcer = CServiceBroker::GetAnnouncementManager();
  for (const auto& change : changes)
  {
    CVariant data;
    data["playlistid"] = static_cast<int>(IdFromType(type));
    switch (change.type)
    {
      case PlayListChange::Type::Added:
        data["position"] = change.position;
        if (announcer)
          announcer->Announce(ANNOUNCEMENT::Playlist, "OnAdd", change.item, data);
        break;
      case PlayListChange::Type::Removed:
        data["position"] = change.position;
        if (announcer)
          announcer->Announce(ANNOUNCEMENT::Playlist, "OnRemove", data);
        break;
      case PlayListChange::Type::Cleared:
        g_application.m_strPlayListFile.clear();
        if (announcer)
          announcer->Announce(ANNOUNCEMENT::Playlist, "OnClear", data);
        break;
      case PlayListChange::Type::Moved:
        break;
    }
  }

  SendPlayListChanged();
}

std::optional<Type> CApplicationPlayLists::GetPlayingType() const
{
  std::unique_lock lock(m_critSection);
  return m_playingType;
}

void CApplicationPlayLists::SetPlayingType(std::optional<Type> type)
{
  {
    std::unique_lock lock(m_critSection);
    if (type == m_playingType)
      return;

    m_playingType = type;
    m_playedFirstFile = false;
  }

  if (g_partyModeManager.IsEnabled())
    g_partyModeManager.Disable();
}

CApplicationPlayLists::Phase CApplicationPlayLists::GetPhase(Type type) const
{
  std::unique_lock lock(m_critSection);
  if (type == PLAYLIST::Video && m_phase[Index(type)] == Phase::Idle)
    return m_slideShowPhase;
  return m_phase[Index(type)];
}

bool CApplicationPlayLists::IsPlaying(Type type) const
{
  return GetPlayingType() == type && GetPhase(type) != Phase::Idle;
}

int CApplicationPlayLists::GetPlayingPosition(Type type) const
{
  if (GetPlayingType() != type)
    return -1;
  return GetPlayList(type).GetCurrentPosition();
}

Type CApplicationPlayLists::GetQueueType(Type fallback) const
{
  if (const std::optional<Type> type = GetPlayingType(); type)
    return *type;

  const auto appPlayer = CServiceBroker::GetAppComponents().GetComponent<CApplicationPlayer>();
  return TypeFromId(appPlayer->GetPreferredPlaylist()).value_or(fallback);
}

bool CApplicationPlayLists::IsAudioFollowingVideo() const
{
  std::unique_lock lock(m_critSection);
  return m_audioFollowsVideo;
}

int CApplicationPlayLists::GetPlayerId() const
{
  return static_cast<int>(IdFromType(GetPlayingType()));
}

int CApplicationPlayLists::GetPlayerId(const CFileItem* item) const
{
  if (item && item->HasPVRChannelInfoTag())
    return static_cast<int>(item->GetPVRChannelInfoTag()->IsRadio() ? Id::TYPE_MUSIC
                                                                    : Id::TYPE_VIDEO);
  return GetPlayerId();
}

bool CApplicationPlayLists::HasPlayedFirstFile() const
{
  std::unique_lock lock(m_critSection);
  return m_playedFirstFile;
}

bool CApplicationPlayLists::OnAction(const CAction& action)
{
  if (action.GetID() == ACTION_PREV_ITEM && !IsSingleItemNonRepeatPlaylist())
  {
    PlayPrevious();
    return true;
  }
  else if (action.GetID() == ACTION_NEXT_ITEM && !IsSingleItemNonRepeatPlaylist())
  {
    PlayNext();
    return true;
  }
  else
    return false;
}

bool CApplicationPlayLists::IsSingleItemNonRepeatPlaylist() const
{
  const std::optional<Type> type = GetPlayingType();
  if (!type)
    return true;

  const CPlayList& playList = GetPlayList(*type);
  return playList.size() <= 1 && !playList.IsRepeat(playList.GetCurrent()) &&
         playList.GetWrap().GetKind() == Wrap::Kind::None;
}

bool CApplicationPlayLists::OnMessage(CGUIMessage& message)
{
  switch (message.GetMessage())
  {
    case GUI_MSG_NOTIFY_ALL:
      if (message.GetParam1() == GUI_MSG_UPDATE_ITEM && message.GetItem())
      {
        const auto item = std::static_pointer_cast<CFileItem>(message.GetItem());
        for (const auto& playList : m_playLists)
          playList->UpdateItem(item.get());
      }
      break;

    case GUI_MSG_PLAYBACK_STARTED:
    {
      std::unique_lock lock(m_critSection);
      m_playbackStarted = true;
      m_phase = {Phase::Idle, Phase::Idle};
      m_audioFollowsVideo = false;
      if (m_playingType)
      {
        m_phase[Index(*m_playingType)] = Phase::Playing;
        if (*m_playingType == PLAYLIST::Video)
        {
          const auto item = GetPlayList(PLAYLIST::Video).GetCurrentItem();
          m_audioFollowsVideo = item && !item->HasPictureInfoTag();
          if (m_audioFollowsVideo)
            m_phase[Index(PLAYLIST::Audio)] = Phase::Playing;
        }
      }
      break;
    }

    case GUI_MSG_PLAYBACK_PAUSED:
    case GUI_MSG_PLAYBACK_RESUMED:
    {
      const Phase phase =
          message.GetMessage() == GUI_MSG_PLAYBACK_PAUSED ? Phase::Paused : Phase::Playing;
      std::unique_lock lock(m_critSection);
      for (auto& sidePhase : m_phase)
      {
        if (sidePhase != Phase::Idle)
          sidePhase = phase;
      }
      break;
    }

    case GUI_MSG_PLAYBACK_ENDED:
    case GUI_MSG_PLAYBACK_ERROR:
    {
      std::unique_lock lock(m_critSection);
      m_phase = {Phase::Idle, Phase::Idle};
      m_audioFollowsVideo = false;
      break;
    }

    case GUI_MSG_PLAYBACK_STOPPED:
    {
      bool wasPlaying;
      {
        std::unique_lock lock(m_critSection);
        m_phase = {Phase::Idle, Phase::Idle};
        m_audioFollowsVideo = false;
        wasPlaying = m_playingType && m_playbackStarted;
      }
      if (wasPlaying)
      {
        EndPlayback(false);
        return true;
      }
      break;
    }
  }

  return false;
}

void CApplicationPlayLists::EndPlayback(bool clearPlayList)
{
  std::optional<Type> type;
  {
    std::unique_lock lock(m_critSection);
    type = m_playingType;
    m_playingType.reset();
    m_playedFirstFile = false;
    m_playbackStarted = false;
    m_queued = NO_ENTRY;
  }

  const int position = type ? GetPlayList(*type).GetCurrentPosition() : -1;
  if (CGUIComponent* gui = CServiceBroker::GetGUI(); gui)
  {
    CGUIMessage msg(GUI_MSG_PLAYLISTPLAYER_STOPPED, 0, 0, static_cast<int>(IdFromType(type)),
                    position);
    gui->GetWindowManager().SendThreadMessage(msg);
  }

  if (type)
  {
    if (clearPlayList)
      GetPlayList(*type).Clear();
    else
      GetPlayList(*type).ClearCurrent();
  }

  SendPlayListChanged();
}

bool CApplicationPlayLists::Play(Type type,
                                 std::optional<int> position /* = std::nullopt */,
                                 const std::string& player /* = "" */,
                                 bool replace /* = false */,
                                 bool playPreviousOnFail /* = false */)
{
  SetPlayingType(type);
  {
    std::unique_lock lock(m_critSection);
    m_playedFirstFile = false;
  }

  CPlayList& playList = GetPlayList(type);
  if (playList.empty())
    return false;

  EntryId entry;
  if (position)
  {
    entry = playList.GetEntryId(std::clamp(*position, 0, playList.size() - 1));
    playList.SetCurrent(entry);
    playList.ResetShuffle();
  }
  else
  {
    playList.ClearCurrent();
    entry = playList.PeekNext(Advance::User);
  }
  return PlayEntry(type, entry, player, replace, playPreviousOnFail);
}

bool CApplicationPlayLists::Play(Type type,
                                 const CFileItemList& items,
                                 std::optional<int> position /* = std::nullopt */,
                                 const std::string& player /* = "" */)
{
  GetPlayList(type).Clear();
  GetPlayList(type).Add(items);
  return Play(type, position, player);
}

int CApplicationPlayLists::Queue(Type type, const CFileItemList& items, bool playNext)
{
  if (items.IsEmpty())
    return -1;

  CPlayList& playList = GetPlayList(type);
  if (playNext && IsPlaying(type))
    return playList.GetPosition(playList.PlayNext(items));

  const int first = playList.size();
  playList.Add(items);
  return first;
}

bool CApplicationPlayLists::Play(Type type,
                                 const std::shared_ptr<CFileItem>& item,
                                 const std::string& player)
{
  GetPlayList(type).Clear();
  GetPlayList(type).Add(item);
  return Play(type, std::nullopt, player);
}

bool CApplicationPlayLists::Play(const std::shared_ptr<CFileItem>& item, const std::string& player)
{
  const bool isVideo{VIDEO::IsVideo(*item)};
  const bool isAudio{MUSIC::IsAudio(*item)};

  if (isVideo == isAudio)
  {
    CLog::LogF(LOGWARNING, "ListItem type must be audio or video type. The type can be specified "
                           "by using ListItem::getVideoInfoTag or ListItem::getMusicInfoTag, in "
                           "the case of playlist entries by adding #KODIPROP mimetype value.");
    return false;
  }

  return Play(isVideo ? PLAYLIST::Video : PLAYLIST::Audio, item, player);
}

bool CApplicationPlayLists::PlayEntry(
    Type type, EntryId entry, const std::string& player, bool replace, bool playPreviousOnFail)
{
  CPlayList& playList = GetPlayList(type);
  const int position = playList.GetPosition(entry);
  if (position < 0)
    return false;

  // check if the item itself is a playlist, and can be expanded
  // only allow a few levels, this could end up in a loop
  // if they refer to each other in a loop
  for (int i = 0; i < 5; i++)
  {
    if (!playList.Expand(position))
      break;
  }
  entry = playList.GetEntryId(position);
  playList.SetCurrent(entry);

  const std::shared_ptr<CFileItem> item = playList.GetItem(entry);
  if (!item)
    return false;

  if (VIDEO::IsVideoDb(*item) && !item->HasVideoInfoTag())
    *(item->GetVideoInfoTag()) = XFILE::CVideoDatabaseFile::GetVideoTag(CURL(item->GetDynPath()));

  {
    std::unique_lock lock(m_critSection);
    m_playbackStarted = false;
  }

  const auto playAttempt = std::chrono::steady_clock::now();
  if (!g_application.PlayFile(*item, player, replace))
  {
    CLog::Log(LOGERROR, "Playlist: skipping unplayable entry: {}, path [{}]", entry,
              CURL::GetRedacted(item->GetDynPath()));
    playList.SetUnPlayable(entry);

    bool abort;
    {
      std::unique_lock lock(m_critSection);
      // abort on 100 failed CONSECUTIVE songs
      if (!m_failedSongs)
        m_failedSongsStart = playAttempt;
      m_failedSongs++;

      const auto advancedSettings =
          CServiceBroker::GetSettingsComponent()->GetAdvancedSettings();
      const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - m_failedSongsStart);

      abort = (m_failedSongs >= advancedSettings->m_playlistRetries &&
               advancedSettings->m_playlistRetries >= 0) ||
              ((duration.count() >=
                static_cast<unsigned int>(advancedSettings->m_playlistTimeout) * 1000) &&
               advancedSettings->m_playlistTimeout);
      if (abort)
      {
        m_failedSongs = 0;
        m_failedSongsStart = std::chrono::steady_clock::now();
      }
    }

    if (abort)
    {
      CLog::Log(LOGDEBUG, "Playlist: one or more items failed to play... aborting playback");
      HELPERS::ShowOKDialogText(CVariant{16026}, CVariant{16027});
      EndPlayback(true);
      return false;
    }

    if (playList.GetPlayable() > 0)
      return playPreviousOnFail ? PlayPrevious() : PlayNext(Advance::Automatic);

    CLog::Log(LOGDEBUG, "Playlist: no more playable items... aborting playback");
    EndPlayback(false);
    return false;
  }

  // reset the start offset of this item
  if (item->GetStartOffset() == STARTOFFSET_RESUME)
    item->SetStartOffset(0);

  // PlayFile() discards the notices the edits leading up to it posted.
  SendPlayListChanged();

  std::unique_lock lock(m_critSection);
  m_failedSongs = 0;
  m_failedSongsStart = std::chrono::steady_clock::now();
  m_playedFirstFile = true;
  return true;
}

bool CApplicationPlayLists::PlayNext(Advance advance /* = Advance::User */)
{
  const std::optional<Type> type = GetPlayingType();
  EntryId next = NO_ENTRY;
  if (type && GetPlayList(*type).GetPlayable() > 0)
    next = GetPlayList(*type).Next(advance);

  if (next == NO_ENTRY)
  {
    if (advance == Advance::User)
      CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Info, Localize(559),
                                            Localize(34201));
    EndPlayback(false);
    return false;
  }

  const auto appPlayer = CServiceBroker::GetAppComponents().GetComponent<CApplicationPlayer>();
  return PlayEntry(*type, next, appPlayer->GetName(), false, false);
}

bool CApplicationPlayLists::PlayNext(Type type)
{
  SetPlayingType(type);
  return PlayNext();
}

bool CApplicationPlayLists::PlayPrevious(Type type)
{
  SetPlayingType(type);
  return PlayPrevious();
}

bool CApplicationPlayLists::PlayPrevious()
{
  const std::optional<Type> type = GetPlayingType();
  if (!type)
    return false;

  const EntryId previous = GetPlayList(*type).Previous();
  if (previous == NO_ENTRY)
  {
    CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Info, Localize(559),
                                          Localize(34202));
    return false;
  }

  return PlayEntry(*type, previous, "", false, true);
}

bool CApplicationPlayLists::PlayOffset(int offset)
{
  const std::optional<Type> type = GetPlayingType();
  if (!type)
    return false;

  const CPlayList& playList = GetPlayList(*type);
  const EntryId entry = offset >= 0 ? playList.PeekNext(Advance::User, offset)
                                    : playList.PeekPrevious(-offset);
  if (entry == NO_ENTRY)
  {
    CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Info, Localize(559),
                                          Localize(34201));
    EndPlayback(false);
    return false;
  }

  const auto appPlayer = CServiceBroker::GetAppComponents().GetComponent<CApplicationPlayer>();
  return PlayEntry(*type, entry, appPlayer->GetName(), false, false);
}

EntryId CApplicationPlayLists::PeekNextEntry() const
{
  const std::optional<Type> type = GetPlayingType();
  if (!type)
    return NO_ENTRY;

  return GetPlayList(*type).PeekNext(Advance::Automatic);
}

void CApplicationPlayLists::OnNextQueued(EntryId entry)
{
  std::unique_lock lock(m_critSection);
  m_queued = entry;
}

void CApplicationPlayLists::SkipQueued(EntryId entry)
{
  if (const std::optional<Type> type = GetPlayingType(); type)
    GetPlayList(*type).SetCurrent(entry);
}

void CApplicationPlayLists::ClearQueued()
{
  std::unique_lock lock(m_critSection);
  m_queued = NO_ENTRY;
}

std::optional<std::shared_ptr<CFileItem>> CApplicationPlayLists::OnQueuedStarted()
{
  std::optional<Type> type;
  EntryId queued;
  {
    std::unique_lock lock(m_critSection);
    type = m_playingType;
    queued = m_queued;
    m_queued = NO_ENTRY;
  }
  if (queued == NO_ENTRY)
    return std::nullopt;

  if (!type || !GetPlayList(*type).SetCurrent(queued))
    return nullptr;

  return GetPlayList(*type).GetItem(queued);
}

void CApplicationPlayLists::SetShuffle(Type type, bool shuffle, bool notify /* = false */)
{
  CPlayList& playList = GetPlayList(type);
  if (shuffle != playList.IsShuffled())
  {
    playList.SetShuffled(shuffle);

    if (notify)
    {
      const std::string shuffleStr =
          StringUtils::Format("{}: {}", Localize(191), Localize(shuffle ? 593 : 591));
      CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Info, Localize(559), shuffleStr);
    }
  }

  SendPlayListChanged();
  Announce(type, PlayerProperty::Shuffled, playList.IsShuffled());
}

bool CApplicationPlayLists::IsShuffled(Type type) const
{
  return GetPlayList(type).IsShuffled();
}

void CApplicationPlayLists::SetRepeat(Type type, Repeat repeat, bool notify /* = false */)
{
  const Repeat previous = GetRepeat(type);

  CPlayList& playList = GetPlayList(type);
  playList.ClearRepeats();
  playList.SetWrap(Wrap::None());
  if (repeat == Repeat::One)
    playList.SetRepeat(playList.GetCurrent());
  else if (repeat == Repeat::All)
    playList.SetWrap(Wrap::ToStart());

  if (previous != repeat && notify)
  {
    int label;
    if (repeat == Repeat::Off)
      label = 595; // Repeat: Off
    else if (repeat == Repeat::One)
      label = 596; // Repeat: One
    else
      label = 597; // Repeat: All
    CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Info, Localize(559),
                                          Localize(label));
  }

  SendPlayListChanged();

  CVariant data;
  switch (GetRepeat(type))
  {
    case Repeat::One:
      data = "one";
      break;
    case Repeat::All:
      data = "all";
      break;
    default:
      data = "off";
      break;
  }
  Announce(type, PlayerProperty::Repeat, data);
}

CApplicationPlayLists::Repeat CApplicationPlayLists::GetRepeat(Type type) const
{
  const CPlayList& playList = GetPlayList(type);
  if (playList.IsRepeat(playList.GetCurrent()))
    return Repeat::One;
  if (playList.GetWrap().GetKind() == Wrap::Kind::ToStart)
    return Repeat::All;
  return Repeat::Off;
}

void CApplicationPlayLists::ClearPlayLists()
{
  for (const auto& playList : m_playLists)
    playList->Clear();
}

void CApplicationPlayLists::Announce(PlayerProperty property, const CVariant& value) const
{
  Announce(PlayerProperties{{property, value}});
}

void CApplicationPlayLists::Announce(const PlayerProperties& properties) const
{
  const auto appPlayer = CServiceBroker::GetAppComponents().GetComponent<CApplicationPlayer>();
  if (!appPlayer->IsPlaying())
    return;

  CVariant data;
  for (const auto& [property, value] : properties)
  {
    if (!value.isNull())
      data["property"][PropertyName(property)] = value;
  }
  if (!data.isMember("property"))
    return;

  data["player"]["playerid"] = GetPlayerId();
  CServiceBroker::GetAnnouncementManager()->Announce(ANNOUNCEMENT::Player, "OnPropertyChanged",
                                                     data);
}

void CApplicationPlayLists::Announce(PlayerEvent event,
                                     const std::shared_ptr<const CFileItem>& item,
                                     CVariant data) const
{
  if (event != PlayerEvent::Stop)
    data["player"]["playerid"] = GetPlayerId(item.get());
  CServiceBroker::GetAnnouncementManager()->Announce(ANNOUNCEMENT::Player, EventName(event), item,
                                                     data);
}

void CApplicationPlayLists::OnSlideShow(PlayerEvent event,
                                        const std::shared_ptr<const CFileItem>& slide,
                                        bool running)
{
  CVariant data;
  data["player"]["playerid"] = static_cast<int>(Id::TYPE_PICTURE);
  {
    std::unique_lock lock(m_critSection);
    switch (event)
    {
      case PlayerEvent::Play:
        m_slideShowPhase = running ? Phase::Playing : Phase::Paused;
        data["player"]["speed"] = running ? 1 : 0;
        break;
      case PlayerEvent::Pause:
        m_slideShowPhase = Phase::Paused;
        data["player"]["speed"] = 0;
        break;
      case PlayerEvent::Stop:
        m_slideShowPhase = Phase::Idle;
        data["end"] = true;
        break;
      default:
        return;
    }
  }
  CServiceBroker::GetAnnouncementManager()->Announce(ANNOUNCEMENT::Player, EventName(event), slide,
                                                     data);
}

void CApplicationPlayLists::OnSlideShow(PlayerProperty property, const CVariant& value) const
{
  if (value.isNull())
    return;

  CVariant data;
  data["player"]["playerid"] = static_cast<int>(Id::TYPE_PICTURE);
  data["property"][PropertyName(property)] = value;
  CServiceBroker::GetAnnouncementManager()->Announce(ANNOUNCEMENT::Player, "OnPropertyChanged",
                                                     data);
}

void CApplicationPlayLists::OnSlideShowListChanged(const PlayListChange& change) const
{
  CVariant data;
  data["playlistid"] = static_cast<int>(Id::TYPE_PICTURE);
  switch (change.type)
  {
    case PlayListChange::Type::Added:
      data["position"] = change.position;
      CServiceBroker::GetAnnouncementManager()->Announce(ANNOUNCEMENT::Playlist, "OnAdd",
                                                         change.item, data);
      break;
    case PlayListChange::Type::Cleared:
      CServiceBroker::GetAnnouncementManager()->Announce(ANNOUNCEMENT::Playlist, "OnClear", data);
      break;
    default:
      break;
  }
}

void CApplicationPlayLists::Announce(Type type,
                                     PlayerProperty property,
                                     const CVariant& value) const
{
  // A playlist's own settings are published only while it is the one playing.
  if (GetPlayingType() == type && GetPhase(type) != Phase::Idle)
    Announce(property, value);
}

int CApplicationPlayLists::GetMessageMask()
{
  return TMSG_MASK_PLAYLISTPLAYER;
}

void CApplicationPlayLists::OnApplicationMessage(ThreadMessage* pMsg)
{
  switch (pMsg->dwMessage)
  {
    case TMSG_PLAYLISTPLAYER_PLAY:
    {
      const std::optional<Type> type = GetPlayingType();
      if (!type)
        break;
      if (pMsg->param1 == -1)
      {
        Play(*type);
      }
      else
      {
        const CPlayList& playList = GetPlayList(*type);
        if (!playList.empty())
        {
          const int position = std::clamp(pMsg->param1, 0, playList.size() - 1);
          PlayEntry(*type, playList.GetEntryId(position), "", false, false);
        }
      }
      break;
    }

    case TMSG_PLAYLISTPLAYER_NEXT:
      PlayNext();
      break;

    case TMSG_PLAYLISTPLAYER_PREV:
      PlayPrevious();
      break;

    case TMSG_PLAYLISTPLAYER_SHUFFLE:
      if (const std::optional<Type> type = TypeFromId(Id{pMsg->param1}); type)
        SetShuffle(*type, pMsg->param2 > 0);
      break;

    case TMSG_PLAYLISTPLAYER_REPEAT:
      if (const std::optional<Type> type = TypeFromId(Id{pMsg->param1}); type)
        SetRepeat(*type, static_cast<Repeat>(pMsg->param2));
      break;

    case TMSG_MEDIA_PLAY:
      OnMediaPlay(pMsg);
      break;

    default:
      break;
  }
}

void CApplicationPlayLists::OnMediaPlay(ThreadMessage* pMsg)
{
  const auto appPower = CServiceBroker::GetAppComponents().GetComponent<CApplicationPowerHandling>();
  appPower->ResetScreenSaver();
  appPower->WakeUpScreenSaverAndDPMS();

  // first check if we were called from the PlayFile() function
  if (pMsg->lpVoid && pMsg->param2 == 0)
  {
    // Leave the current entry, if TMSG_MEDIA_PLAY gets posted with just a single item.
    // Otherwise items may fail to play, when started while a playlist is playing.
    // But a single item in a stack is allowed.
    if (const std::optional<Type> type = GetPlayingType(); type)
    {
      const auto current = GetPlayList(*type).GetCurrentItem();
      if (!current || !URIUtils::IsStack(current->GetDynPath()))
      {
        GetPlayList(*type).ClearCurrent();
        std::unique_lock lock(m_critSection);
        m_playedFirstFile = false;
        m_playbackStarted = false;
      }
    }

    const std::unique_ptr<CFileItem> item{static_cast<CFileItem*>(pMsg->lpVoid)};
    g_application.PlayFile(*item, "", pMsg->param1 != 0);
    return;
  }

  if (pMsg->lpVoid)
  {
    const std::unique_ptr<CFileItemList> list{static_cast<CFileItemList*>(pMsg->lpVoid)};
    if (list->Size() <= 0)
      return;

    // Nobody named a playlist, so the items choose it.
    Type type = PLAYLIST::Audio;
    for (int i = 0; i < list->Size(); i++)
    {
      if (VIDEO::IsVideo(*list->Get(i)))
      {
        type = PLAYLIST::Video;
        break;
      }
    }

    GetPlayList(type).Clear();
    SetPlayingType(type);
    if (list->Size() == 1 && !PLAYLIST::IsPlayList(*list->Get(0)))
    {
      const std::shared_ptr<CFileItem> item = (*list)[0];
      // if the item is a plugin we need to resolve the URL to ensure the infotags are filled.
      if (URIUtils::HasPluginPath(*item) &&
          !XFILE::CPluginDirectory::GetResolvedPluginResult(*item))
      {
        return;
      }
      const bool isVideo{VIDEO::IsVideo(*item)};
      const bool isAudio{MUSIC::IsAudio(*item)};
      if (isAudio || isVideo)
      {
        if ((isVideo && !g_passwordManager.IsVideoUnlocked()) ||
            (isAudio && !g_passwordManager.IsMusicUnlocked()))
        {
          CLog::LogF(LOGERROR, "MasterCode or MediaSource-code is wrong: {} will not be played.",
                     item->GetPath());
          return;
        }
        Play(type, item, pMsg->strParam);
      }
      else
        g_application.PlayMedia(*item, pMsg->strParam, type);
    }
    else
    {
      // Handle "shuffled" option if present
      if (list->HasProperty("shuffled") && list->GetProperty("shuffled").isBoolean())
        SetShuffle(type, list->GetProperty("shuffled").asBoolean(), false);
      // Handle "repeat" option if present
      if (list->HasProperty("repeat") && list->GetProperty("repeat").isInteger())
        SetRepeat(type, static_cast<Repeat>(list->GetProperty("repeat").asInteger()), false);

      GetPlayList(type).Add(*list);
      Play(type, pMsg->param1 < 0 ? std::nullopt : std::optional<int>(pMsg->param1),
           pMsg->strParam);
    }
  }
  else if (const std::optional<Type> type = TypeFromId(Id{pMsg->param1});
           type && (Id{pMsg->param1} == Id::TYPE_MUSIC || Id{pMsg->param1} == Id::TYPE_VIDEO))
  {
    SetPlayingType(*type);
    CServiceBroker::GetAppMessenger()->SendMsg(TMSG_PLAYLISTPLAYER_PLAY, pMsg->param2);
  }
}
