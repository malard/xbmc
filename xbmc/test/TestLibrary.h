/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "FileItem.h"
#include "dbwrappers/test/DatabaseTestUtils.h"
#include "music/MusicDatabase.h"
#include "utils/Artwork.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "video/VideoDatabase.h"
#include "video/VideoInfoTag.h"

#include <string>

#include <gtest/gtest.h>

/*!
 * \brief A music and a video library of their own for a test, which code that opens the
 * libraries itself then reads.
 */
class CTestLibrary
{
public:
  explicit CTestLibrary(const std::string& name)
    : m_music(&CAdvancedSettings::m_databaseMusic, name + "Music"),
      m_video(&CAdvancedSettings::m_databaseVideo, name + "Video")
  {
  }

  CMusicDatabase& Music() { return m_music.Get(); }
  CVideoDatabase& Video() { return m_video.Get(); }

  int AddMovie(const std::string& title)
  {
    CVideoInfoTag tag{Tag("/videos/movies/" + title + ".mkv", title)};
    return Video().SetDetailsForMovie(tag, KODI::ART::Artwork{});
  }

  int AddTvShow(const std::string& title)
  {
    CVideoInfoTag tag;
    tag.m_strTitle = title;
    tag.m_strPath = ShowPath(title);
    return Video().SetDetailsForTvShow({tag.m_strPath}, tag, KODI::ART::Artwork{},
                                       KODI::ART::SeasonsArtwork{});
  }

  int AddEpisode(int idShow, const std::string& show, const std::string& title, int episode)
  {
    CVideoInfoTag tag{Tag(ShowPath(show) + title + ".mkv", title)};
    tag.m_iSeason = 1;
    tag.m_iEpisode = episode;
    return Video().SetDetailsForEpisode(tag, KODI::ART::Artwork{}, idShow);
  }

  int AddMusicVideo(const std::string& title, const std::string& artist)
  {
    CVideoInfoTag tag{Tag("/videos/clips/" + title + ".mkv", title)};
    tag.m_artist = {artist};
    return Video().SetDetailsForMusicVideo(tag, KODI::ART::Artwork{});
  }

  //! \return the song's id; its album and artist are added when the library lacks them
  int AddSong(const std::string& title, const std::string& album, const std::string& artist)
  {
    CMusicDatabase& db{Music()};
    const int idArtist{db.AddArtist(artist, "")};
    int idAlbum{
        Value(db.PrepareSQL("SELECT idAlbum FROM album WHERE strAlbum = '%s'", album.c_str()))};
    if (idAlbum <= 0)
    {
      EXPECT_TRUE(db.ExecuteQuery(
          db.PrepareSQL("INSERT INTO album (strAlbum, strArtistDisp, strReleaseType) "
                        "VALUES ('%s', '%s', 'album')",
                        album.c_str(), artist.c_str())));
      idAlbum = Value("SELECT MAX(idAlbum) FROM album");
      EXPECT_TRUE(db.ExecuteQuery(
          db.PrepareSQL("INSERT INTO album_artist (idArtist, idAlbum, iOrder, strArtist) "
                        "VALUES (%i, %i, 0, '%s')",
                        idArtist, idAlbum, artist.c_str())));
    }
    int idPath{Value("SELECT idPath FROM path WHERE strPath = '/music/'")};
    if (idPath <= 0)
    {
      EXPECT_TRUE(db.ExecuteQuery("INSERT INTO path (strPath) VALUES ('/music/')"));
      idPath = Value("SELECT MAX(idPath) FROM path");
    }
    EXPECT_TRUE(db.ExecuteQuery(db.PrepareSQL(
        "INSERT INTO song (idAlbum, idPath, strFileName, iTrack, strTitle, strArtistDisp) "
        "VALUES (%i, %i, '%s.flac', 1, '%s', '%s')",
        idAlbum, idPath, title.c_str(), title.c_str(), artist.c_str())));
    const int idSong{Value("SELECT MAX(idSong) FROM song")};
    EXPECT_TRUE(db.ExecuteQuery(
        db.PrepareSQL("INSERT INTO song_artist (idArtist, idSong, idRole, iOrder, strArtist) "
                      "VALUES (%i, %i, 1, 0, '%s')",
                      idArtist, idSong, artist.c_str())));
    return idSong;
  }

private:
  static std::string ShowPath(const std::string& show) { return "/videos/tv/" + show + "/"; }

  CVideoInfoTag Tag(const std::string& fileAndPath, const std::string& title)
  {
    CVideoInfoTag tag;
    tag.m_strTitle = title;
    tag.m_strFileNameAndPath = fileAndPath;
    tag.m_strPath = URIUtils::GetDirectory(fileAndPath);
    tag.m_basePath = CFileItem(fileAndPath, false).GetBaseMoviePath(false);
    tag.m_parentPathID = Video().AddPath(URIUtils::GetParentPath(tag.m_basePath));
    return tag;
  }

  int Value(const std::string& sql)
  {
    const std::string value{Music().GetSingleValue(sql)};
    return value.empty() ? -1 : std::stoi(value);
  }

  CScopedTestDatabase<CMusicDatabase> m_music;
  CScopedTestDatabase<CVideoDatabase> m_video;
};
