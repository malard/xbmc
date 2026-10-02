/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "media/MediaType.h"
#include "music/tags/MusicInfoTag.h"
#include "video/VideoInfoTag.h"

#include <gtest/gtest.h>

using namespace KODI::MEDIA;

namespace
{
constexpr MediaType FIRST{MediaType::MUSIC};
constexpr MediaType LAST{MediaType::VIDEO_VERSION};

MediaType Next(MediaType type)
{
  return static_cast<MediaType>(static_cast<int>(type) + 1);
}
} // namespace

TEST(TestMediaType, EveryTypeReadsBackFromItsNames)
{
  for (MediaType type{FIRST}; type <= LAST; type = Next(type))
  {
    EXPECT_FALSE(NameOf(type).empty());
    EXPECT_EQ(MediaTypeFromName(NameOf(type)), type) << NameOf(type);
    EXPECT_EQ(MediaTypeFromName(PluralNameOf(type)), type) << PluralNameOf(type);
  }
}

TEST(TestMediaType, NamesAreTheStoredSpellings)
{
  EXPECT_EQ(NameOf(MediaType::VIDEO_COLLECTION), "set");
  EXPECT_EQ(PluralNameOf(MediaType::VIDEO_COLLECTION), "sets");
  EXPECT_EQ(NameOf(MediaType::TV_SHOW), "tvshow");
  EXPECT_EQ(PluralNameOf(MediaType::MUSIC), "music");
  EXPECT_EQ(NameOf(MediaType::NONE), "");
}

TEST(TestMediaType, AFacetOrUnknownNameIsNoType)
{
  EXPECT_EQ(MediaTypeFromName(""), MediaType::NONE);
  EXPECT_EQ(MediaTypeFromName("genre"), MediaType::NONE);
  EXPECT_EQ(MediaTypeFromName("actor"), MediaType::NONE);
  EXPECT_EQ(MediaTypeFromName("year"), MediaType::NONE);
}

TEST(TestMediaType, ANameIsReadInAnyCase)
{
  EXPECT_EQ(MediaTypeFromName("Movie"), MediaType::MOVIE);
  EXPECT_EQ(MediaTypeFromName("TVSHOWS"), MediaType::TV_SHOW);
}

TEST(TestMediaType, OnlyTheStoredNameIsTheTypeExactly)
{
  for (MediaType type{FIRST}; type <= LAST; type = Next(type))
    EXPECT_EQ(MediaTypeOf(NameOf(type)), type) << NameOf(type);
  EXPECT_EQ(MediaTypeOf("movies"), MediaType::NONE);
  EXPECT_EQ(MediaTypeOf("Movie"), MediaType::NONE);
  EXPECT_EQ(MediaTypeOf("genre"), MediaType::NONE);
  EXPECT_EQ(MediaTypeOf(""), MediaType::NONE);
}

TEST(TestMediaType, ContainersHoldOtherItems)
{
  EXPECT_TRUE(IsContainer(MediaType::ALBUM));
  EXPECT_TRUE(IsContainer(MediaType::TV_SHOW));
  EXPECT_TRUE(IsContainer(MediaType::VIDEO_COLLECTION));
  EXPECT_FALSE(IsContainer(MediaType::MOVIE));
  EXPECT_FALSE(IsContainer(MediaType::SONG));
  EXPECT_FALSE(IsContainer(MediaType::NONE));
}

TEST(TestMediaType, ATagHasATypeOnlyForTheExactStoredName)
{
  CVideoInfoTag video;
  video.m_type = "movie";
  EXPECT_EQ(video.GetMediaType(), MediaType::MOVIE);
  video.m_type = "movies";
  EXPECT_EQ(video.GetMediaType(), MediaType::NONE);
  video.m_type = "Movie";
  EXPECT_EQ(video.GetMediaType(), MediaType::NONE);
  video.m_type = "genre";
  EXPECT_EQ(video.GetMediaType(), MediaType::NONE);

  MUSIC_INFO::CMusicInfoTag music;
  music.SetType("album");
  EXPECT_EQ(music.GetMediaType(), MediaType::ALBUM);
  music.SetType("artists");
  EXPECT_EQ(music.GetMediaType(), MediaType::NONE);
}
