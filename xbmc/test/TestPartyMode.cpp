/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "FileItem.h"
#include "FileItemList.h"
#include "PartyMode.h"
#include "music/tags/MusicInfoTag.h"
#include "video/VideoInfoTag.h"

#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace KODI;

namespace
{
std::shared_ptr<CFileItem> Song(int id)
{
  auto item = std::make_shared<CFileItem>("/music/" + std::to_string(id) + ".flac", false);
  item->GetMusicInfoTag()->SetDatabaseId(id, KODI::MEDIA::NameOf(KODI::MEDIA::MediaType::SONG));
  return item;
}

std::shared_ptr<CFileItem> MusicVideo(int id)
{
  auto item = std::make_shared<CFileItem>("/video/" + std::to_string(id) + ".mkv", false);
  item->GetVideoInfoTag()->m_iDbId = id;
  return item;
}

std::vector<std::string> Paths(const std::vector<std::shared_ptr<CFileItem>>& items)
{
  std::vector<std::string> paths;
  for (const auto& item : items)
    paths.push_back(item->GetPath());
  return paths;
}
} // namespace

TEST(TestPartyMode, ABatchIsQueuedInTheMatchesOrderNotTheDatabases)
{
  constexpr auto SONG = PARTYMODE::Library::Song;
  constexpr auto VIDEO = PARTYMODE::Library::MusicVideo;
  CFileItemList fetched;
  fetched.Add(Song(1));
  fetched.Add(Song(7));
  fetched.Add(MusicVideo(7));

  EXPECT_EQ((std::vector<std::string>{"/video/7.mkv", "/music/1.flac", "/music/7.flac"}),
            Paths(PARTYMODE::InMatchOrder({{VIDEO, 7}, {SONG, 1}, {SONG, 7}}, fetched)))
      << "a song and a music video may share an id";
}

TEST(TestPartyMode, AMatchTheDatabaseNoLongerHoldsIsSkipped)
{
  constexpr auto SONG = PARTYMODE::Library::Song;
  CFileItemList fetched;
  fetched.Add(Song(3));
  fetched.Add(Song(1));

  EXPECT_EQ((std::vector<std::string>{"/music/1.flac", "/music/3.flac"}),
            Paths(PARTYMODE::InMatchOrder({{SONG, 1}, {SONG, 2}, {SONG, 3}}, fetched)));
}
