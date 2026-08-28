/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "playlists/PlayListTypes.h"

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

/*! \brief Everything a playerid is resolved against.
 */
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

 A property of the player itself, so Player.GetActivePlayers publishes the id every other
 Player method accepts. It shares a range with the playlist ids, which is why the two are
 easily confused, but it does not follow whichever playlist the player is working through.

 \param player the player being addressed
 \return the playerid
 */
KODI::PLAYLIST::Id PlayerIdOf(PlayerType player);

/*! \brief The player a client's playerid names.

 Whether that player is running is not asked here. The notifications a client is handed still
 carry playlist-derived ids, so refusing an id for a player that is not running would reject
 ids Kodi itself has just published.

 \param playerid the playerid as the client gave it
 \return the player, or None when the id names none
 */
PlayerType PlayerForId(KODI::PLAYLIST::Id playerid);

/*! \brief The playlist a player is working through.

 What GoTo, SetShuffle, SetRepeat and the playlistid and position properties ask about,
 and unrelated to the id the player is addressed by.

 \param player the player
 \param state the players running and the playlists in force
 \return the playlist
 */
KODI::PLAYLIST::Id PlaylistOf(PlayerType player, const PlayerState& state);

} // namespace JSONRPC
