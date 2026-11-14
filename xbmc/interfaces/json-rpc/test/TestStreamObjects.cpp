/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "cores/VideoPlayer/Interface/StreamInfo.h"
#include "interfaces/PlaybackValues.h"
#include "utils/Variant.h"

#include <gtest/gtest.h>

using namespace KODI;

TEST(TestStreamObjects, AnAudioStreamIsImpairedWhenItDescribesThePicture)
{
  AudioStreamInfo info;
  info.flags = StreamFlags::FLAG_VISUAL_IMPAIRED;
  EXPECT_TRUE(INTERFACES::StreamToObject(1, info)["isImpaired"].asBoolean());

  info.flags = StreamFlags::FLAG_HEARING_IMPAIRED;
  EXPECT_FALSE(INTERFACES::StreamToObject(1, info)["isImpaired"].asBoolean());
}

TEST(TestStreamObjects, ASubtitleIsImpairedWhenItDescribesTheSound)
{
  SubtitleStreamInfo info;
  info.flags = StreamFlags::FLAG_HEARING_IMPAIRED;
  EXPECT_TRUE(INTERFACES::StreamToObject(1, info)["isImpaired"].asBoolean());

  info.flags = StreamFlags::FLAG_VISUAL_IMPAIRED;
  EXPECT_FALSE(INTERFACES::StreamToObject(1, info)["isImpaired"].asBoolean());
}

TEST(TestStreamObjects, AnAudioStreamCarriesTheCodecNameAndFormat)
{
  AudioStreamInfo info;
  info.codecName = "eac3";
  info.codecDesc = "Dolby Digital Plus";
  info.samplerate = 48000;
  info.bitspersample = 24;
  const CVariant stream = INTERFACES::StreamToObject(2, info);
  EXPECT_EQ(2, stream["index"].asInteger());
  EXPECT_EQ("eac3", stream["codec"].asString());
  EXPECT_EQ(48000, stream["sampleRate"].asInteger());
  EXPECT_EQ(24, stream["bitsPerSample"].asInteger());
}
