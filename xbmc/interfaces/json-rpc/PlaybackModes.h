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

/*!
 * \brief Set a playlist's shuffle: true, false or "toggle". Asking for the state in force changes
 * nothing.
 * \return ACK, or FailedToExecute if the playlist refused the change, as party mode's does.
 */
JSONRPC_STATUS ApplyShuffle(KODI::PLAYLIST::Type type, const CVariant& shuffle);

/*!
 * \brief Set a playlist's repeat: "off", "one", "all" or "cycle".
 * \return ACK, or FailedToExecute if the playlist refused the change.
 */
JSONRPC_STATUS ApplyRepeat(KODI::PLAYLIST::Type type, const CVariant& repeat);

//! FailedToExecute when asked to unshuffle: a running slideshow cannot be.
JSONRPC_STATUS ShuffleSlideshow(const CVariant& shuffle);

} // namespace JSONRPC
