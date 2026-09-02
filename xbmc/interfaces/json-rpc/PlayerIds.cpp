/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "PlayerIds.h"

using namespace KODI;

namespace JSONRPC
{

PLAYLIST::Id PlayerIdOf(PlayerType player)
{
  switch (player)
  {
    case Video:
      return PLAYLIST::Id::TYPE_VIDEO;

    case Audio:
      return PLAYLIST::Id::TYPE_MUSIC;

    case Picture:
      return PLAYLIST::Id::TYPE_PICTURE;

    default:
      return PLAYLIST::Id::TYPE_NONE;
  }
}

PlayerType PlayerForId(PLAYLIST::Id playerid)
{
  switch (playerid)
  {
    case PLAYLIST::Id::TYPE_VIDEO:
      return Video;

    case PLAYLIST::Id::TYPE_MUSIC:
      return Audio;

    case PLAYLIST::Id::TYPE_PICTURE:
      return Picture;

    default:
      return None;
  }
}

PLAYLIST::Id PlaylistOf(PlayerType player, const PlayerState& state)
{
  PLAYLIST::Id playlistId = state.currentPlaylist;
  if (playlistId == PLAYLIST::Id::TYPE_NONE) // No active playlist, try guessing
    playlistId = state.preferredPlaylist;

  switch (player)
  {
    case Video:
      return playlistId == PLAYLIST::Id::TYPE_NONE ? PLAYLIST::Id::TYPE_VIDEO : playlistId;

    case Audio:
      return playlistId == PLAYLIST::Id::TYPE_NONE ? PLAYLIST::Id::TYPE_MUSIC : playlistId;

    case Picture:
      return PLAYLIST::Id::TYPE_PICTURE;

    default:
      return playlistId;
  }
}

} // namespace JSONRPC
