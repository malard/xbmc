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
size_t Index(Side side)
{
  return side == Side::Video ? 0 : 1;
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
  for (const Side side : {Side::Video, Side::Audio})
  {
    auto& playList = m_playLists[Index(side)];
    playList = std::make_unique<CPlayList>();
    playList->SetObserver([this, side](const std::vector<PlayListChange>& changes)
                          { OnPlayListChanged(side, changes); });
  }
}

CApplicationPlayLists::~CApplicationPlayLists()
{
  for (const auto& playList : m_playLists)
    playList->SetObserver(nullptr);
}

CPlayList& CApplicationPlayLists::GetPlayList(Side side)
{
  return *m_playLists[Index(side)];
}

const CPlayList& CApplicationPlayLists::GetPlayList(Side side) const
{
  return *m_playLists[Index(side)];
}

void CApplicationPlayLists::OnPlayListChanged(Side side, const std::vector<PlayListChange>& changes)
{
  const auto announcer = CServiceBroker::GetAnnouncementManager();
  for (const auto& change : changes)
  {
    CVariant data;
    data["playlistid"] = static_cast<int>(IdFromSide(side));
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

std::optional<Side> CApplicationPlayLists::GetPlayingSide() const
{
  std::unique_lock lock(m_critSection);
  return m_playingSide;
}

void CApplicationPlayLists::SetPlayingSide(std::optional<Side> side)
{
  {
    std::unique_lock lock(m_critSection);
    if (side == m_playingSide)
      return;

    m_playingSide = side;
    m_playedFirstFile = false;
  }

  if (g_partyModeManager.IsEnabled())
    g_partyModeManager.Disable();
}

CApplicationPlayLists::Phase CApplicationPlayLists::GetPhase(Side side) const
{
  std::unique_lock lock(m_critSection);
  return m_phase[Index(side)];
}

bool CApplicationPlayLists::IsAudioFollowingVideo() const
{
  std::unique_lock lock(m_critSection);
  return m_audioFollowsVideo;
}

int CApplicationPlayLists::GetPlayerId() const
{
  return static_cast<int>(IdFromSide(GetPlayingSide()));
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
  const std::optional<Side> side = GetPlayingSide();
  if (!side)
    return true;

  const CPlayList& playList = GetPlayList(*side);
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
      if (m_playingSide)
      {
        m_phase[Index(*m_playingSide)] = Phase::Playing;
        if (*m_playingSide == Side::Video)
        {
          const auto item = GetPlayList(Side::Video).GetCurrentItem();
          m_audioFollowsVideo = item && !item->HasPictureInfoTag();
          if (m_audioFollowsVideo)
            m_phase[Index(Side::Audio)] = Phase::Playing;
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
        wasPlaying = m_playingSide && m_playbackStarted;
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
  std::optional<Side> side;
  {
    std::unique_lock lock(m_critSection);
    side = m_playingSide;
    m_playingSide.reset();
    m_playedFirstFile = false;
    m_playbackStarted = false;
    m_queued = NO_ENTRY;
  }

  const int position = side ? GetPlayList(*side).GetCurrentPosition() : -1;
  if (CGUIComponent* gui = CServiceBroker::GetGUI(); gui)
  {
    CGUIMessage msg(GUI_MSG_PLAYLISTPLAYER_STOPPED, 0, 0, static_cast<int>(IdFromSide(side)),
                    position);
    gui->GetWindowManager().SendThreadMessage(msg);
  }

  if (side)
  {
    if (clearPlayList)
      GetPlayList(*side).Clear();
    else
      GetPlayList(*side).ClearCurrent();
  }

  SendPlayListChanged();
}

bool CApplicationPlayLists::Play(Side side,
                                 std::optional<int> position /* = std::nullopt */,
                                 const std::string& player /* = "" */,
                                 bool replace /* = false */,
                                 bool playPreviousOnFail /* = false */)
{
  SetPlayingSide(side);
  {
    std::unique_lock lock(m_critSection);
    m_playedFirstFile = false;
  }

  CPlayList& playList = GetPlayList(side);
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
  return PlayEntry(side, entry, player, replace, playPreviousOnFail);
}

bool CApplicationPlayLists::Play(Side side,
                                 const std::shared_ptr<CFileItem>& item,
                                 const std::string& player)
{
  GetPlayList(side).Clear();
  GetPlayList(side).Add(item);
  return Play(side, std::nullopt, player);
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

  return Play(isVideo ? Side::Video : Side::Audio, item, player);
}

bool CApplicationPlayLists::PlayEntry(Side side,
                                      EntryId entry,
                                      const std::string& player,
                                      bool replace,
                                      bool playPreviousOnFail)
{
  CPlayList& playList = GetPlayList(side);
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
  const std::optional<Side> side = GetPlayingSide();
  EntryId next = NO_ENTRY;
  if (side && GetPlayList(*side).GetPlayable() > 0)
    next = GetPlayList(*side).Next(advance);

  if (next == NO_ENTRY)
  {
    if (advance == Advance::User)
      CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Info, Localize(559),
                                            Localize(34201));
    EndPlayback(false);
    return false;
  }

  const auto appPlayer = CServiceBroker::GetAppComponents().GetComponent<CApplicationPlayer>();
  return PlayEntry(*side, next, appPlayer->GetName(), false, false);
}

bool CApplicationPlayLists::PlayPrevious()
{
  const std::optional<Side> side = GetPlayingSide();
  if (!side)
    return false;

  const EntryId previous = GetPlayList(*side).Previous();
  if (previous == NO_ENTRY)
  {
    CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Info, Localize(559),
                                          Localize(34202));
    return false;
  }

  return PlayEntry(*side, previous, "", false, true);
}

bool CApplicationPlayLists::PlayOffset(int offset)
{
  const std::optional<Side> side = GetPlayingSide();
  if (!side)
    return false;

  const CPlayList& playList = GetPlayList(*side);
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
  return PlayEntry(*side, entry, appPlayer->GetName(), false, false);
}

std::shared_ptr<CFileItem> CApplicationPlayLists::PeekNextItem(int steps /* = 1 */) const
{
  const std::optional<Side> side = GetPlayingSide();
  if (!side)
    return nullptr;

  const CPlayList& playList = GetPlayList(*side);
  return playList.GetItem(playList.PeekNext(Advance::Automatic, steps));
}

EntryId CApplicationPlayLists::PeekNextEntry() const
{
  const std::optional<Side> side = GetPlayingSide();
  if (!side)
    return NO_ENTRY;

  return GetPlayList(*side).PeekNext(Advance::Automatic);
}

void CApplicationPlayLists::OnNextQueued(EntryId entry)
{
  std::unique_lock lock(m_critSection);
  m_queued = entry;
}

void CApplicationPlayLists::SkipQueued(EntryId entry)
{
  if (const std::optional<Side> side = GetPlayingSide(); side)
    GetPlayList(*side).SetCurrent(entry);
}

void CApplicationPlayLists::ClearQueued()
{
  std::unique_lock lock(m_critSection);
  m_queued = NO_ENTRY;
}

std::optional<std::shared_ptr<CFileItem>> CApplicationPlayLists::OnQueuedStarted()
{
  std::optional<Side> side;
  EntryId queued;
  {
    std::unique_lock lock(m_critSection);
    side = m_playingSide;
    queued = m_queued;
    m_queued = NO_ENTRY;
  }
  if (queued == NO_ENTRY)
    return std::nullopt;

  if (!side || !GetPlayList(*side).SetCurrent(queued))
    return nullptr;

  return GetPlayList(*side).GetItem(queued);
}

void CApplicationPlayLists::SetShuffle(Side side, bool shuffle, bool notify /* = false */)
{
  CPlayList& playList = GetPlayList(side);
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
  Announce(side, PlayerProperty::Shuffled, playList.IsShuffled());
}

bool CApplicationPlayLists::IsShuffled(Side side) const
{
  return GetPlayList(side).IsShuffled();
}

void CApplicationPlayLists::SetRepeat(Side side, Repeat repeat, bool notify /* = false */)
{
  const Repeat previous = GetRepeat(side);

  CPlayList& playList = GetPlayList(side);
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
  switch (GetRepeat(side))
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
  Announce(side, PlayerProperty::Repeat, data);
}

CApplicationPlayLists::Repeat CApplicationPlayLists::GetRepeat(Side side) const
{
  const CPlayList& playList = GetPlayList(side);
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
  const auto appPlayer = CServiceBroker::GetAppComponents().GetComponent<CApplicationPlayer>();
  if (value.isNull() || !appPlayer->IsPlaying())
    return;

  CVariant data;
  data["player"]["playerid"] = GetPlayerId();
  data["property"][PropertyName(property)] = value;
  CServiceBroker::GetAnnouncementManager()->Announce(ANNOUNCEMENT::Player, "OnPropertyChanged",
                                                     data);
}

void CApplicationPlayLists::Announce(Side side, PlayerProperty property, const CVariant& value) const
{
  // A playlist's own settings are published only while it is the one playing.
  if (GetPlayingSide() == side && GetPhase(side) != Phase::Idle)
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
      const std::optional<Side> side = GetPlayingSide();
      if (!side)
        break;
      if (pMsg->param1 == -1)
      {
        Play(*side);
      }
      else
      {
        const CPlayList& playList = GetPlayList(*side);
        if (!playList.empty())
        {
          const int position = std::clamp(pMsg->param1, 0, playList.size() - 1);
          PlayEntry(*side, playList.GetEntryId(position), "", false, false);
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
      if (const std::optional<Side> side = SideFromId(Id{pMsg->param1}); side)
        SetShuffle(*side, pMsg->param2 > 0);
      break;

    case TMSG_PLAYLISTPLAYER_REPEAT:
      if (const std::optional<Side> side = SideFromId(Id{pMsg->param1}); side)
        SetRepeat(*side, static_cast<Repeat>(pMsg->param2));
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
    if (const std::optional<Side> side = GetPlayingSide(); side)
    {
      const auto current = GetPlayList(*side).GetCurrentItem();
      if (!current || !URIUtils::IsStack(current->GetDynPath()))
      {
        GetPlayList(*side).ClearCurrent();
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

    // Nobody named a side, so the items choose it.
    Side side = Side::Audio;
    for (int i = 0; i < list->Size(); i++)
    {
      if (VIDEO::IsVideo(*list->Get(i)))
      {
        side = Side::Video;
        break;
      }
    }

    GetPlayList(side).Clear();
    SetPlayingSide(side);
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
        Play(side, item, pMsg->strParam);
      }
      else
        g_application.PlayMedia(*item, pMsg->strParam, side);
    }
    else
    {
      // Handle "shuffled" option if present
      if (list->HasProperty("shuffled") && list->GetProperty("shuffled").isBoolean())
        SetShuffle(side, list->GetProperty("shuffled").asBoolean(), false);
      // Handle "repeat" option if present
      if (list->HasProperty("repeat") && list->GetProperty("repeat").isInteger())
        SetRepeat(side, static_cast<Repeat>(list->GetProperty("repeat").asInteger()), false);

      GetPlayList(side).Add(*list);
      Play(side, pMsg->param1 < 0 ? std::nullopt : std::optional<int>(pMsg->param1),
           pMsg->strParam);
    }
  }
  else if (const std::optional<Side> side = SideFromId(Id{pMsg->param1});
           side && (Id{pMsg->param1} == Id::TYPE_MUSIC || Id{pMsg->param1} == Id::TYPE_VIDEO))
  {
    SetPlayingSide(*side);
    CServiceBroker::GetAppMessenger()->SendMsg(TMSG_PLAYLISTPLAYER_PLAY, pMsg->param2);
  }
}
