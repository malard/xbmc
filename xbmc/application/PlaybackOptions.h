/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

namespace KODI::APPLICATION
{

//! Whether the file is opened again in place: a restart, another player, the next stack part.
enum class Reopen
{
  No,
  Yes
};

//! Whether this playback begins something the user started, rather than continuing it; only a
//! start may switch to fullscreen.
enum class StartsRun
{
  No,
  Yes
};

} // namespace KODI::APPLICATION
