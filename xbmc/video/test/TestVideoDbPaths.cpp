/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "filesystem/VideoDatabaseDirectory.h"
#include "filesystem/VideoDatabaseDirectory/DirectoryNode.h"
#include "video/VideoDbPaths.h"

#include <utility>

#include <gtest/gtest.h>

using namespace KODI;
using XFILE::CVideoDatabaseDirectory;
using XFILE::VIDEODATABASEDIRECTORY::NodeType;

TEST(TestVideoDbPaths, EveryPathListsTheNodeItNames)
{
  const std::pair<const char*, NodeType> paths[] = {
      {VIDEODB::ROOT, NodeType::OVERVIEW},
      {VIDEODB::MOVIES, NodeType::MOVIES_OVERVIEW},
      {VIDEODB::MOVIE_GENRES, NodeType::GENRE},
      {VIDEODB::MOVIE_TITLES, NodeType::TITLE_MOVIES},
      {VIDEODB::MOVIE_YEARS, NodeType::YEAR},
      {VIDEODB::MOVIE_ACTORS, NodeType::ACTOR},
      {VIDEODB::MOVIE_DIRECTORS, NodeType::DIRECTOR},
      {VIDEODB::MOVIE_STUDIOS, NodeType::STUDIO},
      {VIDEODB::MOVIE_SETS, NodeType::SETS},
      {VIDEODB::MOVIE_COUNTRIES, NodeType::COUNTRY},
      {VIDEODB::MOVIE_TAGS, NodeType::TAGS},
      {VIDEODB::MOVIE_VIDEO_VERSIONS, NodeType::VIDEOVERSIONS},
      {VIDEODB::TVSHOWS, NodeType::TVSHOWS_OVERVIEW},
      {VIDEODB::TVSHOW_GENRES, NodeType::GENRE},
      {VIDEODB::TVSHOW_TITLES, NodeType::TITLE_TVSHOWS},
      {VIDEODB::TVSHOW_YEARS, NodeType::YEAR},
      {VIDEODB::TVSHOW_ACTORS, NodeType::ACTOR},
      {VIDEODB::TVSHOW_STUDIOS, NodeType::STUDIO},
      {VIDEODB::TVSHOW_TAGS, NodeType::TAGS},
      {VIDEODB::MUSICVIDEOS, NodeType::MUSICVIDEOS_OVERVIEW},
      {VIDEODB::MUSICVIDEO_GENRES, NodeType::GENRE},
      {VIDEODB::MUSICVIDEO_TITLES, NodeType::TITLE_MUSICVIDEOS},
      {VIDEODB::MUSICVIDEO_YEARS, NodeType::YEAR},
      {VIDEODB::MUSICVIDEO_ARTISTS, NodeType::ACTOR},
      {VIDEODB::MUSICVIDEO_ALBUMS, NodeType::MUSICVIDEOS_ALBUM},
      {VIDEODB::MUSICVIDEO_DIRECTORS, NodeType::DIRECTOR},
      {VIDEODB::MUSICVIDEO_STUDIOS, NodeType::STUDIO},
      {VIDEODB::MUSICVIDEO_TAGS, NodeType::TAGS},
      {VIDEODB::RECENTLY_ADDED_MOVIES, NodeType::RECENTLY_ADDED_MOVIES},
      {VIDEODB::RECENTLY_ADDED_EPISODES, NodeType::RECENTLY_ADDED_EPISODES},
      {VIDEODB::RECENTLY_ADDED_MUSICVIDEOS, NodeType::RECENTLY_ADDED_MUSICVIDEOS},
      {VIDEODB::INPROGRESS_TVSHOWS, NodeType::INPROGRESS_TVSHOWS},
  };

  for (const auto& [path, node] : paths)
    EXPECT_EQ(CVideoDatabaseDirectory::GetDirectoryChildType(path), node) << path;
}
