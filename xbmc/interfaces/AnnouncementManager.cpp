/*
 *      Copyright (C) 2005-2010 Team XBMC
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "AnnouncementManager.h"
#include "threads/SingleLock.h"
#include <stdio.h>
#include "utils/log.h"
#include "utils/Variant.h"
#include "FileItem.h"
#include "music/tags/MusicInfoTag.h"
#include "video/VideoDatabase.h"

using namespace std;
using namespace ANNOUNCEMENT;

CCriticalSection CAnnouncementManager::m_critSection;
vector<IAnnouncer *> CAnnouncementManager::m_announcers;

void CAnnouncementManager::AddAnnouncer(IAnnouncer *listener)
{
  CSingleLock lock (m_critSection);
  m_announcers.push_back(listener);
}

void CAnnouncementManager::RemoveAnnouncer(IAnnouncer *listener)
{
  CSingleLock lock (m_critSection);
  for (unsigned int i = 0; i < m_announcers.size(); i++)
  {
    if (m_announcers[i] == listener)
    {
      m_announcers.erase(m_announcers.begin() + i);
      return;
    }
  }
}

void CAnnouncementManager::Announce(EAnnouncementFlag flag, const char *sender, const char *message)
{
  CVariant data;
  Announce(flag, sender, message, data);
}

void CAnnouncementManager::Announce(EAnnouncementFlag flag, const char *sender, const char *message, CVariant &data)
{
  CLog::Log(LOGDEBUG, "CAnnouncementManager - Announcement: %s from %s", message, sender);
  CSingleLock lock (m_critSection);
  for (unsigned int i = 0; i < m_announcers.size(); i++)
    m_announcers[i]->Announce(flag, sender, message, data);
}

void CAnnouncementManager::Announce(EAnnouncementFlag flag, const char *sender, const char *message, CFileItemPtr item)
{
  CVariant data;
  Announce(flag, sender, message, item, data);
}

void CAnnouncementManager::Announce(EAnnouncementFlag flag, const char *sender, const char *message, CFileItemPtr item, CVariant &data)
{
  // Extract db id of item
  CVariant object = data.isNull() || data.isObject() ? data : CVariant::VariantTypeObject;
  CStdString type;
  int id = 0;

  if (item->HasVideoInfoTag())
  {
    CVideoDatabase::VideoContentTypeToString((VIDEODB_CONTENT_TYPE)item->GetVideoContentType(), type);

    id = item->GetVideoInfoTag()->m_iDbId;

    if (id <= 0)
    {
      object["title"] = item->GetVideoInfoTag()->m_strTitle;

      switch (item->GetVideoContentType())
      {
      case VIDEODB_CONTENT_MOVIES:
        if (item->GetVideoInfoTag()->m_iYear > 0)
          object["year"] = item->GetVideoInfoTag()->m_iYear;
        break;
      case VIDEODB_CONTENT_EPISODES:
        if (item->GetVideoInfoTag()->m_iEpisode >= 0)
          object["episode"] = item->GetVideoInfoTag()->m_iEpisode;
        if (item->GetVideoInfoTag()->m_iSeason >= 0)
          object["season"] = item->GetVideoInfoTag()->m_iSeason;
        if (!item->GetVideoInfoTag()->m_strShowTitle.empty())
          object["showtitle"] = item->GetVideoInfoTag()->m_strShowTitle;
        break;
      case VIDEODB_CONTENT_MUSICVIDEOS:
        if (!item->GetVideoInfoTag()->m_strAlbum.empty())
          object["album"] = item->GetVideoInfoTag()->m_strAlbum;
        if (!item->GetVideoInfoTag()->m_strArtist.empty())
          object["artist"] = item->GetVideoInfoTag()->m_strArtist;
        break;
      }
    }
  }
  else if (item->HasMusicInfoTag())
  {
    id = item->GetMusicInfoTag()->GetDatabaseId();

    if (item->IsAlbum())
      type = "album";
    else
    {
      type = "song";

      if (id <= 0)
      {
        if (item->GetMusicInfoTag()->GetTrackNumber() > 0)
          object["track"] = item->GetMusicInfoTag()->GetTrackNumber();
        if (!item->GetMusicInfoTag()->GetAlbum().empty())
          object["album"] = item->GetMusicInfoTag()->GetAlbum();
      }
    }

    if (id <= 0)
    {
      object["title"] = item->GetMusicInfoTag()->GetTitle();
      if (!item->GetMusicInfoTag()->GetArtist().empty())
        object["artist"] = item->GetMusicInfoTag()->GetArtist();
    }
  }
  else
    type = "unknown";

  object["type"] = type;
  if (id > 0)
    object["id"] = id;

  Announce(flag, sender, message, object);
}
