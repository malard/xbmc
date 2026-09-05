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

#include <optional>

class CVariant;

namespace JSONRPC
{

//! Also accepts "toggle". Nothing when the value names no state, or the one already in force.
std::optional<bool> ParseShuffleState(const CVariant& shuffle, bool current);

//! Anything the enum does not name reads as RepeatState::NONE.
KODI::PLAYLIST::RepeatState ParseRepeatState(const CVariant& repeat);

//! Also accepts "cycle", which steps none -> all -> one -> none from the current state.
KODI::PLAYLIST::RepeatState ParseRepeatState(const CVariant& repeat,
                                             KODI::PLAYLIST::RepeatState current);

void ApplyShuffle(KODI::PLAYLIST::Id playlistId, const CVariant& shuffle);

void ApplyRepeat(KODI::PLAYLIST::Id playlistId, const CVariant& repeat);

//! FailedToExecute when asked to unshuffle: a running slideshow cannot be.
JSONRPC_STATUS ShuffleSlideshow(const CVariant& shuffle);

} // namespace JSONRPC
