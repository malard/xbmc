/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "filesystem/MusicDatabaseDirectory.h"
#include "filesystem/MusicDatabaseDirectory/DirectoryNode.h"
#include "music/MusicDbPaths.h"

#include <utility>

#include <gtest/gtest.h>

using namespace KODI;
using XFILE::CMusicDatabaseDirectory;
using XFILE::MUSICDATABASEDIRECTORY::NodeType;

TEST(TestMusicDbPaths, EveryPathListsTheNodeItNames)
{
  const std::pair<const char*, NodeType> paths[] = {
      {MUSICDB::ROOT, NodeType::OVERVIEW},
      {MUSICDB::GENRES, NodeType::GENRE},
      {MUSICDB::ARTISTS, NodeType::ARTIST},
      {MUSICDB::ALBUMS, NodeType::ALBUM},
      {MUSICDB::BOX_SETS, NodeType::ALBUM},
      {MUSICDB::SINGLES, NodeType::SINGLES},
      {MUSICDB::SONGS, NodeType::SONG},
      {MUSICDB::YEARS, NodeType::YEAR},
      {MUSICDB::ORIGINAL_YEARS, NodeType::YEAR},
      {MUSICDB::TOP100, NodeType::TOP100},
      {MUSICDB::TOP100_ALBUMS, NodeType::ALBUM_TOP100},
      {MUSICDB::TOP100_SONGS, NodeType::SONG_TOP100},
      {MUSICDB::RECENTLY_ADDED_ALBUMS, NodeType::ALBUM_RECENTLY_ADDED},
      {MUSICDB::RECENTLY_PLAYED_ALBUMS, NodeType::ALBUM_RECENTLY_PLAYED},
      {MUSICDB::COMPILATIONS, NodeType::ALBUM},
      {MUSICDB::ROLES, NodeType::ROLE},
      {MUSICDB::SOURCES, NodeType::SOURCE},
      {MUSICDB::DISCS, NodeType::DISC},
  };

  for (const auto& [path, node] : paths)
    EXPECT_EQ(CMusicDatabaseDirectory::GetDirectoryChildType(path), node) << path;
}
