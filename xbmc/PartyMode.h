/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

class CFileItem;
class CFileItemList;

namespace KODI::PLAYLIST
{
enum class Type;
}

/*!
 * \brief Party mode: a smart feed played in random order, wrapping, with anything queued playing
 * next. The party buttons, labels, builtin and JSON-RPC all report a playing feed as party mode.
 */
namespace KODI::PARTYMODE
{

/*!
 * \brief Play the party rules for this playlist, PartyMode.xsp or PartyMode-Video.xsp; with no
 * rules file, everything of the playlist's kind.
 */
bool Start(PLAYLIST::Type playList);

/*!
 * \brief Play the rules in a smart playlist file, on the playlist its type gives.
 */
bool Start(const std::string& rulesPath);

//! Stop party mode; see CApplicationPlayLists::DropFeed().
void Stop();

//! Whether a feed is playing on either playlist.
bool IsRunning();
//! Whether a feed is playing on this playlist.
bool IsRunning(PLAYLIST::Type playList);

enum class Library
{
  Song,
  MusicVideo
};
using Match = std::pair<Library, int>;

/*!
 * \brief The fetched items the matches name, in the matches' order. Matches that were not fetched
 * are left out.
 */
std::vector<std::shared_ptr<CFileItem>> InMatchOrder(const std::vector<Match>& matches,
                                                     const CFileItemList& fetched);

} // namespace KODI::PARTYMODE
