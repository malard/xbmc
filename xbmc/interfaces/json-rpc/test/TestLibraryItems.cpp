/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "DatabaseManager.h"
#include "FileItem.h"
#include "JSONRPCTestUtils.h"
#include "ServiceBroker.h"
#include "Util.h"
#include "guilib/test/TestGUIStubs.h"
#include "interfaces/AnnouncementManager.h"
#include "music/MusicDatabase.h"
#include "utils/URIUtils.h"
#include "video/VideoDatabase.h"
#include "video/VideoInfoTag.h"

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace JSONRPC;

namespace
{
using KODI::GUILIB::TEST::CTestGUIComponent;

//! Calls methods as a client would: validated against the shipped schema, then handled
class TestLibraryItems : public ShippedServiceDescriptionTestBase
{
public:
  //! The parameter a refusal names
  std::string Refused(const char* method, const std::string& paramsJson)
  {
    CVariant result;
    EXPECT_EQ(InvalidParams, Invoke(method, paramsJson, result)) << paramsJson;
    return result["name"].asString();
  }
};

//! A library of this test's own items in the profile's databases, removed afterwards
class TestLibraryItemsInDatabase : public TestLibraryItems
{
public:
  void SetUp() override
  {
    TestLibraryItems::SetUp();
    if (!CServiceBroker::GetDatabaseManager().CanOpen("MyVideos") ||
        !CServiceBroker::GetDatabaseManager().CanOpen("MyMusic"))
    {
      ASSERT_TRUE(CServiceBroker::GetDatabaseManager().Initialize());
    }
    ASSERT_TRUE(m_videos.Open());
    ASSERT_TRUE(m_music.Open());

    m_movieId = AddMovie("/jsonrpc-test/library/Set Member (2001).mkv", SET_NAME);
    ASSERT_GT(m_movieId, 0);
    m_otherMovieId = AddMovie("/jsonrpc-test/library/Loner (2002).mkv", "");
    ASSERT_GT(m_otherMovieId, 0);
    m_setId = std::stoi(m_videos.GetSingleValue(
        m_videos.PrepareSQL("SELECT idSet FROM sets WHERE strSet = '%s'", SET_NAME)));

    m_showId = AddShow("/jsonrpc-test/library/Show/");
    ASSERT_GT(m_showId, 0);
    m_episodeId = AddEpisode(m_showId, "/jsonrpc-test/library/Show/Show.S01E01.mkv", 1);
    ASSERT_GT(m_episodeId, 0);
    m_seasonId = m_videos.GetSeasonId(m_showId, 1);
    ASSERT_GT(m_seasonId, 0);

    m_artistId = m_music.AddArtist("JSON-RPC test artist", "");
    ASSERT_GT(m_artistId, 0);
    ASSERT_TRUE(
        m_music.ExecuteQuery("INSERT INTO album (strAlbum) VALUES ('JSON-RPC test album')"));
    m_albumId = std::stoi(m_music.GetSingleValue("SELECT MAX(idAlbum) FROM album"));
    ASSERT_TRUE(m_music.ExecuteQuery(m_music.PrepareSQL(
        "INSERT INTO album_artist (idArtist, idAlbum) VALUES (%i, %i)", m_artistId, m_albumId)));
    ASSERT_TRUE(m_music.ExecuteQuery("INSERT INTO path (strPath) VALUES ('/jsonrpc-test/music/')"));
    m_pathId = std::stoi(m_music.GetSingleValue("SELECT MAX(idPath) FROM path"));
    ASSERT_TRUE(m_music.ExecuteQuery(
        m_music.PrepareSQL("INSERT INTO song (idAlbum, idPath, strFileName, iTrack, strTitle) "
                           "VALUES (%i, %i, 'song.flac', 1, 'JSON-RPC test song')",
                           m_albumId, m_pathId)));
    m_songId = std::stoi(m_music.GetSingleValue("SELECT MAX(idSong) FROM song"));
    ASSERT_TRUE(m_music.ExecuteQuery(
        m_music.PrepareSQL("INSERT INTO song_artist (idArtist, idSong, idRole, iOrder, strArtist) "
                           "VALUES (%i, %i, 1, 0, 'JSON-RPC test artist')",
                           m_artistId, m_songId)));
  }

  void TearDown() override
  {
    m_music.ExecuteQuery(m_music.PrepareSQL("DELETE FROM song_artist WHERE idSong = %i", m_songId));
    m_music.ExecuteQuery(m_music.PrepareSQL("DELETE FROM song WHERE idSong = %i", m_songId));
    m_music.ExecuteQuery(m_music.PrepareSQL("DELETE FROM path WHERE idPath = %i", m_pathId));
    m_music.ExecuteQuery(m_music.PrepareSQL("DELETE FROM album WHERE idAlbum = %i", m_albumId));
    m_music.ExecuteQuery(m_music.PrepareSQL("DELETE FROM artist WHERE idArtist = %i", m_artistId));
    m_music.Close();

    m_videos.DeleteTvShow(m_showId);
    for (const int idShow : m_extraShows)
      m_videos.DeleteTvShow(idShow);
    m_videos.DeleteMovie(m_movieId);
    m_videos.DeleteMovie(m_otherMovieId);
    m_videos.DeleteSet(m_setId);
    m_videos.Close();

    TestLibraryItems::TearDown();
  }

  static constexpr const char* SET_NAME = "JSON-RPC test set";

  CVideoInfoTag Tag(const std::string& fileAndPath)
  {
    CVideoInfoTag tag;
    tag.m_strTitle = URIUtils::GetFileName(fileAndPath);
    tag.m_strFileNameAndPath = fileAndPath;
    tag.m_strPath = URIUtils::GetDirectory(fileAndPath);
    tag.m_basePath = CFileItem(fileAndPath, false).GetBaseMoviePath(false);
    tag.m_parentPathID = m_videos.AddPath(URIUtils::GetParentPath(tag.m_basePath));
    return tag;
  }

  int AddMovie(const std::string& fileAndPath, const std::string& set)
  {
    CVideoInfoTag tag{Tag(fileAndPath)};
    if (!set.empty())
      tag.SetSet(set);
    return m_videos.SetDetailsForMovie(tag, KODI::ART::Artwork{});
  }

  int AddShow(const std::string& showPath, const std::string& title = "JSON-RPC test show")
  {
    CVideoInfoTag tag;
    tag.m_strTitle = title;
    tag.m_strPath = showPath;
    return m_videos.SetDetailsForTvShow({showPath}, tag, KODI::ART::Artwork{},
                                        KODI::ART::SeasonsArtwork{});
  }

  int AddEpisode(int idShow, const std::string& fileAndPath, int episode)
  {
    CVideoInfoTag tag{Tag(fileAndPath)};
    tag.m_iSeason = 1;
    tag.m_iEpisode = episode;
    return m_videos.SetDetailsForEpisode(tag, KODI::ART::Artwork{}, idShow);
  }

  //! A show of two episodes, one of them watched, removed with the fixture
  int AddShowInProgress(const std::string& title)
  {
    const std::string path{"/jsonrpc-test/library/" + title + "/"};
    const int idShow{AddShow(path, title)};
    m_extraShows.push_back(idShow);
    const int idEpisode{AddEpisode(idShow, path + "S01E01.mkv", 1)};
    AddEpisode(idShow, path + "S01E02.mkv", 2);

    CVideoInfoTag watched;
    m_videos.GetEpisodeInfo("", watched, idEpisode);
    m_videos.SetPlayCount(CFileItem(watched), 1);
    return idShow;
  }

  //! Calls \p listMethod and GetItems for \p kind with the same parameters, and expects one answer
  void ExpectTheQueryAnswersAsTheListMethod(const char* libraryNamespace,
                                            const char* listMethod,
                                            const char* kind,
                                            const std::string& params)
  {
    SCOPED_TRACE(listMethod);

    CVariant listed;
    ASSERT_EQ(OK, Invoke(listMethod, "{" + params + "}", listed));

    CVariant queried;
    const std::string query{std::string(libraryNamespace) + ".GetItems"};
    ASSERT_EQ(OK, Invoke(query.c_str(),
                         "{\"kind\": \"" + std::string(kind) + "\"" +
                             (params.empty() ? "" : ", " + params) + "}",
                         queried));

    EXPECT_EQ(ToJson(queried), ToJson(listed));
    EXPECT_TRUE(listed["items"].isArray());
  }

  CTestGUIComponent m_gui{CTestGUIComponent::InfoManager::WITH};
  CScopedAnnouncementManager m_announcements;
  CVideoDatabase m_videos;
  CMusicDatabase m_music;
  int m_movieId{-1};
  int m_otherMovieId{-1};
  int m_setId{-1};
  int m_showId{-1};
  std::vector<int> m_extraShows;
  int m_seasonId{-1};
  int m_episodeId{-1};
  int m_artistId{-1};
  int m_albumId{-1};
  int m_pathId{-1};
  int m_songId{-1};
};

//! Keeps every library update announced, in order
class CUpdateListener : public ANNOUNCEMENT::IAnnouncer
{
public:
  explicit CUpdateListener(std::shared_ptr<ANNOUNCEMENT::CAnnouncementManager> announcements)
    : m_announcements(std::move(announcements))
  {
    m_announcements->AddAnnouncer(this, ANNOUNCEMENT::VideoLibrary | ANNOUNCEMENT::AudioLibrary |
                                            ANNOUNCEMENT::Other);
    m_announcements->Start();
  }

  ~CUpdateListener() override { m_announcements->RemoveAnnouncer(this); }

  void Announce(ANNOUNCEMENT::AnnouncementFlag flag,
                const std::string& sender,
                const std::string& message,
                const CVariant& data) override
  {
    std::unique_lock lock(m_lock);
    if (message == DRAINED)
      m_drained = true;
    else if (message == "OnUpdate")
      m_updates.push_back(data);
    m_arrived.notify_all();
  }

  //! The updates announced about the item of \p kind with \p id, once all announced so far arrived
  std::vector<CVariant> UpdatesTo(const std::string& kind, int id)
  {
    m_announcements->Announce(ANNOUNCEMENT::Other, DRAINED);

    std::unique_lock lock(m_lock);
    m_arrived.wait_for(lock, std::chrono::seconds(5), [this] { return m_drained; });
    m_drained = false;

    std::vector<CVariant> updates;
    for (const CVariant& update : m_updates)
    {
      const CVariant& item{update.isMember("item") ? update["item"] : update};
      if (item["type"].asString() == kind && item["id"].asInteger() == id)
        updates.push_back(update);
    }
    return updates;
  }

private:
  static constexpr const char* DRAINED = "TestLibraryItemsDrained";

  std::shared_ptr<ANNOUNCEMENT::CAnnouncementManager> m_announcements;
  std::mutex m_lock;
  std::condition_variable m_arrived;
  std::vector<CVariant> m_updates;
  bool m_drained{false};
};
} // unnamed namespace

TEST_F(TestLibraryItems, TheQueryRefusesAPropertyTheKindDoesNotHave)
{
  EXPECT_EQ("properties",
            Refused("VideoLibrary.GetItems", R"({"kind": "set", "properties": ["year"]})"));
  EXPECT_EQ("properties",
            Refused("AudioLibrary.GetItems", R"({"kind": "album", "properties": ["lyrics"]})"));
}

TEST_F(TestLibraryItems, TheQueryRefusesAFilterOfAnotherKind)
{
  EXPECT_EQ("filter",
            Refused("VideoLibrary.GetItems", R"({"kind": "tvshow", "filter": {"country": "UK"}})"));
  EXPECT_EQ("filter",
            Refused("AudioLibrary.GetItems", R"({"kind": "album", "filter": {"songId": 1}})"));
}

TEST_F(TestLibraryItems, TheQueryRefusesAFilterForAKindThatTakesNone)
{
  EXPECT_EQ("filter",
            Refused("VideoLibrary.GetItems", R"({"kind": "set", "filter": {"genre": "Drama"}})"));
  EXPECT_EQ("filter", Refused("VideoLibrary.GetItems",
                              R"({"kind": "season", "filter": {"genre": "Drama"}})"));
}

TEST_F(TestLibraryItems, TheQueryRefusesWhatNarrowsAnotherKind)
{
  EXPECT_EQ("tvShowId", Refused("VideoLibrary.GetItems", R"({"kind": "movie", "tvShowId": 3})"));
  EXPECT_EQ("season",
            Refused("VideoLibrary.GetItems", R"({"kind": "season", "tvShowId": 3, "season": 1})"));
  EXPECT_EQ("albumArtistsOnly",
            Refused("AudioLibrary.GetItems", R"({"kind": "song", "albumArtistsOnly": true})"));
  EXPECT_EQ("includeSingles",
            Refused("AudioLibrary.GetItems", R"({"kind": "artist", "includeSingles": true})"));
  EXPECT_EQ("singlesOnly",
            Refused("AudioLibrary.GetItems", R"({"kind": "album", "singlesOnly": false})"));
}

TEST_F(TestLibraryItemsInDatabase, TheVideoListMethodsAreTheQueryWithPresetValues)
{
  const std::string show{std::to_string(m_showId)};
  ExpectTheQueryAnswersAsTheListMethod("VideoLibrary", "VideoLibrary.GetMovies", "movie",
                                       R"("properties": ["title", "setId", "set"])");
  ExpectTheQueryAnswersAsTheListMethod(
      "VideoLibrary", "VideoLibrary.GetMovies", "movie",
      R"("filter": {"set": "JSON-RPC test set"}, "sort": {"method": "title"})");
  ExpectTheQueryAnswersAsTheListMethod("VideoLibrary", "VideoLibrary.GetMovieSets", "set",
                                       R"("properties": ["title"], "limits": {"end": 3})");
  ExpectTheQueryAnswersAsTheListMethod("VideoLibrary", "VideoLibrary.GetTVShows", "tvshow",
                                       R"("properties": ["title", "episode"])");
  ExpectTheQueryAnswersAsTheListMethod("VideoLibrary", "VideoLibrary.GetSeasons", "season",
                                       R"("tvShowId": )" + show + R"(, "properties": ["season"])");
  ExpectTheQueryAnswersAsTheListMethod("VideoLibrary", "VideoLibrary.GetEpisodes", "episode",
                                       R"("tvShowId": )" + show +
                                           R"(, "season": 1, "properties": ["title", "episode"])");
  ExpectTheQueryAnswersAsTheListMethod("VideoLibrary", "VideoLibrary.GetMusicVideos", "musicvideo",
                                       "");
}

TEST_F(TestLibraryItemsInDatabase, TheQueryNarrowsToTheFilterGiven)
{
  CVariant result;
  ASSERT_EQ(OK, Invoke("VideoLibrary.GetItems",
                       R"({"kind": "movie", "filter": {"set": "JSON-RPC test set"}})", result));

  ASSERT_EQ(1u, result["items"].size());
  EXPECT_EQ(m_movieId, result["items"][0]["movieId"].asInteger());
  EXPECT_EQ(1, result["limits"]["total"].asInteger());
}

TEST_F(TestLibraryItemsInDatabase, TheMusicListMethodsAreTheQueryWithPresetValues)
{
  ExpectTheQueryAnswersAsTheListMethod("AudioLibrary", "AudioLibrary.GetArtists", "artist",
                                       R"("properties": ["genre"], "allRoles": true)");
  ExpectTheQueryAnswersAsTheListMethod("AudioLibrary", "AudioLibrary.GetAlbums", "album",
                                       R"("properties": ["title", "art"])");
  ExpectTheQueryAnswersAsTheListMethod("AudioLibrary", "AudioLibrary.GetSongs", "song",
                                       R"("properties": ["title", "albumId", "thumbnail"])");
}

TEST_F(TestLibraryItemsInDatabase, TheQueryAnswersAnEmptyListAsAList)
{
  CVariant result;
  ASSERT_EQ(OK, Invoke("VideoLibrary.GetItems",
                       R"({"kind": "movie", "filter": {"set": "No set is called this"}})", result));

  EXPECT_TRUE(result["items"].isArray());
  EXPECT_EQ(0u, result["items"].size());
}

TEST_F(TestLibraryItems, AnItemRefusesAPropertyItsKindDoesNotHave)
{
  EXPECT_EQ("properties",
            Refused("VideoLibrary.GetItemProperties",
                    R"({"item": {"kind": "season", "id": 1}, "properties": ["plot"]})"));
  EXPECT_EQ("properties",
            Refused("VideoLibrary.SetItemProperties",
                    R"({"item": {"kind": "set", "id": 1}, "properties": {"runtime": 60}})"));
  EXPECT_EQ("properties",
            Refused("AudioLibrary.SetItemProperties",
                    R"({"item": {"kind": "artist", "id": 1}, "properties": {"artist": ["A"]}})"));
}

TEST_F(TestLibraryItemsInDatabase, AnItemAnswersItsPropertiesAsTheListDoes)
{
  const std::string movie{std::to_string(m_movieId)};
  CVariant listed;
  ASSERT_EQ(OK, Invoke("VideoLibrary.GetItems",
                       R"({"kind": "movie", "filter": {"set": "JSON-RPC test set"},
                           "properties": ["title", "set", "setId", "file", "playCount"]})",
                       listed));

  CVariant item;
  ASSERT_EQ(OK, Invoke("VideoLibrary.GetItemProperties",
                       R"({"item": {"kind": "movie", "id": )" + movie +
                           R"(}, "properties": ["title", "set", "setId", "file", "playCount"]})",
                       item));

  ASSERT_EQ(1u, listed["items"].size());
  EXPECT_EQ(ToJson(listed["items"][0]), ToJson(item));
  EXPECT_EQ(SET_NAME, item["set"].asString());
}

TEST_F(TestLibraryItemsInDatabase, AnItemThatDoesNotExistIsNotFound)
{
  CVariant result;
  EXPECT_EQ(NotFound, Invoke("VideoLibrary.GetItemProperties",
                             R"({"item": {"kind": "movie", "id": 987654}})", result));
  EXPECT_EQ(NotFound, Invoke("VideoLibrary.SetItemProperties",
                             R"({"item": {"kind": "episode", "id": 987654},
                                 "properties": {"title": "Nobody"}})",
                             result));
  EXPECT_EQ(NotFound, Invoke("AudioLibrary.GetItemProperties",
                             R"({"item": {"kind": "song", "id": 987654}})", result));
}

TEST_F(TestLibraryItemsInDatabase, AnItemsArtChoicesAreAskedForByItsKindAndId)
{
  const std::string movie{R"({"item": {"kind": "movie", "id": )" + std::to_string(m_movieId) +
                          "}}"};
  const std::string album{R"({"item": {"kind": "album", "id": )" + std::to_string(m_albumId) +
                          "}}"};
  CVariant result;
  ASSERT_EQ(OK, Invoke("VideoLibrary.GetAvailableArtTypes", movie, result));
  EXPECT_TRUE(result["availableArtTypes"].isArray());
  ASSERT_EQ(OK, Invoke("VideoLibrary.GetAvailableArt", movie, result));
  EXPECT_TRUE(result["availableArt"].isArray());
  ASSERT_EQ(OK, Invoke("AudioLibrary.GetAvailableArtTypes", album, result));
  EXPECT_TRUE(result["availableArtTypes"].isArray());
  ASSERT_EQ(OK, Invoke("AudioLibrary.GetAvailableArt", album, result));
  EXPECT_TRUE(result["availableArt"].isArray());

  EXPECT_EQ(InvalidParams,
            Invoke("VideoLibrary.GetAvailableArtTypes",
                   R"({"item": {"movieId": )" + std::to_string(m_movieId) + "}}", result));
}

TEST_F(TestLibraryItemsInDatabase, SettingChangesOnlyWhatIsNamedAndAnswersItsValue)
{
  const std::string item{R"({"kind": "movie", "id": )" + std::to_string(m_movieId) + "}"};
  CVariant result;
  ASSERT_EQ(OK, Invoke("VideoLibrary.SetItemProperties",
                       R"({"item": )" + item + R"(, "properties": {"plot": "Before"}})", result));

  ASSERT_EQ(
      OK, Invoke("VideoLibrary.SetItemProperties",
                 R"({"item": )" + item + R"(, "properties": {"title": "Renamed", "playCount": 2}})",
                 result));

  EXPECT_EQ("Renamed", result["title"].asString());
  EXPECT_EQ(2, result["playCount"].asInteger());
  EXPECT_EQ(m_movieId, result["movieId"].asInteger());
  EXPECT_FALSE(result.isMember("plot"));

  CVariant read;
  ASSERT_EQ(OK,
            Invoke("VideoLibrary.GetItemProperties",
                   R"({"item": )" + item + R"(, "properties": ["title", "plot", "set"]})", read));
  EXPECT_EQ("Renamed", read["title"].asString());
  EXPECT_EQ("Before", read["plot"].asString());
  EXPECT_EQ(SET_NAME, read["set"].asString());
}

TEST_F(TestLibraryItemsInDatabase, ASetSeasonAndEpisodeAreItemsToo)
{
  CVariant result;
  ASSERT_EQ(OK, Invoke("VideoLibrary.SetItemProperties",
                       R"({"item": {"kind": "set", "id": )" + std::to_string(m_setId) +
                           R"(}, "properties": {"plot": "A set of one"}})",
                       result));
  EXPECT_EQ("A set of one", result["plot"].asString());

  ASSERT_EQ(OK, Invoke("VideoLibrary.SetItemProperties",
                       R"({"item": {"kind": "season", "id": )" + std::to_string(m_seasonId) +
                           R"(}, "properties": {"userRating": 7}})",
                       result));
  EXPECT_EQ(7, result["userRating"].asInteger());

  ASSERT_EQ(OK, Invoke("VideoLibrary.GetItemProperties",
                       R"({"item": {"kind": "episode", "id": )" + std::to_string(m_episodeId) +
                           R"(}, "properties": ["tvShowId", "season", "episode"]})",
                       result));
  EXPECT_EQ(m_showId, result["tvShowId"].asInteger());
  EXPECT_EQ(1, result["episode"].asInteger());
}

TEST_F(TestLibraryItemsInDatabase, AMusicItemIsSetAndReadBack)
{
  const std::string song{R"({"kind": "song", "id": )" + std::to_string(m_songId) + "}"};
  CVariant result;
  ASSERT_EQ(OK, Invoke("AudioLibrary.SetItemProperties",
                       R"({"item": )" + song +
                           R"(, "properties": {"title": "Retitled", "releaseDate": "2001-02-03",
                                                "votes": 12}})",
                       result));

  EXPECT_EQ("Retitled", result["title"].asString());
  EXPECT_EQ("2001-02-03", result["releaseDate"].asString());
  EXPECT_EQ(12, result["votes"].asInteger());
  EXPECT_EQ(m_songId, result["songId"].asInteger());

  ASSERT_EQ(OK, Invoke("AudioLibrary.SetItemProperties",
                       R"({"item": {"kind": "album", "id": )" + std::to_string(m_albumId) +
                           R"(}, "properties": {"albumStatus": "official"}})",
                       result));
  EXPECT_EQ("official", result["albumStatus"].asString());

  ASSERT_EQ(OK, Invoke("AudioLibrary.GetItemProperties",
                       R"({"item": {"kind": "artist", "id": )" + std::to_string(m_artistId) +
                           R"(}, "properties": ["sortName"]})",
                       result));
  EXPECT_EQ("JSON-RPC test artist", result["artist"].asString());
}

TEST_F(TestLibraryItemsInDatabase, SettingAnnouncesTheItemAndWhatChanged)
{
  CUpdateListener listener{CServiceBroker::GetAnnouncementManager()};

  CVariant result;
  ASSERT_EQ(OK, Invoke("VideoLibrary.SetItemProperties",
                       R"({"item": {"kind": "movie", "id": )" + std::to_string(m_movieId) +
                           R"(}, "properties": {"plot": "Announced"}})",
                       result));

  const std::vector<CVariant> updates{listener.UpdatesTo("movie", m_movieId)};
  ASSERT_EQ(1u, updates.size());
  EXPECT_EQ("Announced", updates[0]["properties"]["plot"].asString());
  EXPECT_EQ(1u, updates[0]["properties"].size());
}

TEST_F(TestLibraryItemsInDatabase, APlayCountSetIsAnnouncedOnce)
{
  CUpdateListener listener{CServiceBroker::GetAnnouncementManager()};

  CVariant result;
  ASSERT_EQ(OK, Invoke("VideoLibrary.SetItemProperties",
                       R"({"item": {"kind": "movie", "id": )" + std::to_string(m_movieId) +
                           R"(}, "properties": {"playCount": 3}})",
                       result));
  ASSERT_EQ(OK, Invoke("AudioLibrary.SetItemProperties",
                       R"({"item": {"kind": "song", "id": )" + std::to_string(m_songId) +
                           R"(}, "properties": {"playCount": 4}})",
                       result));

  const std::vector<CVariant> movie{listener.UpdatesTo("movie", m_movieId)};
  ASSERT_EQ(1u, movie.size());
  EXPECT_EQ(3, movie[0]["properties"]["playCount"].asInteger());

  const std::vector<CVariant> song{listener.UpdatesTo("song", m_songId)};
  ASSERT_EQ(1u, song.size());
  EXPECT_EQ(4, song[0]["properties"]["playCount"].asInteger());
}

TEST_F(TestLibraryItemsInDatabase, InProgressShowsAreSortedAndLimitedAsAsked)
{
  const int first{AddShowInProgress("AAA JSON-RPC in progress")};
  const int last{AddShowInProgress("zzz JSON-RPC in progress")};

  CVariant ascending;
  ASSERT_EQ(OK, Invoke("VideoLibrary.GetInProgressTVShows",
                       R"({"sort": {"method": "title", "order": "ascending"}})", ascending));
  const int total{static_cast<int>(ascending["limits"]["total"].asInteger())};
  ASSERT_GE(total, 2);
  EXPECT_EQ(first, ascending["items"][0]["tvShowId"].asInteger());

  CVariant descending;
  ASSERT_EQ(OK, Invoke("VideoLibrary.GetInProgressTVShows",
                       R"({"sort": {"method": "title", "order": "descending"},
                           "limits": {"start": 0, "end": 1}})",
                       descending));
  ASSERT_EQ(1u, descending["items"].size());
  EXPECT_EQ(last, descending["items"][0]["tvShowId"].asInteger());
  EXPECT_EQ(total, descending["limits"]["total"].asInteger());
  EXPECT_EQ(1, descending["limits"]["end"].asInteger());
}

TEST_F(TestLibraryItemsInDatabase, AddingAMovieNeedsNoScan)
{
  const std::string path{"/jsonrpc-test/library/Added Film (2003).mkv"};
  CVariant result;
  ASSERT_EQ(OK, Invoke("VideoLibrary.AddItem",
                       R"({"kind": "movie", "path": ")" + path +
                           R"(", "properties": {"plot": "Added", "year": 2003}})",
                       result));
  const int id{static_cast<int>(result["movieId"].asInteger())};
  ASSERT_GT(id, 0);
  EXPECT_EQ(CUtil::GetTitleFromPath(path, false), result["title"].asString());
  EXPECT_EQ("Added", result["plot"].asString());
  EXPECT_EQ(2003, result["year"].asInteger());

  CVariant read;
  ASSERT_EQ(OK, Invoke("VideoLibrary.GetItemProperties",
                       R"({"item": {"kind": "movie", "id": )" + std::to_string(id) +
                           R"(}, "properties": ["file", "plot"]})",
                       read));
  EXPECT_EQ(path, read["file"].asString());
  EXPECT_EQ("Added", read["plot"].asString());

  m_videos.DeleteMovie(id);
}

TEST_F(TestLibraryItemsInDatabase, AnEpisodeIsAddedToTheShowNamed)
{
  CVariant show;
  ASSERT_EQ(OK, Invoke("VideoLibrary.AddItem",
                       R"({"kind": "tvshow", "path": "/jsonrpc-test/library/Added Show",
                           "properties": {"title": "Added Show"}})",
                       show));
  const int showId{static_cast<int>(show["tvShowId"].asInteger())};
  ASSERT_GT(showId, 0);
  m_extraShows.push_back(showId);
  EXPECT_EQ("Added Show", show["title"].asString());

  CVariant episode;
  ASSERT_EQ(OK,
            Invoke("VideoLibrary.AddItem",
                   R"({"kind": "episode", "path": "/jsonrpc-test/library/Added Show/S01E02.mkv",
                           "tvShowId": )" +
                       std::to_string(showId) + R"(, "properties": {"season": 1, "episode": 2}})",
                   episode));
  ASSERT_GT(episode["episodeId"].asInteger(), 0);

  CVariant read;
  ASSERT_EQ(OK, Invoke("VideoLibrary.GetItemProperties",
                       R"({"item": {"kind": "episode", "id": )" +
                           std::to_string(episode["episodeId"].asInteger()) +
                           R"(}, "properties": ["tvShowId", "season", "episode"]})",
                       read));
  EXPECT_EQ(showId, read["tvShowId"].asInteger());
  EXPECT_EQ(1, read["season"].asInteger());
  EXPECT_EQ(2, read["episode"].asInteger());
}

TEST_F(TestLibraryItemsInDatabase, AMusicVideoIsAddedAndAnnouncedAsAdded)
{
  CUpdateListener listener{CServiceBroker::GetAnnouncementManager()};

  CVariant result;
  ASSERT_EQ(OK,
            Invoke("VideoLibrary.AddItem",
                   R"({"kind": "musicvideo", "path": "/jsonrpc-test/library/Clip.mkv"})", result));
  const int id{static_cast<int>(result["musicVideoId"].asInteger())};
  ASSERT_GT(id, 0);

  const std::vector<CVariant> updates{listener.UpdatesTo("musicvideo", id)};
  ASSERT_EQ(1u, updates.size());
  EXPECT_TRUE(updates[0]["added"].asBoolean());

  m_videos.DeleteMusicVideo(id);
}

TEST_F(TestLibraryItemsInDatabase, AnItemAlreadyAtThePathIsNotAddedAgain)
{
  CVariant result;
  EXPECT_EQ(FailedToExecute,
            Invoke("VideoLibrary.AddItem",
                   R"({"kind": "movie", "path": "/jsonrpc-test/library/Loner (2002).mkv"})",
                   result));
  EXPECT_EQ("already-in-library", result["reason"].asString());
}

TEST_F(TestLibraryItemsInDatabase, AnEpisodeNeedsAShowThatExists)
{
  CVariant result;
  EXPECT_EQ(NotFound, Invoke("VideoLibrary.AddItem",
                             R"({"kind": "episode", "path": "/jsonrpc-test/library/Orphan.mkv",
                                 "tvShowId": 987654})",
                             result));
  EXPECT_EQ(InvalidParams,
            Invoke("VideoLibrary.AddItem",
                   R"({"kind": "episode", "path": "/jsonrpc-test/library/Orphan.mkv"})", result));
  EXPECT_EQ(InvalidParams,
            Invoke("VideoLibrary.AddItem",
                   R"({"kind": "movie", "path": "/jsonrpc-test/library/Film.mkv", "tvShowId": )" +
                       std::to_string(m_showId) + "}",
                   result));
}
