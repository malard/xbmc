/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "FileItem.h"
#include "FileItemList.h"
#include "JSONRPCTestUtils.h"
#include "guilib/test/TestGUIStubs.h"
#include "interfaces/json-rpc/AudioLibrary.h"
#include "music/MusicDatabase.h"
#include "music/tags/MusicInfoTag.h"
#include "test/TestAnnouncements.h"
#include "test/TestLibrary.h"
#include "utils/Variant.h"

#include <array>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace
{
using KODI::GUILIB::TEST::CTestGUIComponent;

class TestAudioLibrary : public JSONRPC::ShippedServiceDescriptionTestBase,
                         public testing::WithParamInterface<bool>
{
protected:
  void SetUp() override
  {
    ShippedServiceDescriptionTestBase::SetUp();
    m_artistId = m_db.AddArtist("SetArtistDetails test", "");
    ASSERT_GT(m_artistId, 0);
    ASSERT_TRUE(m_db.ExecuteQuery("INSERT INTO album (strAlbum) VALUES ('Test album')"));
    m_albumId = std::stoi(m_db.GetSingleValue("SELECT MAX(idAlbum) FROM album"));
    ASSERT_TRUE(m_db.ExecuteQuery(m_db.PrepareSQL(
        "INSERT INTO album_artist (idArtist, idAlbum) VALUES (%i, %i)", m_artistId, m_albumId)));
  }

  CTestGUIComponent m_gui{CTestGUIComponent::InfoManager::WITH};
  CScopedAnnouncementManager m_announcements;
  CTestLibrary m_library{"TestAudioLibrary"};
  CMusicDatabase& m_db{m_library.Music()};
  int m_artistId{-1};
  int m_albumId{-1};
};
} // namespace

TEST_P(TestAudioLibrary, SetArtistDetailsPreservesDiscographyAndVideoLinks)
{
  struct Song
  {
    const char* title;
    const char* mbid;
    const char* video;
  };
  const std::array songs{
      Song{"One", "", "https://example.com/shared"},
      Song{"Someone", "", ""},
      Song{"Another song", "", "https://example.com/shared"},
      Song{"One", "", "https://example.com/shared"},
      Song{"One", "", ""},
      Song{"MBID song", "recording-id", "https://example.com/mbid"},
      Song{"MBID song", "recording-id", ""},
  };
  for (size_t i = 0; i < songs.size(); ++i)
  {
    const auto& song = songs[i];
    ASSERT_TRUE(m_db.ExecuteQuery(m_db.PrepareSQL(
        "INSERT INTO song (idAlbum, iTrack, strTitle, strMusicBrainzTrackID, strVideoURL) "
        "VALUES (%i, %i, '%s', NULLIF('%s', ''), NULLIF('%s', ''))",
        m_albumId, static_cast<int>(i), song.title, song.mbid, song.video)));
    if (*song.video)
    {
      ASSERT_TRUE(m_db.ExecuteQuery(
          m_db.PrepareSQL("INSERT INTO art (media_id, media_type, type, url) "
                          "SELECT idSong, 'song', 'videothumb', 'thumb%i' FROM song "
                          "WHERE idAlbum = %i AND iTrack = %i",
                          static_cast<int>(i), m_albumId, static_cast<int>(i))));
    }
  }
  if (GetParam())
  {
    ASSERT_TRUE(m_db.ExecuteQuery(m_db.PrepareSQL(
        "INSERT INTO discography VALUES (%i, 'Scraped album', '2026', 'release-id')", m_artistId)));
  }
  m_db.SetArtForItem(m_artistId, "artist", "fanart", "existing-fanart");
  m_db.SetArtForItem(m_artistId, "artist", "thumb", "old-thumb");
  m_db.SetArtForItem(m_artistId, "artist", "banner", "old-banner");
  ASSERT_TRUE(m_db.ExecuteQuery("UPDATE versiontagscan SET lastscanned = '2000-01-01 00:00:00'"));

  CVariant params(CVariant::VariantTypeObject);
  params["item"]["kind"] = "artist";
  params["item"]["id"] = m_artistId;
  params["properties"]["description"] = "Updated biography";
  params["properties"]["art"]["thumb"] = "new-thumb";
  params["properties"]["art"]["banner"] = CVariant(CVariant::VariantTypeNull);
  CVariant result;
  ASSERT_EQ(JSONRPC::OK, JSONRPC::CAudioLibrary::SetItemProperties(params, result));

  EXPECT_EQ("Updated biography",
            m_db.GetSingleValue(m_db.PrepareSQL(
                "SELECT strBiography FROM artist WHERE idArtist = %i", m_artistId)));
  EXPECT_EQ(GetParam() ? "1" : "0",
            m_db.GetSingleValue(m_db.PrepareSQL(
                "SELECT COUNT(*) FROM discography WHERE idArtist = %i", m_artistId)));
  if (GetParam())
  {
    const std::string where = m_db.PrepareSQL("idArtist = %i", m_artistId);
    EXPECT_EQ("Scraped album", m_db.GetSingleValue("discography", "strAlbum", where));
    EXPECT_EQ("2026", m_db.GetSingleValue("discography", "strYear", where));
    EXPECT_EQ("release-id", m_db.GetSingleValue("discography", "strReleaseGroupMBID", where));
  }
  for (size_t i = 0; i < songs.size(); ++i)
  {
    SCOPED_TRACE(i);
    EXPECT_EQ(songs[i].video, m_db.GetSingleValue(m_db.PrepareSQL(
                                  "SELECT strVideoURL FROM song WHERE idAlbum = %i AND iTrack = %i",
                                  m_albumId, static_cast<int>(i))));
    EXPECT_EQ(
        *songs[i].video ? "thumb" + std::to_string(i) : "",
        m_db.GetSingleValue(m_db.PrepareSQL(
            "SELECT url FROM art JOIN song ON media_id = idSong "
            "WHERE media_type = 'song' AND type = 'videothumb' AND idAlbum = %i AND iTrack = %i",
            m_albumId, static_cast<int>(i))));
  }
  EXPECT_EQ("existing-fanart", m_db.GetArtForItem(m_artistId, "artist", "fanart"));
  EXPECT_EQ("new-thumb", m_db.GetArtForItem(m_artistId, "artist", "thumb"));
  EXPECT_TRUE(m_db.GetArtForItem(m_artistId, "artist", "banner").empty());
  EXPECT_NE("2000-01-01 00:00:00", m_db.GetLibraryLastUpdated());
}

INSTANTIATE_TEST_SUITE_P(WithAndWithoutDiscography, TestAudioLibrary, testing::Bool());

namespace
{
class TestAudioLibraryFillFileItemList : public testing::Test
{
protected:
  int AddAlbumWithTwoTracks(const std::string& name)
  {
    int idSong{-1};
    for (const int track : {1, 2})
    {
      idSong = m_library.AddSong(name + " " + std::to_string(track), name,
                                 "FillFileItemList artist", track);
      EXPECT_GT(idSong, 0);
    }
    return std::stoi(m_library.Music().GetSingleValue(
        m_library.Music().PrepareSQL("SELECT idAlbum FROM song WHERE idSong = %i", idSong)));
  }

  CTestGUIComponent m_gui{CTestGUIComponent::InfoManager::WITH};
  CTestLibrary m_library{"TestAudioLibraryFillFileItemList"};
};
} // namespace

// Playlist.Add and Insert resolve every entry of an item array into one list
TEST_F(TestAudioLibraryFillFileItemList, AlbumsAddedTogetherKeepTheirTracksTogether)
{
  const int first = AddAlbumWithTwoTracks("First");
  const int second = AddAlbumWithTwoTracks("Second");

  CFileItemList list;
  for (const int albumId : {first, second})
  {
    CVariant parameters(CVariant::VariantTypeObject);
    parameters["albumId"] = albumId;
    ASSERT_TRUE(JSONRPC::CAudioLibrary::FillFileItemList(parameters, list));
  }

  std::vector<std::string> titles;
  for (const auto& item : list)
    titles.emplace_back(item->GetMusicInfoTag()->GetTitle());

  EXPECT_EQ((std::vector<std::string>{"First 1", "First 2", "Second 1", "Second 2"}), titles);
}
