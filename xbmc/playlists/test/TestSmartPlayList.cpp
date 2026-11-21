/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "filesystem/Directory.h"
#include "filesystem/File.h"
#include "playlists/SmartPlayList.h"
#include "settings/Settings.h"
#include "test/TestUtils.h"
#include "utils/URIUtils.h"

#include <cstdlib>
#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace KODI;

TEST(TestSmartPlayList, IgnoreFoldersSurvivesAJsonRoundTrip)
{
  PLAYLIST::CSmartPlaylist playlist;
  playlist.SetType("songs");
  playlist.SetOrder(SortBy::TITLE);
  playlist.SetOrderAttributes(SortAttributeIgnoreFolders);
  std::string json;
  ASSERT_TRUE(playlist.SaveAsJson(json));

  PLAYLIST::CSmartPlaylist loaded;
  ASSERT_TRUE(loaded.LoadFromJson(json));
  EXPECT_EQ(SortAttributeIgnoreFolders, loaded.GetOrderAttributes()) << json;
}

namespace
{
constexpr auto PLAYLISTS_FOLDER = "special://temp/test-smartplaylist-references/";

class TestSmartPlayListReferences : public ::testing::Test
{
protected:
  void SetUp() override
  {
    for (const char* folder : {"music", "video", "mixed"})
    {
      ASSERT_TRUE(XFILE::CDirectory::Create(URIUtils::AddFileToFolder(PLAYLISTS_FOLDER, folder)));
    }
  }
  void TearDown() override { XFILE::CDirectory::RemoveRecursive(PLAYLISTS_FOLDER); }

  static std::string PathOf(const std::string& folder, const std::string& name)
  {
    return URIUtils::AddFileToFolder(PLAYLISTS_FOLDER, folder, name + ".xsp");
  }

  //! Writes a playlist of this type into this folder of the playlists path
  static void WritePlaylist(const std::string& name,
                            const std::string& rules,
                            const std::string& type = "songs",
                            const std::string& folder = "music")
  {
    const std::string xml{"<smartplaylist type=\"" + type + "\"><name>" + name +
                          "</name><match>all</match>" + rules + "</smartplaylist>"};
    XFILE::CFile file;
    ASSERT_TRUE(file.OpenForWrite(PathOf(folder, name), true));
    ASSERT_EQ(static_cast<ssize_t>(xml.size()), file.Write(xml.data(), xml.size()));
  }

  static std::vector<std::string> VirtualFolderNamesOf(const std::string& name,
                                                       const std::string& folder = "music")
  {
    PLAYLIST::CSmartPlaylist playlist;
    EXPECT_TRUE(playlist.Load(PathOf(folder, name)));
    std::vector<std::string> folders;
    playlist.GetVirtualFolders(folders);
    for (auto& folder : folders)
    {
      folder = URIUtils::GetFileName(folder);
    }
    return folders;
  }

  //! A cycle that is not stopped recurses until the stack overflows, which ends the whole test
  //! run, so the walk is first tried in a process of its own.
  static void ExpectWalkEnds(const std::string& name)
  {
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    EXPECT_EXIT(
        {
          VirtualFolderNamesOf(name);
          std::_Exit(0);
        },
        ::testing::ExitedWithCode(0), "");
  }

  static std::string Rule(const std::string& field, const std::string& value)
  {
    return "<rule field=\"" + field + "\" operator=\"is\"><value>" + value + "</value></rule>";
  }

  const CScopedSetting m_playlistsPath{CSettings::SETTING_SYSTEM_PLAYLISTSPATH, PLAYLISTS_FOLDER};
};
} // namespace

TEST_F(TestSmartPlayListReferences, APlaylistReferencingItselfHasNoVirtualFolders)
{
  WritePlaylist("A", Rule("playlist", "A"));

  ExpectWalkEnds("A");
  if (HasFailure())
  {
    return;
  }
  EXPECT_TRUE(VirtualFolderNamesOf("A").empty());
}

TEST_F(TestSmartPlayListReferences, PlaylistsReferencingEachOtherGiveTheirVirtualFoldersOnce)
{
  WritePlaylist("A", Rule("playlist", "B"));
  WritePlaylist("B", Rule("playlist", "A") + Rule("virtualfolder", "C"));
  WritePlaylist("C", Rule("genre", "Rock"));

  ExpectWalkEnds("A");
  if (HasFailure())
  {
    return;
  }
  EXPECT_EQ(std::vector<std::string>{"C.xsp"}, VirtualFolderNamesOf("A"));
}

TEST_F(TestSmartPlayListReferences, AMixedPlaylistFindsAMusicVideoPlaylistAsAVirtualFolder)
{
  WritePlaylist("V", Rule("genre", "Rock"), "musicvideos", "video");
  WritePlaylist("M", Rule("virtualfolder", "V"), "mixed", "mixed");

  EXPECT_EQ(std::vector<std::string>{"V.xsp"}, VirtualFolderNamesOf("M", "mixed"));
}
