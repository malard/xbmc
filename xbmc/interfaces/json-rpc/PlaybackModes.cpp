/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "PlaybackModes.h"

#include "utils/Variant.h"

#include <string>

using namespace KODI;

namespace JSONRPC
{

std::optional<bool> ParseShuffleState(const CVariant& shuffle, bool current)
{
  bool requested;
  if (shuffle.isBoolean())
    requested = shuffle.asBoolean();
  else if (shuffle.isString() && shuffle.asString() == "toggle")
    requested = !current;
  else
    return std::nullopt;

  if (requested == current)
    return std::nullopt;

  return requested;
}

PLAYLIST::RepeatState ParseRepeatState(const CVariant& repeat)
{
  const std::string state = repeat.asString();

  if (state == "one")
    return PLAYLIST::RepeatState::ONE;
  if (state == "all")
    return PLAYLIST::RepeatState::ALL;

  return PLAYLIST::RepeatState::NONE;
}

PLAYLIST::RepeatState ParseRepeatState(const CVariant& repeat, PLAYLIST::RepeatState current)
{
  if (repeat.asString() != "cycle")
    return ParseRepeatState(repeat);

  switch (current)
  {
    case PLAYLIST::RepeatState::NONE:
      return PLAYLIST::RepeatState::ALL;
    case PLAYLIST::RepeatState::ALL:
      return PLAYLIST::RepeatState::ONE;
    default:
      return PLAYLIST::RepeatState::NONE;
  }
}

} // namespace JSONRPC
