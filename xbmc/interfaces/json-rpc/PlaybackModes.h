/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "playlists/PlayListTypes.h"

#include <optional>

class CVariant;

namespace JSONRPC
{

/*! \brief Translate the "shuffle" parameter into the state it asks for.

 The parameter is either the state itself or "toggle", so what it means depends on the state
 the target is already in.

 \param shuffle the parameter as given by the client
 \param current whether the target is shuffled at the moment
 \return the requested state, or nothing when that is the current state already
 */
std::optional<bool> ParseShuffleState(const CVariant& shuffle, bool current);

/*! \brief Translate the "repeat" parameter into the state it names.
 \param repeat the parameter as given by the client
 \return the named state, or RepeatState::NONE for anything else
 */
KODI::PLAYLIST::RepeatState ParseRepeatState(const CVariant& repeat);

/*! \brief Translate the "repeat" parameter into the state it asks for.

 As ParseRepeatState above, but also accepts "cycle", which steps none -> all -> one -> none
 from the state the target is already in.

 \param repeat the parameter as given by the client
 \param current the state of the target at the moment
 \return the requested state
 */
KODI::PLAYLIST::RepeatState ParseRepeatState(const CVariant& repeat,
                                             KODI::PLAYLIST::RepeatState current);

} // namespace JSONRPC
