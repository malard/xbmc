/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "JSONRPCUtils.h"
#include "playlists/PlayListTypes.h"

class CVariant;

namespace JSONRPC
{

enum PlayerType
{
  None = 0,
  Video = 0x1,
  Audio = 0x2,
  Picture = 0x4,
  External = 0x8,
  Remote = 0x10
};

//! Everything a playerid is resolved against.
struct PlayerState
{
  //! The players running, as PlayerType flags.
  int players{None};

  //! The playlist the playlist player is working through, TYPE_NONE when it has none.
  KODI::PLAYLIST::Id currentPlaylist{KODI::PLAYLIST::Id::TYPE_NONE};

  //! The playlist the running player would belong to, TYPE_NONE when nothing plays.
  KODI::PLAYLIST::Id preferredPlaylist{KODI::PLAYLIST::Id::TYPE_NONE};
};

/*! \brief The playerid a client addresses a player by.

 Published by Player.GetActivePlayers and accepted by every other Player method. Shares a
 range with the playlist ids but is independent of the playlist the player is working through.
 */
KODI::PLAYLIST::Id PlayerIdOf(PlayerType player);

//! None when the id names no player. The player need not be running.
PlayerType PlayerForId(KODI::PLAYLIST::Id playerid);

/*! \brief The player a request addresses, by playerid or by playlistid.

 Either id is TYPE_NONE when the request did not give it.

 \return OK; InvalidParams for both or neither; Unavailable for a player that is not
         running, or a playlist no running player is working through
 */
JSONRPC_STATUS ResolvePlayer(KODI::PLAYLIST::Id playerid,
                             KODI::PLAYLIST::Id playlistid,
                             const PlayerState& state,
                             PlayerType& player);

//! Fills the "player" member of a Player notification. TYPE_NONE for the player's own playlist.
void DescribePlayer(CVariant& player, PlayerType type, KODI::PLAYLIST::Id playlist);

//! What GoTo, SetShuffle, SetRepeat and the playlistid and position properties ask about.
KODI::PLAYLIST::Id PlaylistOf(PlayerType player, const PlayerState& state);

} // namespace JSONRPC
