/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "PartyMode.h"

#include "FileItem.h"
#include "FileItemList.h"
#include "GUIUserMessages.h"
#include "ServiceBroker.h"
#include "application/ApplicationPlayLists.h"
#include "dialogs/GUIDialogProgress.h"
#include "guilib/GUIComponent.h"
#include "guilib/GUIWindowManager.h"
#include "messaging/helpers/DialogOKHelper.h"
#include "music/MusicDatabase.h"
#include "music/MusicDbPaths.h"
#include "music/tags/MusicInfoTag.h"
#include "playlists/PlayListTypes.h"
#include "playlists/SmartFeed.h"
#include "playlists/SmartPlayList.h"
#include "profiles/ProfileManager.h"
#include "settings/SettingsComponent.h"
#include "utils/StringUtils.h"
#include "utils/Variant.h"
#include "utils/log.h"
#include "video/VideoDatabase.h"
#include "video/VideoDbPaths.h"
#include "video/VideoInfoTag.h"
#include "windows/GUIWindowPlayList.h"

#include <map>
#include <optional>
#include <set>

using namespace KODI;
using namespace KODI::MESSAGING;

namespace
{
using PARTYMODE::Library;
using PARTYMODE::Match;

/*!
 * \brief What the rules match of this content: "songs", "musicvideos" or "mixed". With no rules,
 * everything of the content matches.
 * \return nullopt if a database could not be opened.
 */
std::optional<std::vector<Match>> FindMatches(PLAYLIST::CSmartPlaylist* rules,
                                              const std::string& content)
{
  const bool songs =
      StringUtils::EqualsNoCase(content, "songs") || StringUtils::EqualsNoCase(content, "mixed");
  const bool musicVideos = StringUtils::EqualsNoCase(content, "musicvideos") ||
                           StringUtils::EqualsNoCase(content, "mixed");

  std::vector<Match> matches;
  if (songs)
  {
    CMusicDatabase db;
    if (!db.Open())
      return std::nullopt;
    std::string filter;
    if (rules)
    {
      std::set<std::string, std::less<>> playlists;
      rules->SetType("songs");
      filter = rules->GetWhereClause(db, playlists);
    }
    CLog::LogF(LOGINFO, "song filter [{}]", filter);
    std::vector<int> ids;
    db.GetRandomSongIDs(CDatabase::Filter(filter), ids);
    for (const int id : ids)
      matches.emplace_back(Library::Song, id);
  }

  if (musicVideos)
  {
    CVideoDatabase db;
    if (!db.Open())
      return std::nullopt;
    std::string filter;
    if (rules)
    {
      std::set<std::string, std::less<>> playlists;
      rules->SetType("musicvideos");
      filter = rules->GetWhereClause(db, playlists);
    }
    CLog::LogF(LOGINFO, "music video filter [{}]", filter);
    std::vector<int> ids;
    db.GetRandomMusicVideoIDs(filter, ids);
    for (const int id : ids)
      matches.emplace_back(Library::MusicVideo, id);
  }
  return matches;
}

std::string IdList(const std::vector<Match>& matches, Library library)
{
  std::string ids;
  for (const auto& [matchLibrary, id] : matches)
    if (matchLibrary == library)
      ids += StringUtils::Format("{},", id);
  if (!ids.empty())
    ids.pop_back();
  return ids;
}

std::vector<std::shared_ptr<CFileItem>> Fetch(const std::vector<Match>& matches)
{
  CFileItemList fetched;
  if (const std::string songIds = IdList(matches, Library::Song); !songIds.empty())
  {
    CMusicDatabase database;
    if (database.Open())
    {
      database.GetSongsFullByWhere(MUSICDB::SONGS, fetched, SortDescription{},
                                   CDatabase::Filter("songview.idSong IN (" + songIds + ")"), true);
      for (const auto& item : fetched)
        database.SetPropertiesForFileItem(*item);
    }
    else
      CLog::LogF(LOGERROR, "could not open the music database");
  }
  if (const std::string videoIds = IdList(matches, Library::MusicVideo); !videoIds.empty())
  {
    CVideoDatabase database;
    if (database.Open())
      database.GetMusicVideosByWhere(VIDEODB::MUSICVIDEO_TITLES,
                                     CDatabase::Filter("idMVideo IN (" + videoIds + ")"), fetched);
    else
      CLog::LogF(LOGERROR, "could not open the video database");
  }
  return PARTYMODE::InMatchOrder(matches, fetched);
}

bool Fail(int error, const std::string& message)
{
  HELPERS::ShowOKDialogLines(CVariant{257}, CVariant{16030}, CVariant{error}, CVariant{0});
  CLog::Log(LOGERROR, "PARTY MODE: {}, aborting", message);
  return false;
}

bool StartFeed(std::optional<PLAYLIST::Type> named, const std::string& xspPath)
{
  PLAYLIST::CSmartPlaylist rules;
  std::string rulesPath = xspPath;
  if (rulesPath.empty())
  {
    const std::shared_ptr<CProfileManager> profileManager =
        CServiceBroker::GetSettingsComponent()->GetProfileManager();
    rulesPath = profileManager->GetUserDataItem(named == PLAYLIST::Video ? "PartyMode-Video.xsp"
                                                                         : "PartyMode.xsp");
  }

  const bool rulesLoaded = rules.Load(rulesPath);
  const PLAYLIST::Type playList =
      named.value_or(rulesLoaded ? rules.GetPlayListType() : PLAYLIST::Audio);
  const bool isVideo = playList == PLAYLIST::Video;
  const std::string content = rulesLoaded ? rules.GetType() : (isVideo ? "musicvideos" : "songs");

  CGUIDialogProgress* dialog =
      CServiceBroker::GetGUI()->GetWindowManager().GetWindow<CGUIDialogProgress>(
          WINDOW_DIALOG_PROGRESS);
  dialog->SetHeading(CVariant{isVideo ? 20250 : 20121});
  dialog->SetLine(0, CVariant{isVideo ? 20251 : 20123});
  dialog->SetLine(1, CVariant{""});
  dialog->SetLine(2, CVariant{""});
  dialog->Open();

  std::optional<std::vector<Match>> found = FindMatches(rulesLoaded ? &rules : nullptr, content);
  if (!found)
  {
    dialog->Close();
    return Fail(16033, "could not open a database");
  }
  if (found->empty())
  {
    dialog->Close();
    return Fail(16031, "nothing matched");
  }

  const auto matches = std::make_shared<const std::vector<Match>>(std::move(*found));
  const auto feed = std::make_shared<PLAYLIST::CSmartFeed>(static_cast<int>(matches->size()),
                                                 [matches](const std::vector<int>& slice)
                                                 {
                                                   std::vector<Match> taken;
                                                   taken.reserve(slice.size());
                                                   for (const int entry : slice)
                                                     taken.push_back((*matches)[entry]);
                                                   return Fetch(taken);
                                                 });

  dialog->SetLine(0, CVariant{isVideo ? 20252 : 20124});
  dialog->Progress();

  // a party carries on past the end of its feed
  const bool playing =
      CServiceBroker::GetPlayLists()->PlayFeed(playList, feed, CApplicationPlayLists::Repeat::All);
  dialog->Close();
  if (!playing)
    return Fail(16031, "nothing could be placed");

  CLog::Log(LOGINFO, "PARTY MODE: started, {} matching", feed->GetTotal());
  if (playList == PLAYLIST::Audio)
    ShowPlayListWindow(PLAYLIST::Audio);
  return true;
}
} // namespace

namespace KODI::PARTYMODE
{

bool Start(PLAYLIST::Type playList)
{
  return StartFeed(playList, "");
}

bool Start(const std::string& rulesPath)
{
  return StartFeed(std::nullopt, rulesPath);
}

void Stop()
{
  CServiceBroker::GetPlayLists()->DropFeed();
}

bool IsRunning()
{
  return CServiceBroker::GetPlayLists()->GetFedType().has_value();
}

bool IsRunning(PLAYLIST::Type playList)
{
  return CServiceBroker::GetPlayLists()->GetFedType() == playList;
}

std::vector<std::shared_ptr<CFileItem>> InMatchOrder(const std::vector<Match>& matches,
                                                     const CFileItemList& fetched)
{
  std::map<Match, std::shared_ptr<CFileItem>> byMatch;
  for (const auto& item : fetched)
  {
    if (item->HasMusicInfoTag() && item->GetMusicInfoTag()->GetDatabaseId() > 0)
      byMatch.try_emplace({Library::Song, item->GetMusicInfoTag()->GetDatabaseId()}, item);
    else if (item->HasVideoInfoTag())
      byMatch.try_emplace({Library::MusicVideo, item->GetVideoInfoTag()->m_iDbId}, item);
  }

  std::vector<std::shared_ptr<CFileItem>> ordered;
  for (const Match& match : matches)
    if (const auto it = byMatch.find(match); it != byMatch.end())
      ordered.push_back(it->second);
  return ordered;
}

} // namespace KODI::PARTYMODE
