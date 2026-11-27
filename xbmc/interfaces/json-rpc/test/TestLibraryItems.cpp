/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "FileItem.h"
#include "JSONRPCTestUtils.h"
#include "Util.h"
#include "XBDateTime.h"
#include "guilib/test/TestGUIStubs.h"
#include "media/MediaType.h"
#include "music/MusicDatabase.h"
#include "test/TestAnnouncements.h"
#include "test/TestLibrary.h"
#include "video/VideoDatabase.h"
#include "video/VideoInfoTag.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
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

//! A library of this test's own, which the methods open
class TestLibraryItemsInDatabase : public TestLibraryItems
{
public:
  void SetUp() override
  {
    TestLibraryItems::SetUp();

    m_movieId = KODI::TEST::AddMovie(m_videos, MOVIE_FILE, "", SET_NAME);
    ASSERT_GT(m_movieId, 0);
    m_otherMovieId = KODI::TEST::AddMovie(m_videos, "/jsonrpc-test/library/Loner (2002).mkv");
    ASSERT_GT(m_otherMovieId, 0);
    m_setId = std::stoi(m_videos.GetSingleValue(
        m_videos.PrepareSQL("SELECT idSet FROM sets WHERE strSet = '%s'", SET_NAME)));

    m_showId = AddShow("/jsonrpc-test/library/Show/");
    ASSERT_GT(m_showId, 0);
    m_episodeId = KODI::TEST::AddEpisode(m_videos, m_showId, EPISODE_FILE, 1);
    ASSERT_GT(m_episodeId, 0);
    m_seasonId = m_videos.GetSeasonId(m_showId, 1);
    ASSERT_GT(m_seasonId, 0);

    m_songId = m_library.AddSong("JSON-RPC test song", "JSON-RPC test album", ARTIST_NAME);
    ASSERT_GT(m_songId, 0);
    m_albumId = std::stoi(m_music.GetSingleValue(
        m_music.PrepareSQL("SELECT idAlbum FROM song WHERE idSong = %i", m_songId)));
    m_artistId = std::stoi(m_music.GetSingleValue(
        m_music.PrepareSQL("SELECT idArtist FROM song_artist WHERE idSong = %i", m_songId)));
  }

  static constexpr const char* SET_NAME = "JSON-RPC test set";
  static constexpr const char* ARTIST_NAME = "JSON-RPC test artist";

  int AddShow(const std::string& showPath, const std::string& title = "JSON-RPC test show")
  {
    return KODI::TEST::AddTvShow(m_videos, title, {showPath});
  }

  //! A show of two episodes, one of them watched
  int AddShowInProgress(const std::string& title)
  {
    const std::string path{"/jsonrpc-test/library/" + title + "/"};
    const int idShow{AddShow(path, title)};
    const int idEpisode{KODI::TEST::AddEpisode(m_videos, idShow, path + "S01E01.mkv", 1)};
    KODI::TEST::AddEpisode(m_videos, idShow, path + "S01E02.mkv", 2);

    CVideoInfoTag watched;
    m_videos.GetEpisodeInfo("", watched, idEpisode);
    m_videos.SetPlayCount(CFileItem(watched), 1);
    return idShow;
  }

  static constexpr const char* LAST_PLAYED = "2026-09-02 20:00:00";

  //! Records \p fileAndPath as played \p playCount times, last at LAST_PLAYED
  void Play(const std::string& fileAndPath, int playCount)
  {
    CDateTime lastPlayed;
    lastPlayed.SetFromDBDateTime(LAST_PLAYED);
    m_videos.SetPlayCount(CFileItem(fileAndPath, false), playCount, lastPlayed);
  }

  //! The playcount and last played date stored for \p fileAndPath, the date empty if there is none
  std::pair<int, std::string> Played(const std::string& fileAndPath)
  {
    const CDateTime lastPlayed{m_videos.GetLastPlayed(fileAndPath)};
    return {m_videos.GetPlayCount(fileAndPath),
            lastPlayed.IsValid() ? lastPlayed.GetAsDBDateTime() : ""};
  }

  JSONRPC_STATUS SetProperties(const char* kind, int id, const std::string& properties)
  {
    CVariant result;
    return Invoke("VideoLibrary.SetItemProperties",
                  R"({"item": {"kind": ")" + std::string(kind) + R"(", "id": )" +
                      std::to_string(id) + R"(}, "properties": )" + properties + "}",
                  result);
  }

  std::string TitleOf(const char* kind, int id)
  {
    CVariant result;
    Invoke("VideoLibrary.GetItemProperties",
           R"({"item": {"kind": ")" + std::string(kind) + R"(", "id": )" + std::to_string(id) +
               R"(}, "properties": ["title"]})",
           result);
    return result["title"].asString();
  }

  static constexpr const char* MOVIE_FILE = "/jsonrpc-test/library/Set Member (2001).mkv";
  static constexpr const char* EPISODE_FILE = "/jsonrpc-test/library/Show/Show.S01E01.mkv";

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
  CTestLibrary m_library{"TestLibraryItems"};
  CVideoDatabase& m_videos{m_library.Video()};
  CMusicDatabase& m_music{m_library.Music()};
  int m_movieId{-1};
  int m_otherMovieId{-1};
  int m_setId{-1};
  int m_showId{-1};
  int m_seasonId{-1};
  int m_episodeId{-1};
  int m_artistId{-1};
  int m_albumId{-1};
  int m_songId{-1};
};

//! The updates \p announcements holds about the item of \p kind with \p id, as JSON-RPC gives them
std::vector<CVariant> UpdatesTo(const CQueuedAnnouncements& announcements,
                                const std::string& kind,
                                int id)
{
  std::vector<CVariant> updates;
  for (const auto& update : announcements.LibraryUpdates())
  {
    const CVariant& item{update.data.isMember("item") ? update.data["item"] : update.data};
    if (item["type"].asString() == kind && item["id"].asInteger() == id)
      updates.push_back(update.data);
  }
  return updates;
}
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
                             R"({"item": {"kind": "movie", "id": )" +
                                 std::to_string(m_otherMovieId + 1) + "}}",
                             result));
  EXPECT_EQ(NotFound,
            Invoke("VideoLibrary.SetItemProperties",
                   R"({"item": {"kind": "episode", "id": )" + std::to_string(m_episodeId + 1) +
                       R"(}, "properties": {"title": "Nobody"}})",
                   result));
  EXPECT_EQ(NotFound,
            Invoke("AudioLibrary.GetItemProperties",
                   R"({"item": {"kind": "song", "id": )" + std::to_string(m_songId + 1) + "}}",
                   result));
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
  EXPECT_EQ(ARTIST_NAME, result["artist"].asString());
}

TEST_F(TestLibraryItemsInDatabase, SettingAnnouncesTheItemAndWhatChanged)
{
  const auto announcements{std::make_shared<CQueuedAnnouncements>()};
  const CScopedAnnouncementManager registered{announcements};

  CVariant result;
  ASSERT_EQ(OK, Invoke("VideoLibrary.SetItemProperties",
                       R"({"item": {"kind": "movie", "id": )" + std::to_string(m_movieId) +
                           R"(}, "properties": {"plot": "Announced"}})",
                       result));

  const std::vector<CVariant> updates{UpdatesTo(*announcements, "movie", m_movieId)};
  ASSERT_EQ(1u, updates.size());
  EXPECT_EQ("Announced", updates[0]["properties"]["plot"].asString());
  EXPECT_EQ(1u, updates[0]["properties"].size());
}

TEST_F(TestLibraryItemsInDatabase, APlayCountSetIsAnnouncedOnce)
{
  const auto announcements{std::make_shared<CQueuedAnnouncements>()};
  const CScopedAnnouncementManager registered{announcements};

  CVariant result;
  ASSERT_EQ(OK, Invoke("VideoLibrary.SetItemProperties",
                       R"({"item": {"kind": "movie", "id": )" + std::to_string(m_movieId) +
                           R"(}, "properties": {"playCount": 3}})",
                       result));
  ASSERT_EQ(OK, Invoke("AudioLibrary.SetItemProperties",
                       R"({"item": {"kind": "song", "id": )" + std::to_string(m_songId) +
                           R"(}, "properties": {"playCount": 4}})",
                       result));

  const std::vector<CVariant> movie{UpdatesTo(*announcements, "movie", m_movieId)};
  ASSERT_EQ(1u, movie.size());
  EXPECT_EQ(3, movie[0]["properties"]["playCount"].asInteger());

  const std::vector<CVariant> song{UpdatesTo(*announcements, "song", m_songId)};
  ASSERT_EQ(1u, song.size());
  EXPECT_EQ(4, song[0]["properties"]["playCount"].asInteger());
}

TEST_F(TestLibraryItemsInDatabase, ClearingTheLastPlayedDateOfAWatchedMovieIsRefused)
{
  Play(MOVIE_FILE, 2);
  const std::string title{TitleOf("movie", m_movieId)};

  EXPECT_EQ(InvalidParams,
            SetProperties("movie", m_movieId, R"({"title": "Changed", "lastPlayed": ""})"));

  EXPECT_EQ(std::pair(2, std::string(LAST_PLAYED)), Played(MOVIE_FILE));
  EXPECT_EQ(title, TitleOf("movie", m_movieId));
}

TEST_F(TestLibraryItemsInDatabase, ClearingAMovieWithPlayCountZeroClearsBoth)
{
  Play(MOVIE_FILE, 2);

  EXPECT_EQ(OK, SetProperties("movie", m_movieId, R"({"playCount": 0, "lastPlayed": ""})"));

  EXPECT_EQ(std::pair(0, std::string()), Played(MOVIE_FILE));
}

TEST_F(TestLibraryItemsInDatabase, InProgressShowsAreSortedAndLimitedAsAsked)
{
  const int first{AddShowInProgress("AAA JSON-RPC in progress")};
  const int last{AddShowInProgress("zzz JSON-RPC in progress")};

  CVariant ascending;
  ASSERT_EQ(OK, Invoke("VideoLibrary.GetInProgressTVShows",
                       R"({"sort": {"method": "title", "order": "ascending"}})", ascending));
  const int total{static_cast<int>(ascending["limits"]["total"].asInteger())};
  ASSERT_EQ(2, total);
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
  const auto announcements{std::make_shared<CQueuedAnnouncements>()};
  const CScopedAnnouncementManager registered{announcements};

  CVariant result;
  ASSERT_EQ(OK,
            Invoke("VideoLibrary.AddItem",
                   R"({"kind": "musicvideo", "path": "/jsonrpc-test/library/Clip.mkv"})", result));
  const int id{static_cast<int>(result["musicVideoId"].asInteger())};
  ASSERT_GT(id, 0);

  const std::vector<CVariant> updates{UpdatesTo(*announcements, "musicvideo", id)};
  ASSERT_EQ(1u, updates.size());
  EXPECT_TRUE(updates[0]["added"].asBoolean());
  EXPECT_TRUE(std::ranges::any_of(
      announcements->LibraryUpdates(), [id](const CQueuedAnnouncements::LibraryUpdate& update)
      { return update.type == KODI::MEDIA::TYPE::MUSIC_VIDEO && update.id == id; }));
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
                                 "tvShowId": )" +
                                 std::to_string(m_showId + 1) + "}",
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
