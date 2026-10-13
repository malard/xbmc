/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "PlaybackValues.h"

#include "cores/VideoPlayer/Interface/StreamInfo.h"
#include "utils/Variant.h"

namespace KODI::INTERFACES
{

void MillisecondsToTimeObject(int time, CVariant& result)
{
  int ms = time % 1000;
  result["milliseconds"] = ms;
  time = (time - ms) / 1000;

  int s = time % 60;
  result["seconds"] = s;
  time = (time - s) / 60;

  int m = time % 60;
  result["minutes"] = m;
  time = (time - m) / 60;

  result["hours"] = time;
}

CVariant StreamToObject(int index, const AudioStreamInfo& info)
{
  CVariant stream(CVariant::VariantTypeObject);
  stream["index"] = index;
  stream["name"] = info.name;
  stream["language"] = info.language.ToString();
  stream["codec"] = info.codecName;
  stream["bitrate"] = info.bitrate;
  stream["channels"] = info.channels;
  stream["sampleRate"] = info.samplerate;
  stream["bitsPerSample"] = info.bitspersample;
  stream["isDefault"] = (info.flags & StreamFlags::FLAG_DEFAULT) != 0;
  stream["isOriginal"] = (info.flags & StreamFlags::FLAG_ORIGINAL) != 0;
  stream["isImpaired"] = (info.flags & StreamFlags::FLAG_VISUAL_IMPAIRED) != 0;
  return stream;
}

CVariant StreamToObject(int index, const VideoStreamInfo& info)
{
  CVariant stream(CVariant::VariantTypeObject);
  stream["index"] = index;
  stream["name"] = info.name;
  stream["language"] = info.language.ToString();
  stream["codec"] = info.codecName;
  stream["width"] = info.width;
  stream["height"] = info.height;
  return stream;
}

CVariant StreamToObject(int index, const SubtitleStreamInfo& info)
{
  CVariant stream(CVariant::VariantTypeObject);
  stream["index"] = index;
  stream["name"] = info.name;
  stream["language"] = info.language.ToString();
  stream["codec"] = info.codecName;
  stream["isDefault"] = (info.flags & StreamFlags::FLAG_DEFAULT) != 0;
  stream["isForced"] = (info.flags & StreamFlags::FLAG_FORCED) != 0;
  stream["isImpaired"] = (info.flags & StreamFlags::FLAG_HEARING_IMPAIRED) != 0;
  return stream;
}

} // namespace KODI::INTERFACES
