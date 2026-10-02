/*
 *  Copyright (C) 2015-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "MediaLibraryEvent.h"

#include "ServiceBroker.h"
#include "filesystem/SourcesDirectory.h"
#include "guilib/GUIComponent.h"
#include "guilib/GUIWindowManager.h"
#include "guilib/WindowIDs.h"
#include "music/MusicDbPaths.h"
#include "resources/LocalizeStrings.h"
#include "resources/ResourcesComponent.h"
#include "video/VideoDbPaths.h"

#include <optional>
#include <string>

using KODI::MEDIA::MediaType;

namespace
{
struct Destination
{
  int window;
  std::string root;
};

std::optional<Destination> DestinationFor(MediaType type)
{
  switch (type)
  {
    case MediaType::VIDEO:
      return Destination{WINDOW_VIDEO_NAV,
                         XFILE::CSourcesDirectory::PathOf(KODI::MEDIA::MediaSection::VIDEO)};
    case MediaType::MOVIE:
      return Destination{WINDOW_VIDEO_NAV, KODI::VIDEODB::MOVIE_TITLES};
    case MediaType::VIDEO_COLLECTION:
      return Destination{WINDOW_VIDEO_NAV, KODI::VIDEODB::MOVIE_SETS};
    case MediaType::MUSIC_VIDEO:
      return Destination{WINDOW_VIDEO_NAV, KODI::VIDEODB::MUSICVIDEO_TITLES};
    case MediaType::TV_SHOW:
    case MediaType::SEASON:
      return Destination{WINDOW_VIDEO_NAV, KODI::VIDEODB::TVSHOW_TITLES};
    case MediaType::EPISODE:
      return Destination{WINDOW_VIDEO_NAV, KODI::VIDEODB::TVSHOW_TITLES};
    case MediaType::MUSIC:
      return Destination{WINDOW_MUSIC_NAV,
                         XFILE::CSourcesDirectory::PathOf(KODI::MEDIA::MediaSection::MUSIC)};
    case MediaType::ARTIST:
      return Destination{WINDOW_MUSIC_NAV, KODI::MUSICDB::ARTISTS};
    case MediaType::ALBUM:
      return Destination{WINDOW_MUSIC_NAV, KODI::MUSICDB::ALBUMS};
    case MediaType::SONG:
      return Destination{WINDOW_MUSIC_NAV, KODI::MUSICDB::SONGS};
    case MediaType::NONE:
    case MediaType::VIDEO_VERSION:
      break;
  }
  return {};
}
} // namespace

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
                                       const CVariant& details,
                                       EventLevel level /* = EventLevel::Information */)
  : CUniqueEvent(label, description, icon, details, level),
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

  const std::optional<Destination> destination{DestinationFor(m_mediaType)};
  if (!destination)
    return false;

  const std::string path{m_mediaPath.empty() ? std::string{destination->root} : m_mediaPath};
  CServiceBroker::GetGUI()->GetWindowManager().ActivateWindow(destination->window,
                                                              {path, "return"});
  return true;
}
