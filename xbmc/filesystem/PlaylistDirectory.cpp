/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "PlaylistDirectory.h"

#include "FileItem.h"
#include "FileItemList.h"
#include "ServiceBroker.h"
#include "URL.h"
#include "application/ApplicationComponents.h"
#include "application/ApplicationPlayLists.h"
#include "playlists/PlayList.h"

using namespace KODI;
using namespace XFILE;

CPlaylistDirectory::CPlaylistDirectory() = default;

CPlaylistDirectory::~CPlaylistDirectory() = default;

bool CPlaylistDirectory::GetDirectory(const CURL& url, CFileItemList &items)
{
  std::optional<PLAYLIST::Side> side;
  if (url.IsProtocol("playlistmusic"))
    side = PLAYLIST::Side::Audio;
  else if (url.IsProtocol("playlistvideo"))
    side = PLAYLIST::Side::Video;

  if (!side)
    return false;

  const PLAYLIST::CPlayList& playlist =
      CServiceBroker::GetAppComponents().GetComponent<CApplicationPlayLists>()->GetPlayList(*side);
  items.Reserve(playlist.size());

  for (int i = 0; i < playlist.size(); ++i)
  {
    CFileItemPtr item = playlist[i];
    if (!item)
      break;
    item->SetProperty("playlistposition", i);
    item->SetProperty("playlisttype", static_cast<int>(PLAYLIST::IdFromSide(side)));
    items.Add(item);
  }

  return true;
}
