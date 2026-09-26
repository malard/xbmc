/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "PartyModeManager.h"

#include "FileItem.h"
#include "FileItemList.h"
#include "GUIUserMessages.h"
#include "ServiceBroker.h"
#include "application/ApplicationComponents.h"
#include "application/ApplicationPlayLists.h"
#include "dialogs/GUIDialogProgress.h"
#include "guilib/GUIComponent.h"
#include "guilib/GUIWindowManager.h"
#include "messaging/helpers/DialogOKHelper.h"
#include "music/MusicDatabase.h"
#include "music/tags/MusicInfoTag.h"
#include "playlists/PlayList.h"
#include "playlists/PlayListShuffle.h"
#include "playlists/SmartPlayList.h"
#include "profiles/ProfileManager.h"
#include "settings/SettingsComponent.h"
#include "utils/Random.h"
#include "utils/StringUtils.h"
#include "utils/Variant.h"
#include "utils/log.h"
#include "video/VideoDatabase.h"
#include "video/VideoInfoTag.h"

#include <algorithm>

using namespace KODI;
using namespace KODI::MESSAGING;

#define QUEUE_DEPTH       10

CPartyModeManager::CPartyModeManager(void)
{
  m_bIsVideo = false;
  m_bEnabled = false;
  ClearState();
}

bool CPartyModeManager::Enable(PartyModeContext context /*= PARTYMODECONTEXT_MUSIC*/, const std::string& strXspPath /*= ""*/)
{
  // Filter using our PartyMode xml file
  PLAYLIST::CSmartPlaylist playlist;
  std::string partyModePath;
  bool playlistLoaded;

  m_bIsVideo = context == PartyModeContext::VIDEO;

  const std::shared_ptr<CProfileManager> profileManager = CServiceBroker::GetSettingsComponent()->GetProfileManager();

  if (!strXspPath.empty()) //if a path to a smartplaylist is supplied use it
    partyModePath = strXspPath;
  else if (m_bIsVideo)
    partyModePath = profileManager->GetUserDataItem("PartyMode-Video.xsp");
  else
    partyModePath = profileManager->GetUserDataItem("PartyMode.xsp");

  playlistLoaded=playlist.Load(partyModePath);

  if (playlistLoaded)
  {
    m_type = playlist.GetType();
    if (context == PartyModeContext::UNKNOWN)
    {
      //get it from the xsp file
      m_bIsVideo = (StringUtils::EqualsNoCase(m_type, "video") ||
        StringUtils::EqualsNoCase(m_type, "musicvideos") ||
        StringUtils::EqualsNoCase(m_type, "mixed"));
    }
  }
  else if (m_bIsVideo)
    m_type = "musicvideos";
  else
    m_type = "songs";

  CGUIDialogProgress* pDialog = CServiceBroker::GetGUI()->GetWindowManager().GetWindow<CGUIDialogProgress>(WINDOW_DIALOG_PROGRESS);
  int iHeading = (m_bIsVideo ? 20250 : 20121);
  int iLine0 = (m_bIsVideo ? 20251 : 20123);
  pDialog->SetHeading(CVariant{iHeading});
  pDialog->SetLine(0, CVariant{iLine0});
  pDialog->SetLine(1, CVariant{""});
  pDialog->SetLine(2, CVariant{""});
  pDialog->Open();

  ClearState();
  std::string strCurrentFilterMusic;
  std::string strCurrentFilterVideo;
  unsigned int songcount = 0;
  unsigned int videocount = 0;
  auto start = std::chrono::steady_clock::now();

  if (StringUtils::EqualsNoCase(m_type, "songs") ||
      StringUtils::EqualsNoCase(m_type, "mixed"))
  {
    CMusicDatabase db;
    if (db.Open())
    {
      std::set<std::string, std::less<>> playlists;
      if (playlistLoaded)
      {
        playlist.SetType("songs");
        strCurrentFilterMusic = playlist.GetWhereClause(db, playlists);
      }

      CLog::Log(LOGINFO, "PARTY MODE MANAGER: Registering filter:[{}]", strCurrentFilterMusic);
      songcount = db.GetRandomSongIDs(CDatabase::Filter(strCurrentFilterMusic), m_songIDCache);
      m_iMatchingSongs = static_cast<int>(songcount);
      if (m_iMatchingSongs < 1 && StringUtils::EqualsNoCase(m_type, "songs"))
      {
        pDialog->Close();
        db.Close();
        OnError(16031, "Party mode found no matching songs. Aborting.");
        return false;
      }
    }
    else
    {
      pDialog->Close();
      OnError(16033, "Party mode could not open database. Aborting.");
      return false;
    }
    db.Close();
  }

  if (StringUtils::EqualsNoCase(m_type, "musicvideos") ||
      StringUtils::EqualsNoCase(m_type, "mixed"))
  {
    std::vector< std::pair<int,int> > songIDs2;
    CVideoDatabase db;
    if (db.Open())
    {
      std::set<std::string, std::less<>> playlists;
      if (playlistLoaded)
      {
        playlist.SetType("musicvideos");
        strCurrentFilterVideo = playlist.GetWhereClause(db, playlists);
      }

      CLog::Log(LOGINFO, "PARTY MODE MANAGER: Registering filter:[{}]", strCurrentFilterVideo);
      videocount = db.GetRandomMusicVideoIDs(strCurrentFilterVideo, songIDs2);
      m_iMatchingSongs += static_cast<int>(videocount);
      if (m_iMatchingSongs < 1)
      {
        pDialog->Close();
        db.Close();
        OnError(16031, "Party mode found no matching songs. Aborting.");
        return false;
      }
    }
    else
    {
      pDialog->Close();
      OnError(16033, "Party mode could not open database. Aborting.");
      return false;
    }
    db.Close();
    m_songIDCache.insert(m_songIDCache.end(), songIDs2.begin(), songIDs2.end());
  }

  // Songs and music videos are random from query, but need mixing together when have both
  if (songcount > 0 && videocount > 0 )
    KODI::UTILS::RandomShuffle(m_songIDCache.begin(), m_songIDCache.end());

  CLog::Log(LOGINFO,"PARTY MODE MANAGER: Matching songs = {0}", m_iMatchingSongs);
  CLog::Log(LOGINFO,"PARTY MODE MANAGER: Party mode enabled!");

  const auto playLists = CServiceBroker::GetAppComponents().GetComponent<CApplicationPlayLists>();
  PLAYLIST::CPlayList& playList = playLists->GetPlayList(GetSide());
  playList.Clear();
  playList.SetShuffle(std::make_unique<PLAYLIST::CPlayListNoShuffle>());
  playList.ClearRepeats();
  playList.SetWrap(PLAYLIST::Wrap::None());

  pDialog->SetLine(0, CVariant{m_bIsVideo ? 20252 : 20124});
  pDialog->Progress();
  // add initial songs
  if (!AddRandomSongs())
  {
    pDialog->Close();
    return false;
  }

  auto end = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  CLog::Log(LOGDEBUG, "{} time for song fetch: {} ms", __FUNCTION__, duration.count());

  // start playing
  playLists->SetPlayingType(GetSide());
  Play(0);

  pDialog->Close();
  // open now playing window
  if (StringUtils::EqualsNoCase(m_type, "songs"))
  {
    if (CServiceBroker::GetGUI()->GetWindowManager().GetActiveWindow() != WINDOW_MUSIC_PLAYLIST)
      CServiceBroker::GetGUI()->GetWindowManager().ActivateWindow(WINDOW_MUSIC_PLAYLIST);
  }

  // done
  m_bEnabled = true;
  Announce();
  return true;
}

void CPartyModeManager::Disable()
{
  if (!IsEnabled())
    return;
  m_bEnabled = false;
  Announce();
  // The playing song runs to its end.
  CServiceBroker::GetAppComponents().GetComponent<CApplicationPlayLists>()->GetPlayList(GetSide()).Clear();
  CLog::Log(LOGINFO,"PARTY MODE MANAGER: Party mode disabled.");
}

void CPartyModeManager::OnSongChange(bool bUpdatePlayed /* = false */)
{
  if (!IsEnabled())
    return;
  Process();
  if (bUpdatePlayed)
    m_iSongsPlayed++;
}

void CPartyModeManager::AddUserSongs(CFileItemList& tempList, bool bPlay /* = false */)
{
  if (!IsEnabled())
    return;

  CLog::Log(LOGINFO, "PARTY MODE MANAGER: Adding {} user selected songs to play next",
            tempList.Size());

  const auto playLists = CServiceBroker::GetAppComponents().GetComponent<CApplicationPlayLists>();
  playLists->GetPlayList(GetSide()).PlayNext(tempList);

  if (bPlay)
    playLists->PlayNext();
}
void CPartyModeManager::Process()
{
  AddRandomSongs();
  UpdateStats();
  SendUpdateMessage();
}

bool CPartyModeManager::AddRandomSongs()
{
  // All songs have been picked, no more to add
  if (static_cast<int>(m_songIDCache.size()) == m_iMatchingSongsPicked)
    return false;

  const PLAYLIST::CPlayList& playlist = CServiceBroker::GetAppComponents()
                                             .GetComponent<CApplicationPlayLists>()
                                             ->GetPlayList(GetSide());
  const int ahead = playlist.size() - playlist.GetCurrentPosition() - 1;
  int iMissingSongs = QUEUE_DEPTH - ahead;

  if (iMissingSongs > 0)
  {
    // Limit songs fetched to remainder of songID cache
    iMissingSongs = std::min(iMissingSongs, static_cast<int>(m_songIDCache.size()) - m_iMatchingSongsPicked);

    // Pick iMissingSongs from remaining songID cache
    std::string sqlWhereMusic = "songview.idSong IN (";
    std::string sqlWhereVideo = "idMVideo IN (";

    bool bSongs = false;
    bool bMusicVideos = false;
    for (int i = m_iMatchingSongsPicked; i < m_iMatchingSongsPicked + iMissingSongs; i++)
    {
      std::string song = StringUtils::Format("{},", m_songIDCache[i].second);
      if (m_songIDCache[i].first == 1)
      {
        sqlWhereMusic += song;
        bSongs = true;
      }
      else if (m_songIDCache[i].first == 2)
      {
        sqlWhereVideo += song;
        bMusicVideos = true;
      }
    }
    CFileItemList items;

    if (bSongs)
    {
      sqlWhereMusic.back() = ')'; // replace the last comma with closing bracket
      // Apply random sort (and limit) at db query for efficiency
      SortDescription SortDescription;
      SortDescription.sortBy = SortBy::RANDOM;
      SortDescription.limitEnd = QUEUE_DEPTH;
      CMusicDatabase database;
      if (database.Open())
      {
        database.GetSongsFullByWhere("musicdb://songs/", items, SortDescription,
                                     CDatabase::Filter(sqlWhereMusic), true);

        // Get artist and album properties for songs
        for (auto& item : items)
          database.SetPropertiesForFileItem(*item);
        database.Close();
      }
      else
      {
        OnError(16033, "Party mode could not open database. Aborting.");
        return false;
      }
    }
    if (bMusicVideos)
    {
      sqlWhereVideo.back() = ')'; // replace the last comma with closing bracket
      CVideoDatabase database;
      if (database.Open())
      {
        database.GetMusicVideosByWhere("videodb://musicvideos/titles/",
          CDatabase::Filter(sqlWhereVideo), items);
        database.Close();
      }
      else
      {
        OnError(16033, "Party mode could not open database. Aborting.");
        return false;
      }
    }

    // Randomize if the list has music videos or they will be in db order
    // Songs only are already random.
    if (bMusicVideos)
      items.Randomize();
    for (const auto& item : items)
    {
      // Update songID cache with order items in playlist
      if (item->HasMusicInfoTag())
      {
        m_songIDCache[m_iMatchingSongsPicked].first = 1;
        m_songIDCache[m_iMatchingSongsPicked].second = item->GetMusicInfoTag()->GetDatabaseId();
      }
      else if (item->HasVideoInfoTag())
      {
        m_songIDCache[m_iMatchingSongsPicked].first = 2;
        m_songIDCache[m_iMatchingSongsPicked].second = item->GetVideoInfoTag()->m_iDbId;
      }
      CFileItemPtr pItem(item);
      Add(pItem); // inc m_iMatchingSongsPicked
    }
  }
  return true;
}

void CPartyModeManager::Add(CFileItemPtr &pItem)
{
  PLAYLIST::CPlayList& playlist =
      CServiceBroker::GetAppComponents().GetComponent<CApplicationPlayLists>()->GetPlayList(GetSide());
  playlist.Add(pItem);
  CLog::Log(LOGINFO, "PARTY MODE MANAGER: Adding randomly selected song at {}:[{}]",
            playlist.size() - 1, pItem->GetPath());
  m_iMatchingSongsPicked++;
}

void CPartyModeManager::SendUpdateMessage()
{
  CGUIMessage msg(GUI_MSG_PLAYLIST_CHANGED, 0, 0);
  CServiceBroker::GetGUI()->GetWindowManager().SendThreadMessage(msg);
}

void CPartyModeManager::Play(int iPos)
{
  // Playlist filled up by OnSongChange call from application GUI_MSG_PLAYBACK_STARTED processing
  CServiceBroker::GetAppComponents().GetComponent<CApplicationPlayLists>()->Play(GetSide(), iPos);
  CLog::Log(LOGINFO, "PARTY MODE MANAGER: Playing song at {}", iPos);
}

void CPartyModeManager::OnError(int iError, const std::string&  strLogMessage)
{
  // open error dialog
  HELPERS::ShowOKDialogLines(CVariant{257}, CVariant{16030}, CVariant{iError}, CVariant{0});
  CLog::Log(LOGERROR, "PARTY MODE MANAGER: {}", strLogMessage);
  m_bEnabled = false;
  SendUpdateMessage();
}

int CPartyModeManager::GetSongsPlayed()
{
  if (!IsEnabled())
    return -1;
  return m_iSongsPlayed;
}

int CPartyModeManager::GetMatchingSongs()
{
  if (!IsEnabled())
    return -1;
  return m_iMatchingSongs;
}

int CPartyModeManager::GetMatchingSongsPicked()
{
  if (!IsEnabled())
    return -1;
  return m_iMatchingSongsPicked;
}

int CPartyModeManager::GetMatchingSongsLeft()
{
  if (!IsEnabled())
    return -1;
  return m_iMatchingSongsLeft;
}

int CPartyModeManager::GetRelaxedSongs()
{
  if (!IsEnabled())
    return -1;
  return m_iRelaxedSongs;
}

int CPartyModeManager::GetRandomSongs()
{
  if (!IsEnabled())
    return -1;
  return m_iRandomSongs;
}

PartyModeContext CPartyModeManager::GetType() const
{
  if (!IsEnabled())
    return PartyModeContext::UNKNOWN;

  if (m_bIsVideo)
    return PartyModeContext::VIDEO;

  return PartyModeContext::MUSIC;
}

void CPartyModeManager::ClearState()
{
  m_iSongsPlayed = 0;
  m_iMatchingSongs = 0;
  m_iMatchingSongsPicked = 0;
  m_iMatchingSongsLeft = 0;
  m_iRelaxedSongs = 0;
  m_iRandomSongs = 0;

  m_songIDCache.clear();
}

void CPartyModeManager::UpdateStats()
{
  m_iMatchingSongsLeft = m_iMatchingSongs - m_iMatchingSongsPicked;
  m_iRandomSongs = m_iMatchingSongsPicked;
  m_iRelaxedSongs = 0;  // unsupported at this stage
}

bool CPartyModeManager::IsEnabled(PartyModeContext context /* = PARTYMODECONTEXT_UNKNOWN */) const
{
  if (!m_bEnabled) return false;
  if (context == PartyModeContext::VIDEO)
    return m_bIsVideo;
  if (context == PartyModeContext::MUSIC)
    return !m_bIsVideo;
  return true; // unknown, but we're enabled
}

void CPartyModeManager::Announce()
{
  CServiceBroker::GetAppComponents().GetComponent<CApplicationPlayLists>()->Announce(
      CApplicationPlayLists::PlayerProperty::PartyMode, m_bEnabled);
}

PLAYLIST::Type CPartyModeManager::GetSide() const
{
  return m_bIsVideo ? PLAYLIST::Video : PLAYLIST::Audio;
}
