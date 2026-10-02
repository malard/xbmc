/*
 *  Copyright (C) 2015-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "MediaLibraryEvent.h"

#include "ServiceBroker.h"
#include "guilib/GUIComponent.h"
#include "guilib/GUIWindowManager.h"
#include "guilib/WindowIDs.h"
#include "resources/LocalizeStrings.h"
#include "resources/ResourcesComponent.h"
#include "utils/URIUtils.h"

using KODI::MEDIA::MediaType;

CMediaLibraryEvent::CMediaLibraryEvent(MediaType mediaType,
                                       const std::string& mediaPath,
                                       const CVariant& label,
                                       const CVariant& description,
                                       EventLevel level /* = EventLevel::Information */)
  : CUniqueEvent(label, description, level),
    m_mediaType(mediaType),
    m_mediaPath(mediaPath)
{ }

CMediaLibraryEvent::CMediaLibraryEvent(MediaType mediaType,
                                       const std::string& mediaPath,
                                       const CVariant& label,
                                       const CVariant& description,
                                       const std::string& icon,
                                       EventLevel level /* = EventLevel::Information */)
  : CUniqueEvent(label, description, icon, level),
    m_mediaType(mediaType),
    m_mediaPath(mediaPath)
{ }

CMediaLibraryEvent::CMediaLibraryEvent(MediaType mediaType,
                                       const std::string& mediaPath,
                                       const CVariant& label,
                                       const CVariant& description,
                                       const std::string& icon,
                                       const CVariant& details,
                                       EventLevel level /* = EventLevel::Information */)
  : CUniqueEvent(label, description, icon, details, level),
    m_mediaType(mediaType),
    m_mediaPath(mediaPath)
{ }

CMediaLibraryEvent::CMediaLibraryEvent(MediaType mediaType,
                                       const std::string& mediaPath,
                                       const CVariant& label,
                                       const CVariant& description,
                                       const std::string& icon,
                                       const CVariant& details,
                                       const CVariant& executionLabel,
                                       EventLevel level /* = EventLevel::Information */)
  : CUniqueEvent(label, description, icon, details, executionLabel, level),
    m_mediaType(mediaType),
    m_mediaPath(mediaPath)
{ }

std::string CMediaLibraryEvent::GetExecutionLabel() const
{
  std::string executionLabel = CUniqueEvent::GetExecutionLabel();
  if (!executionLabel.empty())
    return executionLabel;

  return CServiceBroker::GetResourcesComponent().GetLocalizeStrings().Get(24140);
}

bool CMediaLibraryEvent::Execute() const
{
  if (!CanExecute())
    return false;

  int windowId = -1;
  std::string path = m_mediaPath;
  if (m_mediaType == MediaType::VIDEO || m_mediaType == MediaType::MOVIE ||
      m_mediaType == MediaType::VIDEO_COLLECTION || m_mediaType == MediaType::TV_SHOW ||
      m_mediaType == MediaType::SEASON || m_mediaType == MediaType::EPISODE ||
      m_mediaType == MediaType::MUSIC_VIDEO)
  {
    if (path.empty())
    {
      if (m_mediaType == MediaType::VIDEO)
        path = "sources://video/";
      else if (m_mediaType == MediaType::MOVIE)
        path = "videodb://movies/titles/";
      else if (m_mediaType == MediaType::VIDEO_COLLECTION)
        path = "videodb://movies/sets/";
      else if (m_mediaType == MediaType::MUSIC_VIDEO)
        path = "videodb://musicvideos/titles/";
      else if (m_mediaType == MediaType::TV_SHOW || m_mediaType == MediaType::SEASON ||
               m_mediaType == MediaType::EPISODE)
        path = "videodb://tvshows/titles/";
    }
    else
    {
      //! @todo remove the filename for now as CGUIMediaWindow::GetDirectory() can't handle it
      if (m_mediaType == MediaType::MOVIE || m_mediaType == MediaType::MUSIC_VIDEO ||
          m_mediaType == MediaType::EPISODE)
        path = URIUtils::GetDirectory(path);
    }

    windowId = WINDOW_VIDEO_NAV;
  }
  else if (m_mediaType == MediaType::MUSIC || m_mediaType == MediaType::ARTIST ||
           m_mediaType == MediaType::ALBUM || m_mediaType == MediaType::SONG)
  {
    if (path.empty())
    {
      if (m_mediaType == MediaType::MUSIC)
        path = "sources://music/";
      else if (m_mediaType == MediaType::ARTIST)
        path = "musicdb://artists/";
      else if (m_mediaType == MediaType::ALBUM)
        path = "musicdb://albums/";
      else if (m_mediaType == MediaType::SONG)
        path = "musicdb://songs/";
    }
    else
    {
      //! @todo remove the filename for now as CGUIMediaWindow::GetDirectory() can't handle it
      if (m_mediaType == MediaType::SONG)
        path = URIUtils::GetDirectory(path);
    }

    windowId = WINDOW_MUSIC_NAV;
  }

  if (windowId < 0)
    return false;

  std::vector<std::string> params;
  params.push_back(path);
  params.emplace_back("return");
  CServiceBroker::GetGUI()->GetWindowManager().ActivateWindow(windowId, params);
  return true;
}
