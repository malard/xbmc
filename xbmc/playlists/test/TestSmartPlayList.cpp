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
#include <utility>
#include <vector>

#include <gtest/gtest.h>

using namespace KODI;

using Param = std::pair<std::string, PLAYLIST::Type>;

class TestSmartPlayListType : public ::testing::TestWithParam<Param>
{
};

TEST_P(TestSmartPlayListType, PlaysOnThePlayListOfItsType)
{
  PLAYLIST::CSmartPlaylist smartPlayList;
  smartPlayList.SetType(GetParam().first);
  EXPECT_EQ(GetParam().second, smartPlayList.GetPlayListType());
}

INSTANTIATE_TEST_SUITE_P(Types,
                         TestSmartPlayListType,
                         ::testing::Values(Param{"songs", PLAYLIST::Audio},
                                           Param{"albums", PLAYLIST::Audio},
                                           Param{"artists", PLAYLIST::Audio},
                                           Param{"movies", PLAYLIST::Video},
                                           Param{"tvshows", PLAYLIST::Video},
                                           Param{"episodes", PLAYLIST::Video},
                                           Param{"musicvideos", PLAYLIST::Video},
                                           Param{"mixed", PLAYLIST::Video},
                                           Param{"", PLAYLIST::Video}));

namespace
{
constexpr auto PLAYLISTS_FOLDER = "special://temp/test-smartplaylist-references/";

class TestSmartPlayListReferences : public ::testing::Test
{
protected:
  void SetUp() override
  {
    ASSERT_TRUE(XFILE::CDirectory::Create(m_musicFolder));
    ASSERT_TRUE(XFILE::CDirectory::Create(URIUtils::AddFileToFolder(PLAYLISTS_FOLDER, "mixed")));
  }
  void TearDown() override { XFILE::CDirectory::RemoveRecursive(PLAYLISTS_FOLDER); }

  void WritePlaylist(const std::string& name, const std::string& rules) const
  {
    const std::string xml{"<smartplaylist type=\"songs\"><name>" + name +
                          "</name><match>all</match>" + rules + "</smartplaylist>"};
    XFILE::CFile file;
    ASSERT_TRUE(file.OpenForWrite(URIUtils::AddFileToFolder(m_musicFolder, name + ".xsp"), true));
    ASSERT_EQ(static_cast<ssize_t>(xml.size()), file.Write(xml.data(), xml.size()));
  }

  std::vector<std::string> VirtualFolderNamesOf(const std::string& name) const
  {
    PLAYLIST::CSmartPlaylist playlist;
    EXPECT_TRUE(playlist.Load(URIUtils::AddFileToFolder(m_musicFolder, name + ".xsp")));
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
  void ExpectWalkEnds(const std::string& name) const
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

  const std::string m_musicFolder{URIUtils::AddFileToFolder(PLAYLISTS_FOLDER, "music")};
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
