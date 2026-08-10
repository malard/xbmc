/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "interfaces/json-rpc/VideoLibrary.h"
#include "video/VideoInfoTag.h"

#include <string>

#include <gtest/gtest.h>

using namespace JSONRPC;

namespace
{
/*! \brief A tag as CVideoDatabase::GetFileInfo fills it: bookkeeping, and nothing that says
    what the item is. */
CVideoInfoTag FileRow()
{
  CVideoInfoTag fileDetails;
  fileDetails.m_iFileId = 42;
  fileDetails.m_strPath = "plugin://plugin.video.example/";
  fileDetails.m_strFileNameAndPath = "plugin://plugin.video.example/?play=1";
  fileDetails.SetPlayCount(3);
  fileDetails.m_lastPlayed.SetFromDBDateTime("2026-08-10 21:49:28");
  fileDetails.m_dateAdded.SetFromDBDateTime("2026-08-01 10:00:00");
  fileDetails.SetResumePoint(120.0, 1800.0, "");
  return fileDetails;
}

/*! \brief A tag as an add-on's ListItem supplies it: a description, with no playback history. */
CVideoInfoTag PluginDescription()
{
  CVideoInfoTag details;
  details.m_type = MediaTypeEpisode;
  details.m_strTitle = "Extraterrestrial Girl";
  details.m_strShowTitle = "Planetes";
  details.m_iSeason = 1;
  details.m_iEpisode = 7;
  details.m_strFileNameAndPath = "http://stream.example/real-stream.mkv";
  return details;
}
} // unnamed namespace

TEST(TestVideoLibraryPlaybackState, KeepsTheDescriptionItIsAppliedTo)
{
  CVideoInfoTag details = PluginDescription();
  CVideoLibrary::ApplyPlaybackState(FileRow(), details);

  EXPECT_EQ(MediaTypeEpisode, details.m_type);
  EXPECT_EQ("Extraterrestrial Girl", details.m_strTitle);
  EXPECT_EQ("Planetes", details.m_strShowTitle);
  EXPECT_EQ(1, details.m_iSeason);
  EXPECT_EQ(7, details.m_iEpisode);
}

TEST(TestVideoLibraryPlaybackState, AddsTheStateTheFileRowCarries)
{
  CVideoInfoTag details = PluginDescription();
  CVideoLibrary::ApplyPlaybackState(FileRow(), details);

  EXPECT_EQ(42, details.m_iFileId);
  EXPECT_EQ(3, details.GetPlayCount());
  EXPECT_EQ("2026-08-10 21:49:28", details.m_lastPlayed.GetAsDBDateTime());
  EXPECT_EQ("2026-08-01 10:00:00", details.m_dateAdded.GetAsDBDateTime());
  EXPECT_TRUE(details.GetResumePoint().IsSet());
  EXPECT_EQ(120.0, details.GetResumePoint().timeInSeconds);
}

TEST(TestVideoLibraryPlaybackState, LeavesThePathsAlone)
{
  // The stored path is the plugin url the row is keyed on. Letting it through is what made
  // Player.GetItem report a different file on the second play than on the first.
  CVideoInfoTag details = PluginDescription();
  CVideoLibrary::ApplyPlaybackState(FileRow(), details);

  EXPECT_EQ("http://stream.example/real-stream.mkv", details.m_strFileNameAndPath);
  EXPECT_TRUE(details.m_strPath.empty());
}

TEST(TestVideoLibraryPlaybackState, DoesNotDowngradeStateTheItemAlreadyHas)
{
  CVideoInfoTag details = PluginDescription();
  details.SetPlayCount(9);
  details.m_lastPlayed.SetFromDBDateTime("2026-08-10 23:00:00");
  details.SetResumePoint(600.0, 1800.0, "");

  CVideoLibrary::ApplyPlaybackState(FileRow(), details);

  EXPECT_EQ(9, details.GetPlayCount());
  EXPECT_EQ("2026-08-10 23:00:00", details.m_lastPlayed.GetAsDBDateTime());
  EXPECT_EQ(600.0, details.GetResumePoint().timeInSeconds);
}

TEST(TestVideoLibraryPlaybackState, FillsAnItemThatSaysNothingAboutItself)
{
  // Files.GetFileDetails on a file no add-on described: the row is all there is, and it must
  // still come through.
  CVideoInfoTag details;
  CVideoLibrary::ApplyPlaybackState(FileRow(), details);

  EXPECT_EQ(42, details.m_iFileId);
  EXPECT_EQ(3, details.GetPlayCount());
  EXPECT_TRUE(details.GetResumePoint().IsSet());
}
