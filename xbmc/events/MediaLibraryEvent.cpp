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

#include <optional>
#include <string_view>

using KODI::MEDIA::MediaType;

namespace
{
struct Destination
{
  int window;
  std::string_view root;
  //! An item's path names its file rather than its folder.
  bool pathIsAFile;
};

std::optional<Destination> DestinationFor(MediaType type)
{
  switch (type)
  {
    case MediaType::VIDEO:
      return Destination{WINDOW_VIDEO_NAV, "sources://video/", false};
    case MediaType::MOVIE:
      return Destination{WINDOW_VIDEO_NAV, "videodb://movies/titles/", true};
    case MediaType::VIDEO_COLLECTION:
      return Destination{WINDOW_VIDEO_NAV, "videodb://movies/sets/", false};
    case MediaType::MUSIC_VIDEO:
      return Destination{WINDOW_VIDEO_NAV, "videodb://musicvideos/titles/", true};
    case MediaType::TV_SHOW:
    case MediaType::SEASON:
      return Destination{WINDOW_VIDEO_NAV, "videodb://tvshows/titles/", false};
    case MediaType::EPISODE:
      return Destination{WINDOW_VIDEO_NAV, "videodb://tvshows/titles/", true};
    case MediaType::MUSIC:
      return Destination{WINDOW_MUSIC_NAV, "sources://music/", false};
    case MediaType::ARTIST:
      return Destination{WINDOW_MUSIC_NAV, "musicdb://artists/", false};
    case MediaType::ALBUM:
      return Destination{WINDOW_MUSIC_NAV, "musicdb://albums/", false};
    case MediaType::SONG:
      return Destination{WINDOW_MUSIC_NAV, "musicdb://songs/", true};
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

  std::string path{m_mediaPath};
  if (path.empty())
    path = destination->root;
  //! @todo remove the filename for now as CGUIMediaWindow::GetDirectory() can't handle it
  else if (destination->pathIsAFile)
    path = URIUtils::GetDirectory(path);

  CServiceBroker::GetGUI()->GetWindowManager().ActivateWindow(destination->window,
                                                              {path, "return"});
  return true;
}
