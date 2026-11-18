/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "FileItem.h"
#include "FileItemList.h"
#include "ServiceBroker.h"
#include "URL.h"
#include "cores/playercorefactory/PlayerCoreFactory.h"
#include "filesystem/Directory.h"
#include "filesystem/DirectoryCache.h"
#include "filesystem/File.h"
#include "filesystem/SpecialProtocol.h"
#include "interfaces/json-rpc/PlayerOperations.h"
#include "playlists/PlayListFileItemClassify.h"
#include "settings/SettingsComponent.h"
#include "utils/URIUtils.h"
#include "utils/Variant.h"

#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace JSONRPC;

namespace
{

class CTestPlayerOperations : public CPlayerOperations
{
public:
  static bool List(const std::string& path,
                   bool recursive,
                   CFileItemList& pictures,
                   CFileItemList& media)
  {
    return ListSlideshowDirectory(path, recursive, pictures, media);
  }
  using CPlayerOperations::PlayerNameFor;
};

class TestPlayerOpenDirectory : public ::testing::Test
{
protected:
  void SetUp() override
  {
    m_root = URIUtils::AddFileToFolder(CSpecialProtocol::TranslatePath("special://temp/"),
                                       "jsonrpc-playeropen-directory/");
    XFILE::CDirectory::RemoveRecursive(m_root);
    ASSERT_TRUE(XFILE::CDirectory::Create(m_root));
  }

  void TearDown() override
  {
    g_directoryCache.ClearSubPaths(CURL(m_root));
    XFILE::CDirectory::RemoveRecursive(m_root);
  }

  void Touch(const std::string& relative)
  {
    const std::string path{URIUtils::AddFileToFolder(m_root, relative)};
    XFILE::CDirectory::Create(URIUtils::GetDirectory(path));
    XFILE::CFile file;
    ASSERT_TRUE(file.OpenForWrite(path, true)) << path;
    file.Close();
  }

  static std::vector<std::string> Names(const CFileItemList& list)
  {
    std::vector<std::string> names;
    for (int i = 0; i < list.Size(); ++i)
      names.push_back(URIUtils::GetFileName(list[i]->GetPath()));
    return names;
  }

  std::string m_root;
};

class TestPlayerOpenPlayerName : public ::testing::Test
{
protected:
  void SetUp() override
  {
    m_factory = std::make_unique<CPlayerCoreFactory>(
        *CServiceBroker::GetSettingsComponent()->GetProfileManager());
    m_factory->OnSettingsLoaded();
    // The unit environment has no game service, so the media tags say what each file is.
    m_audio.GetMusicInfoTag();
    m_video.GetVideoInfoTag();
  }

  JSONRPC_STATUS NameFor(const CVariant& option, const CFileItem* first)
  {
    return CTestPlayerOperations::PlayerNameFor(option, first, *m_factory, m_name);
  }

  std::unique_ptr<CPlayerCoreFactory> m_factory;
  CFileItem m_audio{"/music/track.wav", false};
  CFileItem m_video{"/video/film.mp4", false};
  std::string m_name{"previous"};
};

} // unnamed namespace

TEST_F(TestPlayerOpenDirectory, ADirectoryOfMediaHoldsNoPictures)
{
  Touch("b.mkv");
  Touch("a.mp4");
  Touch("c.mp3");
  Touch("notes.txt");

  CFileItemList pictures;
  CFileItemList media;
  ASSERT_TRUE(CTestPlayerOperations::List(m_root, true, pictures, media));

  EXPECT_EQ(0, pictures.Size());
  EXPECT_EQ((std::vector<std::string>{"a.mp4", "b.mkv", "c.mp3"}), Names(media));
}

TEST_F(TestPlayerOpenDirectory, OnePictureMakesItASlideshowSource)
{
  Touch("a.mp4");
  Touch("x.jpg");

  CFileItemList pictures;
  CFileItemList media;
  ASSERT_TRUE(CTestPlayerOperations::List(m_root, true, pictures, media));

  EXPECT_EQ((std::vector<std::string>{"x.jpg"}), Names(pictures));
  EXPECT_EQ((std::vector<std::string>{"a.mp4"}), Names(media));
}

TEST_F(TestPlayerOpenDirectory, AnArchiveIsNeitherAPictureNorMedia)
{
  Touch("movie.mkv");
  Touch("backup.zip");
  Touch("scans.rar");

  CFileItemList pictures;
  CFileItemList media;
  ASSERT_TRUE(CTestPlayerOperations::List(m_root, true, pictures, media));

  EXPECT_EQ(0, pictures.Size());
  EXPECT_EQ((std::vector<std::string>{"movie.mkv"}), Names(media));
}

TEST_F(TestPlayerOpenDirectory, AFileThatCouldBeAudioOrVideoAsksForTheVideoPlayer)
{
  Touch("recording.wtv");
  Touch("film.mkv");

  CFileItemList pictures;
  CFileItemList media;
  ASSERT_TRUE(CTestPlayerOperations::List(m_root, true, pictures, media));

  ASSERT_EQ((std::vector<std::string>{"film.mkv", "recording.wtv"}), Names(media));
  EXPECT_EQ(KODI::PLAYLIST::Video, KODI::PLAYLIST::TypeFor(*media[1]));
}

TEST_F(TestPlayerOpenDirectory, SubdirectoriesAreOnlyFollowedWhenAsked)
{
  Touch("a.mp4");
  Touch("sub/d.mp4");
  Touch("sub/y.png");

  CFileItemList pictures;
  CFileItemList media;
  ASSERT_TRUE(CTestPlayerOperations::List(m_root, false, pictures, media));
  EXPECT_EQ(0, pictures.Size());
  EXPECT_EQ((std::vector<std::string>{"a.mp4"}), Names(media));

  pictures.Clear();
  media.Clear();
  ASSERT_TRUE(CTestPlayerOperations::List(m_root, true, pictures, media));
  EXPECT_EQ((std::vector<std::string>{"y.png"}), Names(pictures));
  EXPECT_EQ((std::vector<std::string>{"a.mp4", "d.mp4"}), Names(media));
}

// An NFS symlink lists with its target's own path, so a link to an enclosing directory names
// that directory again rather than a deeper path. The listings are seeded into the directory
// cache, which CDirectory::GetDirectory consults before the filesystem.
TEST_F(TestPlayerOpenDirectory, ALinkToAnEnclosingDirectoryIsListedOnce)
{
  const std::string top{URIUtils::AddFileToFolder(m_root, "share/")};
  const std::string sub{URIUtils::AddFileToFolder(top, "sub/")};

  CFileItemList topItems;
  topItems.Add(std::make_shared<CFileItem>(URIUtils::AddFileToFolder(top, "a.mp4"), false));
  topItems.Add(std::make_shared<CFileItem>(sub, true));
  g_directoryCache.SetDirectory(CURL(top), topItems, XFILE::CacheType::ALWAYS);

  CFileItemList subItems;
  subItems.Add(std::make_shared<CFileItem>(URIUtils::AddFileToFolder(sub, "b.mp4"), false));
  subItems.Add(std::make_shared<CFileItem>(top, true));
  g_directoryCache.SetDirectory(CURL(sub), subItems, XFILE::CacheType::ALWAYS);

  CFileItemList pictures;
  CFileItemList media;
  ASSERT_TRUE(CTestPlayerOperations::List(top, true, pictures, media));

  EXPECT_EQ(0, pictures.Size());
  EXPECT_EQ((std::vector<std::string>{"a.mp4", "b.mp4"}), Names(media));
}

TEST_F(TestPlayerOpenDirectory, ADirectoryThatCannotBeListedIsReported)
{
  CFileItemList pictures;
  CFileItemList media;
  EXPECT_FALSE(CTestPlayerOperations::List(URIUtils::AddFileToFolder(m_root, "missing/"), true,
                                           pictures, media));
}

TEST_F(TestPlayerOpenPlayerName, NoNameOrTheNameDefaultIsTheDefaultPlayer)
{
  EXPECT_EQ(OK, NameFor(CVariant(), &m_audio));
  EXPECT_EQ("", m_name);
  EXPECT_EQ(OK, NameFor(CVariant("default"), &m_audio));
  EXPECT_EQ("", m_name);
}

TEST_F(TestPlayerOpenPlayerName, AnUnknownPlayerIsRefused)
{
  EXPECT_EQ(InvalidParams, NameFor(CVariant("NoSuchPlayer"), &m_audio));
  EXPECT_EQ(InvalidParams, NameFor(CVariant(42), &m_audio));
  EXPECT_EQ(InvalidParams, NameFor(CVariant("NoSuchPlayer"), nullptr));
}

TEST_F(TestPlayerOpenPlayerName, APlayerMustPlayTheFirstItem)
{
  EXPECT_EQ(InvalidParams, NameFor(CVariant("PAPlayer"), &m_video));
  EXPECT_EQ(OK, NameFor(CVariant("VideoPlayer"), &m_video));
  EXPECT_EQ("VideoPlayer", m_name);
}

TEST_F(TestPlayerOpenPlayerName, WithNoFirstItemOnlyThePlayerIsChecked)
{
  EXPECT_EQ(OK, NameFor(CVariant("PAPlayer"), nullptr));
  EXPECT_EQ("PAPlayer", m_name);
}
