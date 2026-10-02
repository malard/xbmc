/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "dbwrappers/qry_dat.h"
#include "music/MusicDatabase.h"
#include "utils/DatabaseUtils.h"
#include "utils/StringUtils.h"
#include "utils/Variant.h"
#include "video/VideoDatabase.h"
#include "video/VideoDatabaseColumns.h"

#include <gtest/gtest.h>

using KODI::MEDIA::MediaType;

class TestDatabaseUtilsHelper
{
public:
  TestDatabaseUtilsHelper()
  {
    album_idAlbum = CMusicDatabase::album_idAlbum;
    album_strAlbum = CMusicDatabase::album_strAlbum;
    album_strArtists = CMusicDatabase::album_strArtists;
    album_strGenres = CMusicDatabase::album_strGenres;
    album_strMoods = CMusicDatabase::album_strMoods;
    album_strReleaseDate = CMusicDatabase::album_strReleaseDate;
    album_strOrigReleaseDate = CMusicDatabase::album_strOrigReleaseDate;
    album_strStyles = CMusicDatabase::album_strStyles;
    album_strThemes = CMusicDatabase::album_strThemes;
    album_strReview = CMusicDatabase::album_strReview;
    album_strLabel = CMusicDatabase::album_strLabel;
    album_strType = CMusicDatabase::album_strType;
    album_fRating = CMusicDatabase::album_fRating;
    album_iVotes = CMusicDatabase::album_iVotes;
    album_iUserrating = CMusicDatabase::album_iUserrating;
    album_dtDateAdded = CMusicDatabase::album_dateAdded;

    song_idSong = CMusicDatabase::song_idSong;
    song_strTitle = CMusicDatabase::song_strTitle;
    song_iTrack = CMusicDatabase::song_iTrack;
    song_iDuration = CMusicDatabase::song_iDuration;
    song_strReleaseDate = CMusicDatabase::song_strReleaseDate;
    song_strOrigReleaseDate = CMusicDatabase::song_strOrigReleaseDate;
    song_strFileName = CMusicDatabase::song_strFileName;
    song_iTimesPlayed = CMusicDatabase::song_iTimesPlayed;
    song_iStartOffset = CMusicDatabase::song_iStartOffset;
    song_iEndOffset = CMusicDatabase::song_iEndOffset;
    song_lastplayed = CMusicDatabase::song_lastplayed;
    song_rating = CMusicDatabase::song_rating;
    song_votes = CMusicDatabase::song_votes;
    song_userrating = CMusicDatabase::song_userrating;
    song_comment = CMusicDatabase::song_comment;
    song_strAlbum = CMusicDatabase::song_strAlbum;
    song_strPath = CMusicDatabase::song_strPath;
    song_strGenres = CMusicDatabase::song_strGenres;
    song_strArtists = CMusicDatabase::song_strArtists;
  }

  int album_idAlbum;
  int album_strAlbum;
  int album_strArtists;
  int album_strGenres;
  int album_strMoods;
  int album_strReleaseDate;
  int album_strOrigReleaseDate;
  int album_strStyles;
  int album_strThemes;
  int album_strReview;
  int album_strLabel;
  int album_strType;
  int album_fRating;
  int album_iVotes;
  int album_iUserrating;
  int album_dtDateAdded;

  int song_idSong;
  int song_strTitle;
  int song_iTrack;
  int song_iDuration;
  int song_strReleaseDate;
  int song_strOrigReleaseDate;
  int song_strFileName;
  int song_iTimesPlayed;
  int song_iStartOffset;
  int song_iEndOffset;
  int song_lastplayed;
  int song_rating;
  int song_votes;
  int song_userrating;
  int song_comment;
  int song_strAlbum;
  int song_strPath;
  int song_strGenres;
  int song_strArtists;
};

TEST(TestDatabaseUtils, GetField_None)
{
  std::string refstr, varstr;

  refstr = "";
  varstr = DatabaseUtils::GetField(Field::NONE, MediaType::NONE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  varstr = DatabaseUtils::GetField(Field::NONE, MediaType::MOVIE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());
}

TEST(TestDatabaseUtils, GetField_MediaTypeAlbum)
{
  std::string refstr, varstr;

  refstr = "albumview.idAlbum";
  varstr = DatabaseUtils::GetField(Field::ID, MediaType::ALBUM, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "albumview.strAlbum";
  varstr = DatabaseUtils::GetField(Field::ALBUM, MediaType::ALBUM, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "albumview.strArtists";
  varstr = DatabaseUtils::GetField(Field::ARTIST, MediaType::ALBUM, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "albumview.strArtists";
  varstr =
      DatabaseUtils::GetField(Field::ALBUM_ARTIST, MediaType::ALBUM, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "albumview.strGenres";
  varstr = DatabaseUtils::GetField(Field::GENRE, MediaType::ALBUM, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "albumview.strReleaseDate";
  varstr = DatabaseUtils::GetField(Field::YEAR, MediaType::ALBUM, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "albumview.strOrigReleaseDate";
  varstr = DatabaseUtils::GetField(Field::ORIG_YEAR, MediaType::ALBUM, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "albumview.strMoods";
  varstr = DatabaseUtils::GetField(Field::MOODS, MediaType::ALBUM, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "albumview.strStyles";
  varstr = DatabaseUtils::GetField(Field::STYLES, MediaType::ALBUM, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "albumview.strThemes";
  varstr = DatabaseUtils::GetField(Field::THEMES, MediaType::ALBUM, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "albumview.strReview";
  varstr = DatabaseUtils::GetField(Field::REVIEW, MediaType::ALBUM, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "albumview.strLabel";
  varstr = DatabaseUtils::GetField(Field::MUSIC_LABEL, MediaType::ALBUM, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "albumview.strType";
  varstr = DatabaseUtils::GetField(Field::ALBUM_TYPE, MediaType::ALBUM, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "albumview.fRating";
  varstr = DatabaseUtils::GetField(Field::RATING, MediaType::ALBUM, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "albumview.iVotes";
  varstr = DatabaseUtils::GetField(Field::VOTES, MediaType::ALBUM, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "albumview.iUserrating";
  varstr = DatabaseUtils::GetField(Field::USER_RATING, MediaType::ALBUM, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "albumview.dateAdded";
  varstr = DatabaseUtils::GetField(Field::DATE_ADDED, MediaType::ALBUM, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "";
  varstr = DatabaseUtils::GetField(Field::NONE, MediaType::ALBUM, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "albumview.strAlbum";
  varstr = DatabaseUtils::GetField(Field::ALBUM, MediaType::ALBUM, DatabaseQueryPart::WHERE);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  varstr = DatabaseUtils::GetField(Field::ALBUM, MediaType::ALBUM, DatabaseQueryPart::ORDER_BY);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());
}

TEST(TestDatabaseUtils, GetField_MediaTypeSong)
{
  std::string refstr, varstr;

  refstr = "songview.idSong";
  varstr = DatabaseUtils::GetField(Field::ID, MediaType::SONG, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "songview.strTitle";
  varstr = DatabaseUtils::GetField(Field::TITLE, MediaType::SONG, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "songview.iTrack";
  varstr = DatabaseUtils::GetField(Field::TRACK_NUMBER, MediaType::SONG, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "songview.iDuration";
  varstr = DatabaseUtils::GetField(Field::TIME, MediaType::SONG, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "songview.strFilename";
  varstr = DatabaseUtils::GetField(Field::FILENAME, MediaType::SONG, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "songview.iTimesPlayed";
  varstr = DatabaseUtils::GetField(Field::PLAYCOUNT, MediaType::SONG, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "songview.iStartOffset";
  varstr = DatabaseUtils::GetField(Field::START_OFFSET, MediaType::SONG, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "songview.iEndOffset";
  varstr = DatabaseUtils::GetField(Field::END_OFFSET, MediaType::SONG, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "songview.lastPlayed";
  varstr = DatabaseUtils::GetField(Field::LAST_PLAYED, MediaType::SONG, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "songview.rating";
  varstr = DatabaseUtils::GetField(Field::RATING, MediaType::SONG, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "songview.votes";
  varstr = DatabaseUtils::GetField(Field::VOTES, MediaType::SONG, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "songview.userrating";
  varstr = DatabaseUtils::GetField(Field::USER_RATING, MediaType::SONG, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "songview.comment";
  varstr = DatabaseUtils::GetField(Field::COMMENT, MediaType::SONG, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "songview.strReleaseDate";
  varstr = DatabaseUtils::GetField(Field::YEAR, MediaType::SONG, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "songview.strOrigReleaseDate";
  varstr = DatabaseUtils::GetField(Field::ORIG_YEAR, MediaType::SONG, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "songview.strAlbum";
  varstr = DatabaseUtils::GetField(Field::ALBUM, MediaType::SONG, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "songview.strPath";
  varstr = DatabaseUtils::GetField(Field::PATH, MediaType::SONG, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "songview.strArtists";
  varstr = DatabaseUtils::GetField(Field::ARTIST, MediaType::SONG, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "songview.strArtists";
  varstr = DatabaseUtils::GetField(Field::ALBUM_ARTIST, MediaType::SONG, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "songview.strGenres";
  varstr = DatabaseUtils::GetField(Field::GENRE, MediaType::SONG, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "songview.dateAdded";
  varstr = DatabaseUtils::GetField(Field::DATE_ADDED, MediaType::SONG, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "songview.strPath";
  varstr = DatabaseUtils::GetField(Field::PATH, MediaType::SONG, DatabaseQueryPart::WHERE);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  varstr = DatabaseUtils::GetField(Field::PATH, MediaType::SONG, DatabaseQueryPart::ORDER_BY);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());
}

TEST(TestDatabaseUtils, GetField_MediaTypeMusicVideo)
{
  std::string refstr, varstr;

  refstr = "musicvideo_view.idMVideo";
  varstr = DatabaseUtils::GetField(Field::ID, MediaType::MUSIC_VIDEO, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = StringUtils::Format("musicvideo_view.c{:02}", VIDEODB_ID_MUSICVIDEO_TITLE);
  varstr = DatabaseUtils::GetField(Field::TITLE, MediaType::MUSIC_VIDEO, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = StringUtils::Format("musicvideo_view.c{:02}", VIDEODB_ID_MUSICVIDEO_RUNTIME);
  varstr = DatabaseUtils::GetField(Field::TIME, MediaType::MUSIC_VIDEO, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = StringUtils::Format("musicvideo_view.c{:02}", VIDEODB_ID_MUSICVIDEO_DIRECTOR);
  varstr =
      DatabaseUtils::GetField(Field::DIRECTOR, MediaType::MUSIC_VIDEO, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = StringUtils::Format("musicvideo_view.c{:02}", VIDEODB_ID_MUSICVIDEO_STUDIOS);
  varstr =
      DatabaseUtils::GetField(Field::STUDIO, MediaType::MUSIC_VIDEO, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = StringUtils::Format("musicvideo_view.c{:02}", VIDEODB_ID_MUSICVIDEO_PLOT);
  varstr = DatabaseUtils::GetField(Field::PLOT, MediaType::MUSIC_VIDEO, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = StringUtils::Format("musicvideo_view.c{:02}", VIDEODB_ID_MUSICVIDEO_ALBUM);
  varstr = DatabaseUtils::GetField(Field::ALBUM, MediaType::MUSIC_VIDEO, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = StringUtils::Format("musicvideo_view.c{:02}", VIDEODB_ID_MUSICVIDEO_ARTIST);
  varstr =
      DatabaseUtils::GetField(Field::ARTIST, MediaType::MUSIC_VIDEO, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = StringUtils::Format("musicvideo_view.c{:02}", VIDEODB_ID_MUSICVIDEO_GENRE);
  varstr = DatabaseUtils::GetField(Field::GENRE, MediaType::MUSIC_VIDEO, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = StringUtils::Format("musicvideo_view.c{:02}", VIDEODB_ID_MUSICVIDEO_TRACK);
  varstr = DatabaseUtils::GetField(Field::TRACK_NUMBER, MediaType::MUSIC_VIDEO,
                                   DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "musicvideo_view.strFilename";
  varstr =
      DatabaseUtils::GetField(Field::FILENAME, MediaType::MUSIC_VIDEO, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "musicvideo_view.strPath";
  varstr = DatabaseUtils::GetField(Field::PATH, MediaType::MUSIC_VIDEO, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "musicvideo_view.playCount";
  varstr =
      DatabaseUtils::GetField(Field::PLAYCOUNT, MediaType::MUSIC_VIDEO, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "musicvideo_view.lastPlayed";
  varstr = DatabaseUtils::GetField(Field::LAST_PLAYED, MediaType::MUSIC_VIDEO,
                                   DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "musicvideo_view.dateAdded";
  varstr =
      DatabaseUtils::GetField(Field::DATE_ADDED, MediaType::MUSIC_VIDEO, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "";
  varstr = DatabaseUtils::GetField(Field::VIDEO_RESOLUTION, MediaType::MUSIC_VIDEO,
                                   DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "musicvideo_view.strPath";
  varstr = DatabaseUtils::GetField(Field::PATH, MediaType::MUSIC_VIDEO, DatabaseQueryPart::WHERE);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "musicvideo_view.strPath";
  varstr =
      DatabaseUtils::GetField(Field::PATH, MediaType::MUSIC_VIDEO, DatabaseQueryPart::ORDER_BY);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "musicvideo_view.userrating";
  varstr = DatabaseUtils::GetField(Field::USER_RATING, MediaType::MUSIC_VIDEO,
                                   DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());
}

TEST(TestDatabaseUtils, GetField_MediaTypeMovie)
{
  std::string refstr, varstr;

  refstr = "movie_view.idMovie";
  varstr = DatabaseUtils::GetField(Field::ID, MediaType::MOVIE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = StringUtils::Format("movie_view.c{:02}", VIDEODB_ID_TITLE);
  varstr = DatabaseUtils::GetField(Field::TITLE, MediaType::MOVIE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = StringUtils::Format("CASE WHEN length(movie_view.c{:02}) > 0 THEN movie_view.c{:02} "
                               "ELSE movie_view.c{:02} END",
                               VIDEODB_ID_SORTTITLE, VIDEODB_ID_SORTTITLE, VIDEODB_ID_TITLE);
  varstr = DatabaseUtils::GetField(Field::TITLE, MediaType::MOVIE, DatabaseQueryPart::ORDER_BY);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = StringUtils::Format("movie_view.c{:02}", VIDEODB_ID_PLOT);
  varstr = DatabaseUtils::GetField(Field::PLOT, MediaType::MOVIE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = StringUtils::Format("movie_view.c{:02}", VIDEODB_ID_PLOTOUTLINE);
  varstr =
      DatabaseUtils::GetField(Field::PLOT_OUTLINE, MediaType::MOVIE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = StringUtils::Format("movie_view.c{:02}", VIDEODB_ID_TAGLINE);
  varstr = DatabaseUtils::GetField(Field::TAGLINE, MediaType::MOVIE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "movie_view.votes";
  varstr = DatabaseUtils::GetField(Field::VOTES, MediaType::MOVIE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "movie_view.rating";
  varstr = DatabaseUtils::GetField(Field::RATING, MediaType::MOVIE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = StringUtils::Format("movie_view.c{:02}", VIDEODB_ID_CREDITS);
  varstr = DatabaseUtils::GetField(Field::WRITER, MediaType::MOVIE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = StringUtils::Format("movie_view.c{:02}", VIDEODB_ID_SORTTITLE);
  varstr = DatabaseUtils::GetField(Field::SORT_TITLE, MediaType::MOVIE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = StringUtils::Format("movie_view.c{:02}", VIDEODB_ID_RUNTIME);
  varstr = DatabaseUtils::GetField(Field::TIME, MediaType::MOVIE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = StringUtils::Format("movie_view.c{:02}", VIDEODB_ID_MPAA);
  varstr = DatabaseUtils::GetField(Field::MPAA, MediaType::MOVIE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = StringUtils::Format("movie_view.c{:02}", VIDEODB_ID_TOP250);
  varstr = DatabaseUtils::GetField(Field::TOP250, MediaType::MOVIE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = StringUtils::Format("movie_view.c{:02}", VIDEODB_ID_GENRE);
  varstr = DatabaseUtils::GetField(Field::GENRE, MediaType::MOVIE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = StringUtils::Format("movie_view.c{:02}", VIDEODB_ID_DIRECTOR);
  varstr = DatabaseUtils::GetField(Field::DIRECTOR, MediaType::MOVIE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = StringUtils::Format("movie_view.c{:02}", VIDEODB_ID_STUDIOS);
  varstr = DatabaseUtils::GetField(Field::STUDIO, MediaType::MOVIE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = StringUtils::Format("movie_view.c{:02}", VIDEODB_ID_TRAILER);
  varstr = DatabaseUtils::GetField(Field::TRAILER, MediaType::MOVIE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = StringUtils::Format("movie_view.c{:02}", VIDEODB_ID_COUNTRY);
  varstr = DatabaseUtils::GetField(Field::COUNTRY, MediaType::MOVIE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "movie_view.strFilename";
  varstr = DatabaseUtils::GetField(Field::FILENAME, MediaType::MOVIE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "movie_view.strPath";
  varstr = DatabaseUtils::GetField(Field::PATH, MediaType::MOVIE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "movie_view.playCount";
  varstr = DatabaseUtils::GetField(Field::PLAYCOUNT, MediaType::MOVIE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "movie_view.lastPlayed";
  varstr = DatabaseUtils::GetField(Field::LAST_PLAYED, MediaType::MOVIE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "movie_view.dateAdded";
  varstr = DatabaseUtils::GetField(Field::DATE_ADDED, MediaType::MOVIE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "movie_view.userrating";
  varstr = DatabaseUtils::GetField(Field::USER_RATING, MediaType::MOVIE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "";
  varstr = DatabaseUtils::GetField(Field::RANDOM, MediaType::MOVIE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());
}

TEST(TestDatabaseUtils, GetField_MediaTypeTvShow)
{
  std::string refstr, varstr;

  refstr = "tvshow_view.idShow";
  varstr = DatabaseUtils::GetField(Field::ID, MediaType::TV_SHOW, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr =
      StringUtils::Format("CASE WHEN length(tvshow_view.c{:02}) > 0 THEN tvshow_view.c{:02} "
                          "ELSE tvshow_view.c{:02} END",
                          VIDEODB_ID_TV_SORTTITLE, VIDEODB_ID_TV_SORTTITLE, VIDEODB_ID_TV_TITLE);
  varstr = DatabaseUtils::GetField(Field::TITLE, MediaType::TV_SHOW, DatabaseQueryPart::ORDER_BY);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = StringUtils::Format("tvshow_view.c{:02}", VIDEODB_ID_TV_TITLE);
  varstr = DatabaseUtils::GetField(Field::TITLE, MediaType::TV_SHOW, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = StringUtils::Format("tvshow_view.c{:02}", VIDEODB_ID_TV_PLOT);
  varstr = DatabaseUtils::GetField(Field::PLOT, MediaType::TV_SHOW, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = StringUtils::Format("tvshow_view.c{:02}", VIDEODB_ID_TV_STATUS);
  varstr =
      DatabaseUtils::GetField(Field::TVSHOW_STATUS, MediaType::TV_SHOW, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "tvshow_view.votes";
  varstr = DatabaseUtils::GetField(Field::VOTES, MediaType::TV_SHOW, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "tvshow_view.rating";
  varstr = DatabaseUtils::GetField(Field::RATING, MediaType::TV_SHOW, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = StringUtils::Format("tvshow_view.c{:02}", VIDEODB_ID_TV_PREMIERED);
  varstr = DatabaseUtils::GetField(Field::YEAR, MediaType::TV_SHOW, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = StringUtils::Format("tvshow_view.c{:02}", VIDEODB_ID_TV_GENRE);
  varstr = DatabaseUtils::GetField(Field::GENRE, MediaType::TV_SHOW, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = StringUtils::Format("tvshow_view.c{:02}", VIDEODB_ID_TV_MPAA);
  varstr = DatabaseUtils::GetField(Field::MPAA, MediaType::TV_SHOW, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = StringUtils::Format("tvshow_view.c{:02}", VIDEODB_ID_TV_STUDIOS);
  varstr = DatabaseUtils::GetField(Field::STUDIO, MediaType::TV_SHOW, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = StringUtils::Format("tvshow_view.c{:02}", VIDEODB_ID_TV_SORTTITLE);
  varstr =
      DatabaseUtils::GetField(Field::SORT_TITLE, MediaType::TV_SHOW, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "tvshow_view.strPath";
  varstr = DatabaseUtils::GetField(Field::PATH, MediaType::TV_SHOW, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "tvshow_view.dateAdded";
  varstr =
      DatabaseUtils::GetField(Field::DATE_ADDED, MediaType::TV_SHOW, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "tvshow_view.totalSeasons";
  varstr = DatabaseUtils::GetField(Field::SEASON, MediaType::TV_SHOW, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "tvshow_view.totalCount";
  varstr = DatabaseUtils::GetField(Field::NUMBER_OF_EPISODES, MediaType::TV_SHOW,
                                   DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "tvshow_view.watchedcount";
  varstr = DatabaseUtils::GetField(Field::NUMBER_OF_WATCHED_EPISODES, MediaType::TV_SHOW,
                                   DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "tvshow_view.userrating";
  varstr =
      DatabaseUtils::GetField(Field::USER_RATING, MediaType::TV_SHOW, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "";
  varstr = DatabaseUtils::GetField(Field::RANDOM, MediaType::TV_SHOW, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());
}

TEST(TestDatabaseUtils, GetField_MediaTypeEpisode)
{
  std::string refstr, varstr;

  refstr = "episode_view.idEpisode";
  varstr = DatabaseUtils::GetField(Field::ID, MediaType::EPISODE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = StringUtils::Format("episode_view.c{:02}", VIDEODB_ID_EPISODE_TITLE);
  varstr = DatabaseUtils::GetField(Field::TITLE, MediaType::EPISODE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = StringUtils::Format("episode_view.c{:02}", VIDEODB_ID_EPISODE_PLOT);
  varstr = DatabaseUtils::GetField(Field::PLOT, MediaType::EPISODE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "episode_view.votes";
  varstr = DatabaseUtils::GetField(Field::VOTES, MediaType::EPISODE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "episode_view.rating";
  varstr = DatabaseUtils::GetField(Field::RATING, MediaType::EPISODE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = StringUtils::Format("episode_view.c{:02}", VIDEODB_ID_EPISODE_CREDITS);
  varstr = DatabaseUtils::GetField(Field::WRITER, MediaType::EPISODE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = StringUtils::Format("episode_view.c{:02}", VIDEODB_ID_EPISODE_AIRED);
  varstr = DatabaseUtils::GetField(Field::AIR_DATE, MediaType::EPISODE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = StringUtils::Format("episode_view.c{:02}", VIDEODB_ID_EPISODE_RUNTIME);
  varstr = DatabaseUtils::GetField(Field::TIME, MediaType::EPISODE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = StringUtils::Format("episode_view.c{:02}", VIDEODB_ID_EPISODE_DIRECTOR);
  varstr = DatabaseUtils::GetField(Field::DIRECTOR, MediaType::EPISODE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = StringUtils::Format("episode_view.c{:02}", VIDEODB_ID_EPISODE_SEASON);
  varstr = DatabaseUtils::GetField(Field::SEASON, MediaType::EPISODE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = StringUtils::Format("episode_view.c{:02}", VIDEODB_ID_EPISODE_EPISODE);
  varstr =
      DatabaseUtils::GetField(Field::EPISODE_NUMBER, MediaType::EPISODE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "episode_view.strFilename";
  varstr = DatabaseUtils::GetField(Field::FILENAME, MediaType::EPISODE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "episode_view.strPath";
  varstr = DatabaseUtils::GetField(Field::PATH, MediaType::EPISODE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "episode_view.playCount";
  varstr = DatabaseUtils::GetField(Field::PLAYCOUNT, MediaType::EPISODE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "episode_view.lastPlayed";
  varstr =
      DatabaseUtils::GetField(Field::LAST_PLAYED, MediaType::EPISODE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "episode_view.dateAdded";
  varstr =
      DatabaseUtils::GetField(Field::DATE_ADDED, MediaType::EPISODE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "episode_view.strTitle";
  varstr =
      DatabaseUtils::GetField(Field::TVSHOW_TITLE, MediaType::EPISODE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "episode_view.premiered";
  varstr = DatabaseUtils::GetField(Field::YEAR, MediaType::EPISODE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "episode_view.mpaa";
  varstr = DatabaseUtils::GetField(Field::MPAA, MediaType::EPISODE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "episode_view.strStudio";
  varstr = DatabaseUtils::GetField(Field::STUDIO, MediaType::EPISODE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "episode_view.userrating";
  varstr =
      DatabaseUtils::GetField(Field::USER_RATING, MediaType::EPISODE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "";
  varstr = DatabaseUtils::GetField(Field::RANDOM, MediaType::EPISODE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());
}

TEST(TestDatabaseUtils, GetField_FieldRandom)
{
  std::string refstr, varstr;

  refstr = "";
  varstr = DatabaseUtils::GetField(Field::RANDOM, MediaType::EPISODE, DatabaseQueryPart::SELECT);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "";
  varstr = DatabaseUtils::GetField(Field::RANDOM, MediaType::EPISODE, DatabaseQueryPart::WHERE);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());

  refstr = "RANDOM()";
  varstr = DatabaseUtils::GetField(Field::RANDOM, MediaType::EPISODE, DatabaseQueryPart::ORDER_BY);
  EXPECT_STREQ(refstr.c_str(), varstr.c_str());
}

TEST(TestDatabaseUtils, GetFieldIndex_None)
{
  int refindex, varindex;

  refindex = -1;
  varindex = DatabaseUtils::GetFieldIndex(Field::RANDOM, MediaType::NONE);
  EXPECT_EQ(refindex, varindex);

  varindex = DatabaseUtils::GetFieldIndex(Field::NONE, MediaType::ALBUM);
  EXPECT_EQ(refindex, varindex);
}

//! @todo Should enums in CMusicDatabase be made public instead?
TEST(TestDatabaseUtils, GetFieldIndex_MediaTypeAlbum)
{
  int refindex, varindex;
  TestDatabaseUtilsHelper a;

  refindex = a.album_idAlbum;
  varindex = DatabaseUtils::GetFieldIndex(Field::ID, MediaType::ALBUM);
  EXPECT_EQ(refindex, varindex);

  refindex = a.album_strAlbum;
  varindex = DatabaseUtils::GetFieldIndex(Field::ALBUM, MediaType::ALBUM);
  EXPECT_EQ(refindex, varindex);

  refindex = a.album_strArtists;
  varindex = DatabaseUtils::GetFieldIndex(Field::ARTIST, MediaType::ALBUM);
  EXPECT_EQ(refindex, varindex);

  refindex = a.album_strArtists;
  varindex = DatabaseUtils::GetFieldIndex(Field::ALBUM_ARTIST, MediaType::ALBUM);
  EXPECT_EQ(refindex, varindex);

  refindex = a.album_strGenres;
  varindex = DatabaseUtils::GetFieldIndex(Field::GENRE, MediaType::ALBUM);
  EXPECT_EQ(refindex, varindex);

  refindex = a.album_strReleaseDate;
  varindex = DatabaseUtils::GetFieldIndex(Field::YEAR, MediaType::ALBUM);
  EXPECT_EQ(refindex, varindex);

  refindex = a.album_strOrigReleaseDate;
  varindex = DatabaseUtils::GetFieldIndex(Field::ORIG_YEAR, MediaType::ALBUM);
  EXPECT_EQ(refindex, varindex);

  refindex = a.album_strMoods;
  varindex = DatabaseUtils::GetFieldIndex(Field::MOODS, MediaType::ALBUM);
  EXPECT_EQ(refindex, varindex);

  refindex = a.album_strStyles;
  varindex = DatabaseUtils::GetFieldIndex(Field::STYLES, MediaType::ALBUM);
  EXPECT_EQ(refindex, varindex);

  refindex = a.album_strThemes;
  varindex = DatabaseUtils::GetFieldIndex(Field::THEMES, MediaType::ALBUM);
  EXPECT_EQ(refindex, varindex);

  refindex = a.album_strReview;
  varindex = DatabaseUtils::GetFieldIndex(Field::REVIEW, MediaType::ALBUM);
  EXPECT_EQ(refindex, varindex);

  refindex = a.album_strLabel;
  varindex = DatabaseUtils::GetFieldIndex(Field::MUSIC_LABEL, MediaType::ALBUM);
  EXPECT_EQ(refindex, varindex);

  refindex = a.album_strType;
  varindex = DatabaseUtils::GetFieldIndex(Field::ALBUM_TYPE, MediaType::ALBUM);
  EXPECT_EQ(refindex, varindex);

  refindex = a.album_fRating;
  varindex = DatabaseUtils::GetFieldIndex(Field::RATING, MediaType::ALBUM);
  EXPECT_EQ(refindex, varindex);

  refindex = a.album_dtDateAdded;
  varindex = DatabaseUtils::GetFieldIndex(Field::DATE_ADDED, MediaType::ALBUM);
  EXPECT_EQ(refindex, varindex);

  refindex = -1;
  varindex = DatabaseUtils::GetFieldIndex(Field::RANDOM, MediaType::ALBUM);
  EXPECT_EQ(refindex, varindex);
}

TEST(TestDatabaseUtils, GetFieldIndex_MediaTypeSong)
{
  int refindex, varindex;
  TestDatabaseUtilsHelper a;

  refindex = a.song_idSong;
  varindex = DatabaseUtils::GetFieldIndex(Field::ID, MediaType::SONG);
  EXPECT_EQ(refindex, varindex);

  refindex = a.song_strTitle;
  varindex = DatabaseUtils::GetFieldIndex(Field::TITLE, MediaType::SONG);
  EXPECT_EQ(refindex, varindex);

  refindex = a.song_iTrack;
  varindex = DatabaseUtils::GetFieldIndex(Field::TRACK_NUMBER, MediaType::SONG);
  EXPECT_EQ(refindex, varindex);

  refindex = a.song_iDuration;
  varindex = DatabaseUtils::GetFieldIndex(Field::TIME, MediaType::SONG);
  EXPECT_EQ(refindex, varindex);

  refindex = a.song_strReleaseDate;
  varindex = DatabaseUtils::GetFieldIndex(Field::YEAR, MediaType::SONG);
  EXPECT_EQ(refindex, varindex);

  refindex = a.song_strFileName;
  varindex = DatabaseUtils::GetFieldIndex(Field::FILENAME, MediaType::SONG);
  EXPECT_EQ(refindex, varindex);

  refindex = a.song_iTimesPlayed;
  varindex = DatabaseUtils::GetFieldIndex(Field::PLAYCOUNT, MediaType::SONG);
  EXPECT_EQ(refindex, varindex);

  refindex = a.song_iStartOffset;
  varindex = DatabaseUtils::GetFieldIndex(Field::START_OFFSET, MediaType::SONG);
  EXPECT_EQ(refindex, varindex);

  refindex = a.song_iEndOffset;
  varindex = DatabaseUtils::GetFieldIndex(Field::END_OFFSET, MediaType::SONG);
  EXPECT_EQ(refindex, varindex);

  refindex = a.song_lastplayed;
  varindex = DatabaseUtils::GetFieldIndex(Field::LAST_PLAYED, MediaType::SONG);
  EXPECT_EQ(refindex, varindex);

  refindex = a.song_rating;
  varindex = DatabaseUtils::GetFieldIndex(Field::RATING, MediaType::SONG);
  EXPECT_EQ(refindex, varindex);

  refindex = a.song_votes;
  varindex = DatabaseUtils::GetFieldIndex(Field::VOTES, MediaType::SONG);
  EXPECT_EQ(refindex, varindex);

  refindex = a.song_userrating;
  varindex = DatabaseUtils::GetFieldIndex(Field::USER_RATING, MediaType::SONG);
  EXPECT_EQ(refindex, varindex);

  refindex = a.song_comment;
  varindex = DatabaseUtils::GetFieldIndex(Field::COMMENT, MediaType::SONG);
  EXPECT_EQ(refindex, varindex);

  refindex = a.song_strAlbum;
  varindex = DatabaseUtils::GetFieldIndex(Field::ALBUM, MediaType::SONG);
  EXPECT_EQ(refindex, varindex);

  refindex = a.song_strPath;
  varindex = DatabaseUtils::GetFieldIndex(Field::PATH, MediaType::SONG);
  EXPECT_EQ(refindex, varindex);

  refindex = a.song_strArtists;
  varindex = DatabaseUtils::GetFieldIndex(Field::ARTIST, MediaType::SONG);
  EXPECT_EQ(refindex, varindex);

  refindex = a.song_strGenres;
  varindex = DatabaseUtils::GetFieldIndex(Field::GENRE, MediaType::SONG);
  EXPECT_EQ(refindex, varindex);

  refindex = -1;
  varindex = DatabaseUtils::GetFieldIndex(Field::RANDOM, MediaType::SONG);
  EXPECT_EQ(refindex, varindex);
}

TEST(TestDatabaseUtils, GetFieldIndex_MediaTypeMusicVideo)
{
  int refindex, varindex;

  refindex = 0;
  varindex = DatabaseUtils::GetFieldIndex(Field::ID, MediaType::MUSIC_VIDEO);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_ID_MUSICVIDEO_TITLE + 2;
  varindex = DatabaseUtils::GetFieldIndex(Field::TITLE, MediaType::MUSIC_VIDEO);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_ID_MUSICVIDEO_RUNTIME + 2;
  varindex = DatabaseUtils::GetFieldIndex(Field::TIME, MediaType::MUSIC_VIDEO);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_ID_MUSICVIDEO_DIRECTOR + 2;
  varindex = DatabaseUtils::GetFieldIndex(Field::DIRECTOR, MediaType::MUSIC_VIDEO);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_ID_MUSICVIDEO_STUDIOS + 2;
  varindex = DatabaseUtils::GetFieldIndex(Field::STUDIO, MediaType::MUSIC_VIDEO);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_ID_MUSICVIDEO_PLOT + 2;
  varindex = DatabaseUtils::GetFieldIndex(Field::PLOT, MediaType::MUSIC_VIDEO);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_ID_MUSICVIDEO_ALBUM + 2;
  varindex = DatabaseUtils::GetFieldIndex(Field::ALBUM, MediaType::MUSIC_VIDEO);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_ID_MUSICVIDEO_ARTIST + 2;
  varindex = DatabaseUtils::GetFieldIndex(Field::ARTIST, MediaType::MUSIC_VIDEO);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_ID_MUSICVIDEO_GENRE + 2;
  varindex = DatabaseUtils::GetFieldIndex(Field::GENRE, MediaType::MUSIC_VIDEO);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_ID_MUSICVIDEO_TRACK + 2;
  varindex = DatabaseUtils::GetFieldIndex(Field::TRACK_NUMBER, MediaType::MUSIC_VIDEO);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_DETAILS_MUSICVIDEO_FILE;
  varindex = DatabaseUtils::GetFieldIndex(Field::FILENAME, MediaType::MUSIC_VIDEO);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_DETAILS_MUSICVIDEO_PATH;
  varindex = DatabaseUtils::GetFieldIndex(Field::PATH, MediaType::MUSIC_VIDEO);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_DETAILS_MUSICVIDEO_PLAYCOUNT;
  varindex = DatabaseUtils::GetFieldIndex(Field::PLAYCOUNT, MediaType::MUSIC_VIDEO);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_DETAILS_MUSICVIDEO_LASTPLAYED;
  varindex = DatabaseUtils::GetFieldIndex(Field::LAST_PLAYED, MediaType::MUSIC_VIDEO);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_DETAILS_MUSICVIDEO_DATEADDED;
  varindex = DatabaseUtils::GetFieldIndex(Field::DATE_ADDED, MediaType::MUSIC_VIDEO);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_DETAILS_MUSICVIDEO_USER_RATING;
  varindex = DatabaseUtils::GetFieldIndex(Field::USER_RATING, MediaType::MUSIC_VIDEO);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_DETAILS_MUSICVIDEO_PREMIERED;
  varindex = DatabaseUtils::GetFieldIndex(Field::YEAR, MediaType::MUSIC_VIDEO);
  EXPECT_EQ(refindex, varindex);

  refindex = -1;
  varindex = DatabaseUtils::GetFieldIndex(Field::RANDOM, MediaType::MUSIC_VIDEO);
  EXPECT_EQ(refindex, varindex);
}

TEST(TestDatabaseUtils, GetFieldIndex_MediaTypeMovie)
{
  int refindex, varindex;

  refindex = 0;
  varindex = DatabaseUtils::GetFieldIndex(Field::ID, MediaType::MOVIE);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_ID_TITLE + 2;
  varindex = DatabaseUtils::GetFieldIndex(Field::TITLE, MediaType::MOVIE);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_ID_SORTTITLE + 2;
  varindex = DatabaseUtils::GetFieldIndex(Field::SORT_TITLE, MediaType::MOVIE);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_ID_PLOT + 2;
  varindex = DatabaseUtils::GetFieldIndex(Field::PLOT, MediaType::MOVIE);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_ID_PLOTOUTLINE + 2;
  varindex = DatabaseUtils::GetFieldIndex(Field::PLOT_OUTLINE, MediaType::MOVIE);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_ID_TAGLINE + 2;
  varindex = DatabaseUtils::GetFieldIndex(Field::TAGLINE, MediaType::MOVIE);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_ID_CREDITS + 2;
  varindex = DatabaseUtils::GetFieldIndex(Field::WRITER, MediaType::MOVIE);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_ID_RUNTIME + 2;
  varindex = DatabaseUtils::GetFieldIndex(Field::TIME, MediaType::MOVIE);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_ID_MPAA + 2;
  varindex = DatabaseUtils::GetFieldIndex(Field::MPAA, MediaType::MOVIE);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_ID_TOP250 + 2;
  varindex = DatabaseUtils::GetFieldIndex(Field::TOP250, MediaType::MOVIE);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_ID_GENRE + 2;
  varindex = DatabaseUtils::GetFieldIndex(Field::GENRE, MediaType::MOVIE);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_ID_DIRECTOR + 2;
  varindex = DatabaseUtils::GetFieldIndex(Field::DIRECTOR, MediaType::MOVIE);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_ID_STUDIOS + 2;
  varindex = DatabaseUtils::GetFieldIndex(Field::STUDIO, MediaType::MOVIE);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_ID_TRAILER + 2;
  varindex = DatabaseUtils::GetFieldIndex(Field::TRAILER, MediaType::MOVIE);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_ID_COUNTRY + 2;
  varindex = DatabaseUtils::GetFieldIndex(Field::COUNTRY, MediaType::MOVIE);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_DETAILS_MOVIE_FILE + 2;
  varindex = DatabaseUtils::GetFieldIndex(Field::FILENAME, MediaType::MOVIE);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_DETAILS_MOVIE_PATH;
  varindex = DatabaseUtils::GetFieldIndex(Field::PATH, MediaType::MOVIE);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_DETAILS_MOVIE_PLAYCOUNT;
  varindex = DatabaseUtils::GetFieldIndex(Field::PLAYCOUNT, MediaType::MOVIE);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_DETAILS_MOVIE_LASTPLAYED;
  varindex = DatabaseUtils::GetFieldIndex(Field::LAST_PLAYED, MediaType::MOVIE);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_DETAILS_MOVIE_DATEADDED;
  varindex = DatabaseUtils::GetFieldIndex(Field::DATE_ADDED, MediaType::MOVIE);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_DETAILS_MOVIE_USER_RATING;
  varindex = DatabaseUtils::GetFieldIndex(Field::USER_RATING, MediaType::MOVIE);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_DETAILS_MOVIE_VOTES;
  varindex = DatabaseUtils::GetFieldIndex(Field::VOTES, MediaType::MOVIE);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_DETAILS_MOVIE_RATING;
  varindex = DatabaseUtils::GetFieldIndex(Field::RATING, MediaType::MOVIE);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_DETAILS_MOVIE_PREMIERED;
  varindex = DatabaseUtils::GetFieldIndex(Field::YEAR, MediaType::MOVIE);
  EXPECT_EQ(refindex, varindex);

  refindex = -1;
  varindex = DatabaseUtils::GetFieldIndex(Field::RANDOM, MediaType::MOVIE);
  EXPECT_EQ(refindex, varindex);
}

TEST(TestDatabaseUtils, GetFieldIndex_MediaTypeTvShow)
{
  int refindex, varindex;

  refindex = 0;
  varindex = DatabaseUtils::GetFieldIndex(Field::ID, MediaType::TV_SHOW);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_ID_TV_TITLE + 1;
  varindex = DatabaseUtils::GetFieldIndex(Field::TITLE, MediaType::TV_SHOW);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_ID_TV_SORTTITLE + 1;
  varindex = DatabaseUtils::GetFieldIndex(Field::SORT_TITLE, MediaType::TV_SHOW);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_ID_TV_PLOT + 1;
  varindex = DatabaseUtils::GetFieldIndex(Field::PLOT, MediaType::TV_SHOW);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_ID_TV_STATUS + 1;
  varindex = DatabaseUtils::GetFieldIndex(Field::TVSHOW_STATUS, MediaType::TV_SHOW);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_ID_TV_PREMIERED + 1;
  varindex = DatabaseUtils::GetFieldIndex(Field::YEAR, MediaType::TV_SHOW);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_ID_TV_GENRE + 1;
  varindex = DatabaseUtils::GetFieldIndex(Field::GENRE, MediaType::TV_SHOW);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_ID_TV_MPAA + 1;
  varindex = DatabaseUtils::GetFieldIndex(Field::MPAA, MediaType::TV_SHOW);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_ID_TV_STUDIOS + 1;
  varindex = DatabaseUtils::GetFieldIndex(Field::STUDIO, MediaType::TV_SHOW);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_DETAILS_TVSHOW_PATH;
  varindex = DatabaseUtils::GetFieldIndex(Field::PATH, MediaType::TV_SHOW);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_DETAILS_TVSHOW_DATEADDED;
  varindex = DatabaseUtils::GetFieldIndex(Field::DATE_ADDED, MediaType::TV_SHOW);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_DETAILS_TVSHOW_NUM_EPISODES;
  varindex = DatabaseUtils::GetFieldIndex(Field::NUMBER_OF_EPISODES, MediaType::TV_SHOW);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_DETAILS_TVSHOW_NUM_WATCHED;
  varindex = DatabaseUtils::GetFieldIndex(Field::NUMBER_OF_WATCHED_EPISODES, MediaType::TV_SHOW);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_DETAILS_TVSHOW_NUM_SEASONS;
  varindex = DatabaseUtils::GetFieldIndex(Field::SEASON, MediaType::TV_SHOW);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_DETAILS_TVSHOW_USER_RATING;
  varindex = DatabaseUtils::GetFieldIndex(Field::USER_RATING, MediaType::TV_SHOW);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_DETAILS_TVSHOW_VOTES;
  varindex = DatabaseUtils::GetFieldIndex(Field::VOTES, MediaType::TV_SHOW);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_DETAILS_TVSHOW_RATING;
  varindex = DatabaseUtils::GetFieldIndex(Field::RATING, MediaType::TV_SHOW);
  EXPECT_EQ(refindex, varindex);

  refindex = -1;
  varindex = DatabaseUtils::GetFieldIndex(Field::RANDOM, MediaType::TV_SHOW);
  EXPECT_EQ(refindex, varindex);
}

TEST(TestDatabaseUtils, GetFieldIndex_MediaTypeEpisode)
{
  int refindex, varindex;

  refindex = 0;
  varindex = DatabaseUtils::GetFieldIndex(Field::ID, MediaType::EPISODE);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_ID_EPISODE_TITLE + 2;
  varindex = DatabaseUtils::GetFieldIndex(Field::TITLE, MediaType::EPISODE);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_ID_EPISODE_PLOT + 2;
  varindex = DatabaseUtils::GetFieldIndex(Field::PLOT, MediaType::EPISODE);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_ID_EPISODE_CREDITS + 2;
  varindex = DatabaseUtils::GetFieldIndex(Field::WRITER, MediaType::EPISODE);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_ID_EPISODE_AIRED + 2;
  varindex = DatabaseUtils::GetFieldIndex(Field::AIR_DATE, MediaType::EPISODE);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_ID_EPISODE_RUNTIME + 2;
  varindex = DatabaseUtils::GetFieldIndex(Field::TIME, MediaType::EPISODE);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_ID_EPISODE_DIRECTOR + 2;
  varindex = DatabaseUtils::GetFieldIndex(Field::DIRECTOR, MediaType::EPISODE);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_ID_EPISODE_SEASON + 2;
  varindex = DatabaseUtils::GetFieldIndex(Field::SEASON, MediaType::EPISODE);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_ID_EPISODE_EPISODE + 2;
  varindex = DatabaseUtils::GetFieldIndex(Field::EPISODE_NUMBER, MediaType::EPISODE);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_DETAILS_EPISODE_FILE;
  varindex = DatabaseUtils::GetFieldIndex(Field::FILENAME, MediaType::EPISODE);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_DETAILS_EPISODE_PATH;
  varindex = DatabaseUtils::GetFieldIndex(Field::PATH, MediaType::EPISODE);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_DETAILS_EPISODE_PLAYCOUNT;
  varindex = DatabaseUtils::GetFieldIndex(Field::PLAYCOUNT, MediaType::EPISODE);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_DETAILS_EPISODE_LASTPLAYED;
  varindex = DatabaseUtils::GetFieldIndex(Field::LAST_PLAYED, MediaType::EPISODE);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_DETAILS_EPISODE_DATEADDED;
  varindex = DatabaseUtils::GetFieldIndex(Field::DATE_ADDED, MediaType::EPISODE);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_DETAILS_EPISODE_TVSHOW_NAME;
  varindex = DatabaseUtils::GetFieldIndex(Field::TVSHOW_TITLE, MediaType::EPISODE);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_DETAILS_EPISODE_TVSHOW_STUDIO;
  varindex = DatabaseUtils::GetFieldIndex(Field::STUDIO, MediaType::EPISODE);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_DETAILS_EPISODE_TVSHOW_AIRED;
  varindex = DatabaseUtils::GetFieldIndex(Field::YEAR, MediaType::EPISODE);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_DETAILS_EPISODE_TVSHOW_MPAA;
  varindex = DatabaseUtils::GetFieldIndex(Field::MPAA, MediaType::EPISODE);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_DETAILS_EPISODE_USER_RATING;
  varindex = DatabaseUtils::GetFieldIndex(Field::USER_RATING, MediaType::EPISODE);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_DETAILS_EPISODE_VOTES;
  varindex = DatabaseUtils::GetFieldIndex(Field::VOTES, MediaType::EPISODE);
  EXPECT_EQ(refindex, varindex);

  refindex = VIDEODB_DETAILS_EPISODE_RATING;
  varindex = DatabaseUtils::GetFieldIndex(Field::RATING, MediaType::EPISODE);
  EXPECT_EQ(refindex, varindex);

  refindex = -1;
  varindex = DatabaseUtils::GetFieldIndex(Field::RANDOM, MediaType::EPISODE);
  EXPECT_EQ(refindex, varindex);
}

TEST(TestDatabaseUtils, GetSelectFields)
{
  Fields fields;
  FieldList fieldlist;

  EXPECT_FALSE(DatabaseUtils::GetSelectFields(fields, MediaType::ALBUM, fieldlist));

  fields = {
      Field::ID, Field::GENRE, Field::ALBUM, Field::ARTIST, Field::TITLE,
  };
  EXPECT_FALSE(DatabaseUtils::GetSelectFields(fields, MediaType::NONE, fieldlist));
  EXPECT_TRUE(DatabaseUtils::GetSelectFields(fields, MediaType::ALBUM, fieldlist));
  EXPECT_FALSE(fieldlist.empty());
}

TEST(TestDatabaseUtils, GetFieldValue)
{
  CVariant v_null, v_string;
  dbiplus::field_value f_null, f_string("test");

  f_null.set_isNull();
  EXPECT_TRUE(DatabaseUtils::GetFieldValue(f_null, v_null));
  EXPECT_TRUE(v_null.isNull());

  EXPECT_TRUE(DatabaseUtils::GetFieldValue(f_string, v_string));
  EXPECT_FALSE(v_string.isNull());
  EXPECT_TRUE(v_string.isString());
}

//! @todo Need some way to test this function
// TEST(TestDatabaseUtils, GetDatabaseResults)
// {
//   static bool GetDatabaseResults(MediaType mediaType, const FieldList &fields,
//                                  const std::unique_ptr<dbiplus::Dataset> &dataset,
//                                  DatabaseResults &results);
// }

TEST(TestDatabaseUtils, BuildLimitClause)
{
  std::string a = DatabaseUtils::BuildLimitClause(100);
  EXPECT_STREQ(" LIMIT 100", a.c_str());
}

// class DatabaseUtils
// {
// public:
//
//
//   static std::string BuildLimitClause(int end, int start = 0);
// };
