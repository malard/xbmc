/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

class CVariant;
struct AudioStreamInfo;
struct SubtitleStreamInfo;
struct VideoStreamInfo;

/*!
 * \brief How playback state is written into what the notifications and remote interfaces publish.
 */
namespace KODI::INTERFACES
{

//! Write a time in milliseconds as hours, minutes, seconds and milliseconds.
void MillisecondsToTimeObject(int time, CVariant& result);

CVariant StreamToObject(int index, const AudioStreamInfo& info);
CVariant StreamToObject(int index, const VideoStreamInfo& info);
CVariant StreamToObject(int index, const SubtitleStreamInfo& info);

} // namespace KODI::INTERFACES
