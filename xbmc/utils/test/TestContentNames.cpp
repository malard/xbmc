/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "media/MediaType.h"
#include "utils/ContentNames.h"

#include <gtest/gtest.h>

using namespace KODI;
using KODI::MEDIA::MediaType;
using KODI::MEDIA::PluralNameOf;

// A list is given its media type's plural as its content in places, so the two must agree
TEST(TestContentNames, AMediaTypesPluralIsItsContent)
{
  EXPECT_EQ(PluralNameOf(MediaType::ALBUM), CONTENT::ALBUMS);
  EXPECT_EQ(PluralNameOf(MediaType::ARTIST), CONTENT::ARTISTS);
  EXPECT_EQ(PluralNameOf(MediaType::SONG), CONTENT::SONGS);
  EXPECT_EQ(PluralNameOf(MediaType::MOVIE), CONTENT::MOVIES);
  EXPECT_EQ(PluralNameOf(MediaType::TV_SHOW), CONTENT::TVSHOWS);
  EXPECT_EQ(PluralNameOf(MediaType::EPISODE), CONTENT::EPISODES);
  EXPECT_EQ(PluralNameOf(MediaType::MUSIC_VIDEO), CONTENT::MUSICVIDEOS);
}
