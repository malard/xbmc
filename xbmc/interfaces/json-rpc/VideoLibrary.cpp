/*
 *  Copyright (C) 2016-2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "VideoLibrary.h"

#include "FileItem.h"
#include "FileItemList.h"
#include "PVROperations.h"
#include "ServiceBroker.h"
#include "Util.h"
#include "VideoLibrarySetSourceContent.h"
#include "addons/AddonManager.h"
#include "addons/Scraper.h"
#include "addons/addoninfo/AddonInfo.h"
#include "imagefiles/ImageFileURL.h"
#include "messaging/ApplicationMessenger.h"
#include "utils/SortUtils.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/Variant.h"
#include "video/VideoDatabase.h"
#include "video/VideoDbUrl.h"
#include "video/VideoLibraryQueue.h"
#include "video/geometry/ContentGeometryScanner.h"
#include "video/geometry/GeometrySettings.h"

#include <algorithm>
#include <memory>

using namespace JSONRPC;

JSONRPC_STATUS CVideoLibrary::GetMovies(const CVariant& parameterObject, CVariant& result)
{
  CVideoDatabase videodatabase;
  if (!videodatabase.Open())
    return InternalError;

  SortDescription sorting;
  ParseLimits(parameterObject, sorting.limitStart, sorting.limitEnd);
  if (!ParseSorting(parameterObject, sorting.sortBy, sorting.sortOrder, sorting.sortAttributes))
    return InvalidParams;

  CVideoDbUrl videoUrl;
  if (!videoUrl.FromString("videodb://movies/titles/"))
    return InternalError;

  static constexpr FilterField filters[] = {FilterField::Number("genreId", "genreid"),
                                            FilterField::Text("genre"),
                                            FilterField::Number("year"),
                                            FilterField::Text("actor"),
                                            FilterField::Text("director"),
                                            FilterField::Text("studio"),
                                            FilterField::Text("country"),
                                            FilterField::Number("setId", "setid"),
                                            FilterField::Text("set"),
                                            FilterField::Text("tag")};
  if (!ApplyFilter(parameterObject["filter"], filters, "movies", videoUrl))
    return InvalidParams;

  CFileItemList items;
  if (!videodatabase.GetMoviesByWhere(videoUrl.ToString(), CDatabase::Filter(), items, sorting,
                                      RequiresAdditionalDetails(MediaTypeMovie, parameterObject)))
    return InvalidParams;

  return HandleItems("movieId", "movies", items, parameterObject, result, false);
}

JSONRPC_STATUS CVideoLibrary::GetMovieDetails(const CVariant& parameterObject, CVariant& result)
{
  int id = static_cast<int>(parameterObject["movieId"].asInteger());

  CVideoDatabase videodatabase;
  if (!videodatabase.Open())
    return InternalError;

  CVideoInfoTag infos;
  //! @todo API support for video version id
  if (const JSONRPC_STATUS status = StatusFor(
          videodatabase.TryGetMovieInfo("", infos, id, -1, -1,
                                        RequiresAdditionalDetails(MediaTypeMovie, parameterObject)),
          result, Target("movieId", parameterObject["movieId"]));
      status != OK)
    return status;

  HandleFileItem("movieId", true, "movieDetails", std::make_shared<CFileItem>(infos),
                 parameterObject, parameterObject["properties"], result, false);
  return OK;
}

JSONRPC_STATUS CVideoLibrary::GetMovieSets(const CVariant& parameterObject, CVariant& result)
{
  CVideoDatabase videodatabase;
  if (!videodatabase.Open())
    return InternalError;

  CFileItemList items;
  if (!videodatabase.GetSetsNav("videodb://movies/sets/", items, VideoDbContentType::MOVIES))
    return InternalError;

  HandleFileItemList("setId", false, "sets", items, parameterObject, result);
  return OK;
}

JSONRPC_STATUS CVideoLibrary::GetMovieSetDetails(const CVariant& parameterObject, CVariant& result)
{
  int id = static_cast<int>(parameterObject["setId"].asInteger());

  CVideoDatabase videodatabase;
  if (!videodatabase.Open())
    return InternalError;

  // Get movie set details
  CVideoInfoTag infos;
  if (const JSONRPC_STATUS status = StatusFor(videodatabase.TryGetSetInfo(id, infos), result,
                                              Target("setId", parameterObject["setId"]));
      status != OK)
    return status;

  HandleFileItem("setId", false, "setDetails", std::make_shared<CFileItem>(infos), parameterObject,
                 parameterObject["properties"], result, false);

  // Get movies from the set
  CFileItemList items;
  if (!videodatabase.GetMoviesNav(
          "videodb://movies/titles/", items, -1, -1, -1, -1, -1, -1, id, -1, SortDescription(),
          RequiresAdditionalDetails(MediaTypeMovie, parameterObject["movies"])))
    return InternalError;

  return HandleItems("movieId", "movies", items, parameterObject["movies"], result["setDetails"],
                     true);
}

JSONRPC_STATUS CVideoLibrary::GetTVShows(const CVariant& parameterObject, CVariant& result)
{
  CVideoDatabase videodatabase;
  if (!videodatabase.Open())
    return InternalError;

  SortDescription sorting;
  ParseLimits(parameterObject, sorting.limitStart, sorting.limitEnd);
  if (!ParseSorting(parameterObject, sorting.sortBy, sorting.sortOrder, sorting.sortAttributes))
    return InvalidParams;

  CVideoDbUrl videoUrl;
  if (!videoUrl.FromString("videodb://tvshows/titles/"))
    return InternalError;

  static constexpr FilterField filters[] = {FilterField::Number("genreId", "genreid"),
                                            FilterField::Text("genre"),
                                            FilterField::Number("year"),
                                            FilterField::Text("actor"),
                                            FilterField::Text("studio"),
                                            FilterField::Text("tag")};
  if (!ApplyFilter(parameterObject["filter"], filters, "tvshows", videoUrl))
    return InvalidParams;

  CFileItemList items;
  if (!videodatabase.GetTvShowsByWhere(videoUrl.ToString(), CDatabase::Filter(), items, sorting,
                                       RequiresAdditionalDetails(MediaTypeTvShow, parameterObject)))
    return InvalidParams;

  return HandleItems("tvShowId", "tvShows", items, parameterObject, result, false);
}

JSONRPC_STATUS CVideoLibrary::GetTVShowDetails(const CVariant& parameterObject, CVariant& result)
{
  CVideoDatabase videodatabase;
  if (!videodatabase.Open())
    return InternalError;

  int id = static_cast<int>(parameterObject["tvShowId"].asInteger());

  CFileItemPtr fileItem(new CFileItem());
  CVideoInfoTag infos;
  if (const JSONRPC_STATUS status =
          StatusFor(videodatabase.TryGetTvShowInfo(
                        "", infos, id, fileItem.get(),
                        RequiresAdditionalDetails(MediaTypeTvShow, parameterObject)),
                    result, Target("tvShowId", parameterObject["tvShowId"]));
      status != OK)
    return status;

  fileItem->SetFromVideoInfoTag(infos);
  HandleFileItem("tvShowId", true, "tvShowDetails", fileItem, parameterObject,
                 parameterObject["properties"], result, false);
  return OK;
}

JSONRPC_STATUS CVideoLibrary::GetSeasons(const CVariant& parameterObject, CVariant& result)
{
  CVideoDatabase videodatabase;
  if (!videodatabase.Open())
    return InternalError;

  int tvshowID = static_cast<int>(parameterObject["tvShowId"].asInteger());

  std::string strPath = StringUtils::Format("videodb://tvshows/titles/{}/", tvshowID);
  CFileItemList items;
  if (!videodatabase.GetSeasonsNav(strPath, items, -1, -1, -1, -1, tvshowID, false))
    return InternalError;

  HandleFileItemList("seasonId", false, "seasons", items, parameterObject, result);
  return OK;
}

JSONRPC_STATUS CVideoLibrary::GetSeasonDetails(const CVariant& parameterObject, CVariant& result)
{
  CVideoDatabase videodatabase;
  if (!videodatabase.Open())
    return InternalError;

  int id = static_cast<int>(parameterObject["seasonId"].asInteger());

  CVideoInfoTag infos;
  if (const JSONRPC_STATUS status = StatusFor(videodatabase.TryGetSeasonInfo(id, infos), result,
                                              Target("seasonId", parameterObject["seasonId"]));
      status != OK)
    return status;
  if (infos.m_iIdShow <= 0)
    return Fail(result, Reason::NoSuchItem, Target("seasonId", parameterObject["seasonId"]));

  CFileItemPtr pItem = std::make_shared<CFileItem>(infos);
  HandleFileItem("seasonId", false, "seasonDetails", pItem, parameterObject,
                 parameterObject["properties"], result, false);
  return OK;
}

JSONRPC_STATUS CVideoLibrary::GetEpisodes(const CVariant& parameterObject, CVariant& result)
{
  CVideoDatabase videodatabase;
  if (!videodatabase.Open())
    return InternalError;

  SortDescription sorting;
  ParseLimits(parameterObject, sorting.limitStart, sorting.limitEnd);
  if (!ParseSorting(parameterObject, sorting.sortBy, sorting.sortOrder, sorting.sortAttributes))
    return InvalidParams;

  int tvshowID = static_cast<int>(parameterObject["tvShowId"].asInteger());
  int season = static_cast<int>(parameterObject["season"].asInteger());

  std::string strPath = StringUtils::Format("videodb://tvshows/titles/{}/{}/", tvshowID, season);

  CVideoDbUrl videoUrl;
  if (!videoUrl.FromString(strPath))
    return InternalError;

  static constexpr FilterField filters[] = {
      FilterField::Number("genreId", "genreid"), FilterField::Text("genre"),
      FilterField::Number("year"), FilterField::Text("actor"), FilterField::Text("director")};
  if (!ApplyFilter(parameterObject["filter"], filters, "episodes", videoUrl))
    return InvalidParams;

  if (tvshowID <= 0 && (season > 0 || videoUrl.HasOption("genreid") ||
                        videoUrl.HasOption("genre") || videoUrl.HasOption("actor")))
    return InvalidParams;

  if (tvshowID > 0)
  {
    videoUrl.AddOption("tvshowid", tvshowID);
    if (season >= 0)
      videoUrl.AddOption("season", season);
  }

  CFileItemList items;
  if (!videodatabase.GetEpisodesByWhere(
          videoUrl.ToString(), CDatabase::Filter(), items, false, sorting,
          RequiresAdditionalDetails(MediaTypeEpisode, parameterObject)))
    return InvalidParams;

  return HandleItems("episodeId", "episodes", items, parameterObject, result, false);
}

JSONRPC_STATUS CVideoLibrary::GetEpisodeDetails(const CVariant& parameterObject, CVariant& result)
{
  CVideoDatabase videodatabase;
  if (!videodatabase.Open())
    return InternalError;

  int id = static_cast<int>(parameterObject["episodeId"].asInteger());

  CVideoInfoTag infos;
  if (const JSONRPC_STATUS status = StatusFor(
          videodatabase.TryGetEpisodeInfo(
              "", infos, id, RequiresAdditionalDetails(MediaTypeEpisode, parameterObject)),
          result, Target("episodeId", parameterObject["episodeId"]));
      status != OK)
    return status;

  CFileItemPtr pItem = std::make_shared<CFileItem>(infos);
  // We need to set the correct base path to get the valid fanart
  int tvshowid = infos.m_iIdShow;
  if (tvshowid <= 0)
    tvshowid = videodatabase.GetTvShowForEpisode(id);

  std::string basePath =
      StringUtils::Format("videodb://tvshows/titles/{}/{}/{}", tvshowid, infos.m_iSeason, id);
  pItem->SetPath(basePath);

  HandleFileItem("episodeId", true, "episodeDetails", pItem, parameterObject,
                 parameterObject["properties"], result, false);
  return OK;
}

JSONRPC_STATUS CVideoLibrary::GetMusicVideos(const CVariant& parameterObject, CVariant& result)
{
  CVideoDatabase videodatabase;
  if (!videodatabase.Open())
    return InternalError;

  SortDescription sorting;
  ParseLimits(parameterObject, sorting.limitStart, sorting.limitEnd);
  if (!ParseSorting(parameterObject, sorting.sortBy, sorting.sortOrder, sorting.sortAttributes))
    return InvalidParams;

  CVideoDbUrl videoUrl;
  if (!videoUrl.FromString("videodb://musicvideos/titles/"))
    return InternalError;

  static constexpr FilterField filters[] = {
      FilterField::Text("artist"),   FilterField::Number("genreId", "genreid"),
      FilterField::Text("genre"),    FilterField::Number("year"),
      FilterField::Text("director"), FilterField::Text("studio"),
      FilterField::Text("tag")};
  if (!ApplyFilter(parameterObject["filter"], filters, "musicvideos", videoUrl))
    return InvalidParams;

  CFileItemList items;
  if (!videodatabase.GetMusicVideosByWhere(
          videoUrl.ToString(), CDatabase::Filter(), items, true, sorting,
          RequiresAdditionalDetails(MediaTypeMusicVideo, parameterObject)))
    return InternalError;

  return HandleItems("musicVideoId", "musicVideos", items, parameterObject, result, false);
}

JSONRPC_STATUS CVideoLibrary::GetMusicVideoDetails(const CVariant& parameterObject,
                                                   CVariant& result)
{
  CVideoDatabase videodatabase;
  if (!videodatabase.Open())
    return InternalError;

  int id = static_cast<int>(parameterObject["musicVideoId"].asInteger());

  CVideoInfoTag infos;
  if (const JSONRPC_STATUS status = StatusFor(
          videodatabase.TryGetMusicVideoInfo(
              "", infos, id, RequiresAdditionalDetails(MediaTypeMusicVideo, parameterObject)),
          result, Target("musicVideoId", parameterObject["musicVideoId"]));
      status != OK)
    return status;

  HandleFileItem("musicVideoId", true, "musicVideoDetails", std::make_shared<CFileItem>(infos),
                 parameterObject, parameterObject["properties"], result, false);
  return OK;
}

JSONRPC_STATUS CVideoLibrary::GetRecentlyAddedMovies(const CVariant& parameterObject,
                                                     CVariant& result)
{
  CVideoDatabase videodatabase;
  if (!videodatabase.Open())
    return InternalError;

  CFileItemList items;
  if (!videodatabase.GetRecentlyAddedMoviesNav(
          "videodb://recentlyaddedmovies/", items, 0,
          RequiresAdditionalDetails(MediaTypeMovie, parameterObject)))
    return InternalError;

  return HandleItems("movieId", "movies", items, parameterObject, result, true);
}

JSONRPC_STATUS CVideoLibrary::GetRecentlyAddedEpisodes(const CVariant& parameterObject,
                                                       CVariant& result)
{
  CVideoDatabase videodatabase;
  if (!videodatabase.Open())
    return InternalError;

  CFileItemList items;
  if (!videodatabase.GetRecentlyAddedEpisodesNav(
          "videodb://recentlyaddedepisodes/", items, 0,
          RequiresAdditionalDetails(MediaTypeEpisode, parameterObject)))
    return InternalError;

  return HandleItems("episodeId", "episodes", items, parameterObject, result, true);
}

JSONRPC_STATUS CVideoLibrary::GetRecentlyAddedMusicVideos(const CVariant& parameterObject,
                                                          CVariant& result)
{
  CVideoDatabase videodatabase;
  if (!videodatabase.Open())
    return InternalError;

  CFileItemList items;
  if (!videodatabase.GetRecentlyAddedMusicVideosNav(
          "videodb://recentlyaddedmusicvideos/", items, 0,
          RequiresAdditionalDetails(MediaTypeMusicVideo, parameterObject)))
    return InternalError;

  return HandleItems("musicVideoId", "musicVideos", items, parameterObject, result, true);
}

JSONRPC_STATUS CVideoLibrary::GetInProgressTVShows(const CVariant& parameterObject,
                                                   CVariant& result)
{
  CVideoDatabase videodatabase;
  if (!videodatabase.Open())
    return InternalError;

  CFileItemList items;
  if (!videodatabase.GetInProgressTvShowsNav(
          "videodb://inprogresstvshows/", items,
          RequiresAdditionalDetails(MediaTypeTvShow, parameterObject)))
    return InternalError;

  return HandleItems("tvShowId", "tvShows", items, parameterObject, result, false);
}

JSONRPC_STATUS CVideoLibrary::GetGenres(const CVariant& parameterObject, CVariant& result)
{
  std::string media = parameterObject["type"].asString();
  StringUtils::ToLower(media);
  VideoDbContentType idContent = VideoDbContentType::UNKNOWN;

  std::string strPath = "videodb://";
  /* select which video content to get genres from*/
  if (media == MediaTypeMovie)
  {
    idContent = VideoDbContentType::MOVIES;
    strPath += "movies";
  }
  else if (media == MediaTypeTvShow)
  {
    idContent = VideoDbContentType::TVSHOWS;
    strPath += "tvshows";
  }
  else if (media == MediaTypeMusicVideo)
  {
    idContent = VideoDbContentType::MUSICVIDEOS;
    strPath += "musicvideos";
  }
  strPath += "/genres/";

  CVideoDatabase videodatabase;
  if (!videodatabase.Open())
    return InternalError;

  CFileItemList items;
  if (!videodatabase.GetGenresNav(strPath, items, idContent))
    return InternalError;

  /* need to set strTitle in each item*/
  for (unsigned int i = 0; i < static_cast<unsigned int>(items.Size()); i++)
    items[i]->GetVideoInfoTag()->m_strTitle = items[i]->GetLabel();

  HandleFileItemList("genreId", false, "genres", items, parameterObject, result);
  return OK;
}

JSONRPC_STATUS CVideoLibrary::GetTags(const CVariant& parameterObject, CVariant& result)
{
  std::string media = parameterObject["type"].asString();
  StringUtils::ToLower(media);
  VideoDbContentType idContent = VideoDbContentType::UNKNOWN;

  std::string strPath = "videodb://";
  /* select which video content to get tags from*/
  if (media == MediaTypeMovie)
  {
    idContent = VideoDbContentType::MOVIES;
    strPath += "movies";
  }
  else if (media == MediaTypeTvShow)
  {
    idContent = VideoDbContentType::TVSHOWS;
    strPath += "tvshows";
  }
  else if (media == MediaTypeMusicVideo)
  {
    idContent = VideoDbContentType::MUSICVIDEOS;
    strPath += "musicvideos";
  }
  strPath += "/tags/";

  CVideoDatabase videodatabase;
  if (!videodatabase.Open())
    return InternalError;

  CFileItemList items;
  if (!videodatabase.GetTagsNav(strPath, items, idContent))
    return InternalError;

  /* need to set strTitle in each item*/
  for (int i = 0; i < items.Size(); i++)
    items[i]->GetVideoInfoTag()->m_strTitle = items[i]->GetLabel();

  HandleFileItemList("tagId", false, "tags", items, parameterObject, result);
  return OK;
}

namespace
{
const std::map<std::string, std::string> mediaIDTypes = {
    {"episodeId", MediaTypeEpisode},     {"tvShowId", MediaTypeTvShow},
    {"seasonId", MediaTypeSeason},       {"movieId", MediaTypeMovie},
    {"setId", MediaTypeVideoCollection}, {"musicVideoId", MediaTypeMusicVideo},
};
}

JSONRPC_STATUS CVideoLibrary::GetAvailableArtTypes(const CVariant& parameterObject,
                                                   CVariant& result)
{
  std::string mediaType;
  int mediaID = -1;
  for (const auto& mediaIDType : mediaIDTypes)
  {
    if (parameterObject["item"].isMember(mediaIDType.first))
    {
      mediaType = mediaIDType.second;
      mediaID = parameterObject["item"][mediaIDType.first].asInteger32();
      break;
    }
  }
  if (mediaID == -1)
    return InternalError;

  CVideoDatabase videodatabase;
  if (!videodatabase.Open())
    return InternalError;

  CVariant availablearttypes = CVariant(CVariant::VariantTypeArray);
  for (const auto& artType : videodatabase.GetAvailableArtTypesForItem(mediaID, mediaType))
  {
    availablearttypes.append(artType);
  }
  result = CVariant(CVariant::VariantTypeObject);
  result["availableArtTypes"] = availablearttypes;

  return OK;
}

JSONRPC_STATUS CVideoLibrary::GetAvailableArt(const CVariant& parameterObject, CVariant& result)
{
  std::string mediaType;
  int mediaID = -1;
  for (const auto& mediaIDType : mediaIDTypes)
  {
    if (parameterObject["item"].isMember(mediaIDType.first))
    {
      mediaType = mediaIDType.second;
      mediaID = parameterObject["item"][mediaIDType.first].asInteger32();
      break;
    }
  }
  if (mediaID == -1)
    return InternalError;

  std::string artType = parameterObject["artType"].asString();
  StringUtils::ToLower(artType);

  CVideoDatabase videodatabase;
  if (!videodatabase.Open())
    return InternalError;

  CVariant availableart = CVariant(CVariant::VariantTypeArray);
  for (const auto& artentry : videodatabase.GetAvailableArtForItem(mediaID, mediaType, artType))
  {
    CVariant item = CVariant(CVariant::VariantTypeObject);
    item["url"] = IMAGE_FILES::URLFromFile(artentry.m_url);
    item["artType"] = artentry.m_aspect;
    if (!artentry.m_preview.empty())
      item["previewUrl"] = IMAGE_FILES::URLFromFile(artentry.m_preview);
    availableart.append(item);
  }
  result = CVariant(CVariant::VariantTypeObject);
  result["availableArt"] = availableart;

  return OK;
}

JSONRPC_STATUS CVideoLibrary::SetMovieDetails(const CVariant& parameterObject, CVariant& result)
{
  int id = static_cast<int>(parameterObject["movieId"].asInteger());

  CVideoDatabase videodatabase;
  if (!videodatabase.Open())
    return InternalError;

  CVideoInfoTag infos;
  //! @todo API support for video version id
  if (const JSONRPC_STATUS status =
          StatusFor(videodatabase.TryGetMovieInfo("", infos, id, -1), result,
                    Target("movieId", parameterObject["movieId"]));
      status != OK)
    return status;

  const PlaybackUpdate before{infos.GetPlayCount(), infos.m_lastPlayed};

  const DetailsEdit edit = EditDetails(parameterObject, infos, videodatabase);

  if (videodatabase.UpdateDetailsForMovie(id, infos, edit.artwork, edit.updatedDetails) <= 0)
    return InternalError;

  if (!videodatabase.RemoveArtForItem(infos.m_iDbId, MediaTypeMovie, edit.removedArtwork))
    return InternalError;

  StorePlaybackEdit(parameterObject, before, infos, videodatabase);

  CJSONRPCUtils::NotifyItemUpdated(infos, edit.artwork);
  return ACK;
}

JSONRPC_STATUS CVideoLibrary::SetMovieSetDetails(const CVariant& parameterObject, CVariant& result)
{
  int id = static_cast<int>(parameterObject["setId"].asInteger());

  CVideoDatabase videodatabase;
  if (!videodatabase.Open())
    return InternalError;

  CVideoInfoTag infos;
  if (const JSONRPC_STATUS status = StatusFor(videodatabase.TryGetSetInfo(id, infos), result,
                                              Target("setId", parameterObject["setId"]));
      status != OK)
    return status;

  const DetailsEdit edit = EditDetails(parameterObject, infos, videodatabase);

  if (videodatabase.SetDetailsForMovieSet(infos, edit.artwork, id) <= 0)
    return InternalError;

  if (!videodatabase.RemoveArtForItem(infos.m_iDbId, "set", edit.removedArtwork))
    return InternalError;

  CJSONRPCUtils::NotifyItemUpdated();
  return ACK;
}

JSONRPC_STATUS CVideoLibrary::SetTVShowDetails(const CVariant& parameterObject, CVariant& result)
{
  int id = static_cast<int>(parameterObject["tvShowId"].asInteger());

  CVideoDatabase videodatabase;
  if (!videodatabase.Open())
    return InternalError;

  CVideoInfoTag infos;
  if (const JSONRPC_STATUS status = StatusFor(videodatabase.TryGetTvShowInfo("", infos, id), result,
                                              Target("tvShowId", parameterObject["tvShowId"]));
      status != OK)
    return status;

  KODI::ART::SeasonsArtwork seasonArt;
  videodatabase.GetTvShowSeasonArt(infos.m_iDbId, seasonArt);

  const DetailsEdit edit = EditDetails(parameterObject, infos, videodatabase);

  // we need to manually remove tags/taglinks for now because they aren't replaced
  // due to scrapers not supporting them
  videodatabase.RemoveTagsFromItem(id, MediaTypeTvShow);

  if (!videodatabase.UpdateDetailsForTvShow(id, infos, edit.artwork, seasonArt))
    return InternalError;

  if (!videodatabase.RemoveArtForItem(infos.m_iDbId, MediaTypeTvShow, edit.removedArtwork))
    return InternalError;

  const bool updatePlaycount = ParameterNotNull(parameterObject, "playCount");
  const bool updateLastplayed = ParameterNotNull(parameterObject, "lastPlayed");
  if (updatePlaycount || updateLastplayed)
  {
    // a tvshow has no file row of its own - its playcount is derived from its
    // episodes, so the new values have to be applied to every episode of the show
    CVideoDbUrl videoUrl;
    if (!videoUrl.FromString(StringUtils::Format("videodb://tvshows/titles/{}/-1/", id)))
      return InternalError;
    videoUrl.AddOption("tvshowid", id);

    CFileItemList episodes;
    if (!videodatabase.GetEpisodesByWhere(videoUrl.ToString(), CDatabase::Filter(), episodes,
                                          false))
      return InternalError;

    videodatabase.BeginTransaction();
    for (const auto& episode : episodes)
    {
      if (!episode->HasVideoInfoTag())
        continue;

      const auto update = EpisodePlaybackUpdate(infos, updatePlaycount, updateLastplayed,
                                                *episode->GetVideoInfoTag());
      if (update)
        videodatabase.SetPlayCount(*episode, update->playCount, update->lastPlayed);
    }
    videodatabase.CommitTransaction();
  }

  CJSONRPCUtils::NotifyItemUpdated();
  return ACK;
}

JSONRPC_STATUS CVideoLibrary::SetSeasonDetails(const CVariant& parameterObject, CVariant& result)
{
  int id = static_cast<int>(parameterObject["seasonId"].asInteger());

  CVideoDatabase videodatabase;
  if (!videodatabase.Open())
    return InternalError;

  CVideoInfoTag infos;
  if (const JSONRPC_STATUS status = StatusFor(videodatabase.TryGetSeasonInfo(id, infos), result,
                                              Target("seasonId", parameterObject["seasonId"]));
      status != OK)
    return status;
  if (infos.m_iIdShow <= 0)
    return Fail(result, Reason::NoSuchItem, Target("seasonId", parameterObject["seasonId"]));

  const DetailsEdit edit = EditDetails(parameterObject, infos, videodatabase);
  if (ParameterNotNull(parameterObject, "title"))
    infos.SetSortTitle(parameterObject["title"].asString());

  if (videodatabase.SetDetailsForSeason(infos, edit.artwork, infos.m_iIdShow, id) <= 0)
    return InternalError;

  if (!videodatabase.RemoveArtForItem(infos.m_iDbId, MediaTypeSeason, edit.removedArtwork))
    return InternalError;

  CJSONRPCUtils::NotifyItemUpdated();
  return ACK;
}

JSONRPC_STATUS CVideoLibrary::SetEpisodeDetails(const CVariant& parameterObject, CVariant& result)
{
  int id = static_cast<int>(parameterObject["episodeId"].asInteger());

  CVideoDatabase videodatabase;
  if (!videodatabase.Open())
    return InternalError;

  CVideoInfoTag infos;
  if (const JSONRPC_STATUS status =
          StatusFor(videodatabase.TryGetEpisodeInfo("", infos, id), result,
                    Target("episodeId", parameterObject["episodeId"]));
      status != OK)
    return status;

  int tvshowid = videodatabase.GetTvShowForEpisode(id);
  if (tvshowid <= 0)
    return Fail(result, Reason::NoSuchItem, Target("episodeId", parameterObject["episodeId"]));

  const PlaybackUpdate before{infos.GetPlayCount(), infos.m_lastPlayed};

  const DetailsEdit edit = EditDetails(parameterObject, infos, videodatabase);

  if (videodatabase.SetDetailsForEpisode(infos, edit.artwork, tvshowid, id) <= 0)
    return InternalError;

  if (!videodatabase.RemoveArtForItem(infos.m_iDbId, MediaTypeEpisode, edit.removedArtwork))
    return InternalError;

  StorePlaybackEdit(parameterObject, before, infos, videodatabase);

  CJSONRPCUtils::NotifyItemUpdated();
  return ACK;
}

JSONRPC_STATUS CVideoLibrary::SetMusicVideoDetails(const CVariant& parameterObject,
                                                   CVariant& result)
{
  int id = static_cast<int>(parameterObject["musicVideoId"].asInteger());

  CVideoDatabase videodatabase;
  if (!videodatabase.Open())
    return InternalError;

  CVideoInfoTag infos;
  if (const JSONRPC_STATUS status =
          StatusFor(videodatabase.TryGetMusicVideoInfo("", infos, id), result,
                    Target("musicVideoId", parameterObject["musicVideoId"]));
      status != OK)
    return status;

  const PlaybackUpdate before{infos.GetPlayCount(), infos.m_lastPlayed};

  const DetailsEdit edit = EditDetails(parameterObject, infos, videodatabase);

  // we need to manually remove tags/taglinks for now because they aren't replaced
  // due to scrapers not supporting them
  videodatabase.RemoveTagsFromItem(id, MediaTypeMusicVideo);

  if (videodatabase.SetDetailsForMusicVideo(infos, edit.artwork, id) <= 0)
    return InternalError;

  if (!videodatabase.RemoveArtForItem(infos.m_iDbId, MediaTypeMusicVideo, edit.removedArtwork))
    return InternalError;

  StorePlaybackEdit(parameterObject, before, infos, videodatabase);

  CJSONRPCUtils::NotifyItemUpdated();
  return ACK;
}

JSONRPC_STATUS CVideoLibrary::Refresh(const CVariant& parameterObject, CVariant& result)
{
  return RefreshVideo(parameterObject["item"], parameterObject, result);
}

JSONRPC_STATUS CVideoLibrary::RefreshMovie(const CVariant& parameterObject, CVariant& result)
{
  return RefreshVideo(parameterObject, parameterObject, result);
}

JSONRPC_STATUS CVideoLibrary::RefreshTVShow(const CVariant& parameterObject, CVariant& result)
{
  return RefreshVideo(parameterObject, parameterObject, result);
}

JSONRPC_STATUS CVideoLibrary::RefreshEpisode(const CVariant& parameterObject, CVariant& result)
{
  return RefreshVideo(parameterObject, parameterObject, result);
}

JSONRPC_STATUS CVideoLibrary::RefreshMusicVideo(const CVariant& parameterObject, CVariant& result)
{
  return RefreshVideo(parameterObject, parameterObject, result);
}

JSONRPC_STATUS CVideoLibrary::RefreshContentGeometry(const CVariant& parameterObject,
                                                     CVariant& result)
{
  if (!KODI::VIDEO::GEOMETRY::ContentGeometryEnabledFromSettings())
    return FailedToExecute;

  const CVariant& item = parameterObject["item"];

  CVideoDatabase videodatabase;
  if (!videodatabase.Open())
    return InternalError;

  // A path is taken as given; an id is resolved to the file behind it.
  CFileItem fileItem;
  if (item.isMember("file"))
  {
    fileItem.SetPath(item["file"].asString());
  }
  else
  {
    CVideoInfoTag infos;
    bool found = false;
    std::string key = "musicVideoId";
    if (item.isMember("movieId"))
    {
      key = "movieId";
      found =
          videodatabase.GetMovieInfo("", infos, static_cast<int>(item["movieId"].asInteger()), -1);
    }
    else if (item.isMember("episodeId"))
    {
      key = "episodeId";
      found =
          videodatabase.GetEpisodeInfo("", infos, static_cast<int>(item["episodeId"].asInteger()));
    }
    else
      found = videodatabase.GetMusicVideoInfo("", infos,
                                              static_cast<int>(item["musicVideoId"].asInteger()));

    if (!found || infos.m_iDbId <= 0)
      return Fail(result, Reason::NoSuchItem, Target(key, item[key]));

    fileItem.SetFromVideoInfoTag(infos);
  }

  const KODI::VIDEO::GEOMETRY::SamplingDepth depth{
      parameterObject["thorough"].asBoolean(false) ? KODI::VIDEO::GEOMETRY::SamplingDepth::Thorough
                                                   : KODI::VIDEO::GEOMETRY::SamplingDepth::Normal};

  if (!KODI::VIDEO::GEOMETRY::RemeasureContentGeometry(fileItem, depth))
    return Unavailable;

  return ACK;
}

JSONRPC_STATUS CVideoLibrary::RemoveMovie(const CVariant& parameterObject, CVariant& result)
{
  return RemoveVideo(parameterObject);
}

JSONRPC_STATUS CVideoLibrary::RemoveTVShow(const CVariant& parameterObject, CVariant& result)
{
  return RemoveVideo(parameterObject);
}

JSONRPC_STATUS CVideoLibrary::RemoveEpisode(const CVariant& parameterObject, CVariant& result)
{
  return RemoveVideo(parameterObject);
}

JSONRPC_STATUS CVideoLibrary::RemoveMusicVideo(const CVariant& parameterObject, CVariant& result)
{
  return RemoveVideo(parameterObject);
}

JSONRPC_STATUS CVideoLibrary::Scan(const CVariant& parameterObject, CVariant& result)
{
  std::string directory = parameterObject["directory"].asString();
  if (!directory.empty())
  {
    // a directory outside every video source could never be found by the scan
    directory = CVideoDatabase::ToStoredPath(directory);

    CVideoDatabase videodatabase;
    if (!videodatabase.Open())
      return InternalError;

    std::string sourcePath;
    if (!videodatabase.GetSourcePath(directory, sourcePath))
      return Fail(result, Reason::NoSuchSource, Target("directory", parameterObject["directory"]));
  }

  std::string cmd =
      StringUtils::Format("updatelibrary(video, {}, {})", StringUtils::Paramify(directory),
                          parameterObject["showDialogs"].asBoolean() ? "true" : "false");

  CServiceBroker::GetAppMessenger()->SendMsg(TMSG_EXECUTE_BUILT_IN, -1, -1, nullptr, cmd);
  return ACK;
}

JSONRPC_STATUS CVideoLibrary::SetSourceContent(const CVariant& parameterObject, CVariant& result)
{
  ParsedSetSourceContent parsed;
  const JSONRPC_STATUS status = ParseSetSourceContentParams(parameterObject, parsed);
  if (status != OK)
  {
    return status;
  }

  // SetScraperForPath() stores the path verbatim, GetScraperForPath() reads it back as a
  // directory, so an unterminated path would never be found again.
  if (!URIUtils::IsMultiPath(parsed.path))
  {
    URIUtils::AddSlashAtEnd(parsed.path);
  }

  CVideoDatabase videodatabase;
  if (!videodatabase.Open())
  {
    return InternalError;
  }

  // m_allExtAudio has no parameter, but SetScraperForPath() writes it in every branch, so
  // the path's stored value has to be carried across.
  KODI::VIDEO::SScanSettings existing;
  videodatabase.GetScraperForPath(parsed.path, existing);
  parsed.settings.m_allExtAudio = existing.m_allExtAudio;

  ADDON::ScraperPtr scraper;
  if (parsed.content != ADDON::ContentType::NONE)
  {
    // Looked up by type: a scraper serving more than one content type has an instance per
    // type, and the binding is stored with the instance's own content.
    ADDON::AddonPtr addon;
    ADDON::CAddonMgr& addonMgr = CServiceBroker::GetAddonMgr();
    if (!addonMgr.GetAddon(parsed.scraperId, addon, ADDON::ScraperTypeFromContent(parsed.content),
                           ADDON::OnlyEnabled::CHOICE_YES))
    {
      if (!addonMgr.GetAddon(parsed.scraperId, addon, ADDON::OnlyEnabled::CHOICE_YES))
      {
        return Fail(result, Reason::NoSuchAddon, Target("scraperId", parameterObject["scraperId"]));
      }
      return InvalidParams;
    }

    scraper = std::dynamic_pointer_cast<ADDON::CScraper>(addon);
    if (!scraper)
    {
      return InvalidParams;
    }

    // Without supplied XML a failure is the scraper's own defaults, not the caller's doing.
    if (!scraper->SetPathSettings(parsed.content, parsed.scraperSettings) &&
        !parsed.scraperSettings.empty())
    {
      return InvalidParams;
    }
  }
  else if (parsed.clearMode == SourceContentClearMode::REMOVE)
  {
    videodatabase.RemoveContentForPath(parsed.path);
  }

  videodatabase.SetScraperForPath(parsed.path, scraper, parsed.settings);

  CUtil::DeleteVideoDatabaseDirectoryCache();

  if (parsed.refresh)
  {
    CVideoLibraryQueue::GetInstance().ScanLibrary(parsed.path, true, true);
  }

  return ACK;
}

JSONRPC_STATUS CVideoLibrary::Export(const CVariant& parameterObject, CVariant& result)
{
  std::string cmd;
  if (parameterObject["options"].isMember("path"))
    cmd = StringUtils::Format("exportlibrary2(video, singlefile, {})",
                              StringUtils::Paramify(parameterObject["options"]["path"].asString()));
  else
  {
    cmd = "exportlibrary2(video, separate, dummy";
    if (parameterObject["options"]["images"].isBoolean() &&
        parameterObject["options"]["images"].asBoolean() == true)
      cmd += ", artwork";
    if (parameterObject["options"]["overwrite"].isBoolean() &&
        parameterObject["options"]["overwrite"].asBoolean() == true)
      cmd += ", overwrite";
    if (parameterObject["options"]["actorThumbs"].isBoolean() &&
        parameterObject["options"]["actorThumbs"].asBoolean() == true)
      cmd += ", actorthumbs";
    cmd += ")";
  }

  CServiceBroker::GetAppMessenger()->SendMsg(TMSG_EXECUTE_BUILT_IN, -1, -1, nullptr, cmd);
  return ACK;
}

JSONRPC_STATUS CVideoLibrary::Clean(const CVariant& parameterObject, CVariant& result)
{
  std::string directory = parameterObject["directory"].asString();
  if (!directory.empty())
  {
    // a directory that resolves to no library path leaves the clean nothing to touch
    const std::string contentParam = parameterObject["content"].asString();

    CVideoDatabase videodatabase;
    if (!videodatabase.Open())
      return InternalError;

    std::set<int> paths;
    if (!videodatabase.GetPathsForCleaning(directory, contentParam == "video" ? "" : contentParam,
                                           paths))
      return InternalError;
    if (paths.empty())
      return Fail(result, Reason::NotInLibrary, Target("directory", parameterObject["directory"]));
  }

  std::string cmd;
  if (parameterObject["content"].empty())
    cmd = StringUtils::Format("cleanlibrary(video, {0}, {1})",
                              parameterObject["showDialogs"].asBoolean() ? "true" : "false",
                              StringUtils::Paramify(directory));
  else
    cmd = StringUtils::Format("cleanlibrary({0}, {1}, {2})", parameterObject["content"].asString(),
                              parameterObject["showDialogs"].asBoolean() ? "true" : "false",
                              StringUtils::Paramify(directory));

  CServiceBroker::GetAppMessenger()->SendMsg(TMSG_EXECUTE_BUILT_IN, -1, -1, nullptr, cmd);
  return ACK;
}

std::optional<CVideoLibrary::PlaybackUpdate> CVideoLibrary::EpisodePlaybackUpdate(
    const CVideoInfoTag& show,
    bool updatePlaycount,
    bool updateLastplayed,
    const CVideoInfoTag& episode)
{
  const int count = updatePlaycount ? show.GetPlayCount() : episode.GetPlayCount();
  if (!updateLastplayed && count == episode.GetPlayCount())
    return std::nullopt;

  return PlaybackUpdate{count, updateLastplayed ? show.m_lastPlayed : episode.m_lastPlayed};
}

void CVideoLibrary::ApplyPlaybackState(const CVideoInfoTag& fileDetails, CVideoInfoTag& details)
{
  details.m_iFileId = fileDetails.m_iFileId;
  details.SetPlayCount(std::max(details.GetPlayCount(), fileDetails.GetPlayCount()));
  if (!details.m_lastPlayed.IsValid())
    details.m_lastPlayed = fileDetails.m_lastPlayed;
  if (!details.m_dateAdded.IsValid())
    details.m_dateAdded = fileDetails.m_dateAdded;
  if (!details.GetResumePoint().IsSet())
    details.SetResumePoint(fileDetails.GetResumePoint());
  if (!details.m_streamDetails.HasItems())
    details.m_streamDetails = fileDetails.m_streamDetails;
}

bool CVideoLibrary::FillFileItem(
    const std::string& strFilename,
    std::shared_ptr<CFileItem>& item,
    const CVariant& parameterObject /* = CVariant(CVariant::VariantTypeArray) */)
{
  CVideoDatabase videodatabase;
  if (strFilename.empty())
    return false;

  bool filled = false;
  if (videodatabase.Open())
  {
    // Only a library row describes the item; the files table knows anything ever played.
    // A tv show is keyed on its folder, so it is only asked about for a folder entry.
    CVideoInfoTag details;
    if (videodatabase.GetMovieInfo(strFilename, details) ||
        videodatabase.GetEpisodeInfo(strFilename, details) ||
        videodatabase.GetMusicVideoInfo(strFilename, details) ||
        (item->IsFolder() && videodatabase.GetTvShowInfo(strFilename, details, -1, item.get())))
    {
      item->SetFromVideoInfoTag(details);
      item->SetDynPath(strFilename);
      filled = true;
    }
    else
    {
      // Not a library item: add the files table's playback state to what the entry already says.
      CVideoInfoTag fileDetails;
      if (videodatabase.GetFileInfo(strFilename, fileDetails))
      {
        ApplyPlaybackState(fileDetails, *item->GetVideoInfoTag());
        if (item->GetPath().empty())
          item->SetPath(strFilename);
        filled = true;
      }
    }
  }

  if (item->GetLabel().empty())
  {
    item->SetLabel(CUtil::GetTitleFromPath(strFilename, false));
    if (item->GetLabel().empty())
      item->SetLabel(URIUtils::GetFileName(strFilename));
  }

  return filled;
}

bool CVideoLibrary::FillFileItemList(const CVariant& parameterObject, CFileItemList& list)
{
  CVideoDatabase videodatabase;
  if (!videodatabase.Open())
    return false;

  std::string file = parameterObject["file"].asString();
  int movieID = static_cast<int>(parameterObject["movieId"].asInteger(-1));
  int episodeID = static_cast<int>(parameterObject["episodeId"].asInteger(-1));
  int musicVideoID = static_cast<int>(parameterObject["musicVideoId"].asInteger(-1));
  int recordingID = static_cast<int>(parameterObject["recordingId"].asInteger());

  bool success = false;
  CFileItemPtr fileItem(new CFileItem());
  if (FillFileItem(file, fileItem))
  {
    success = true;
    list.Add(fileItem);
  }

  if (movieID > 0)
  {
    CVideoInfoTag details;
    videodatabase.GetMovieInfo("", details, movieID, -1); //! @todo API support for video version id
    if (!details.IsEmpty())
    {
      list.Add(std::make_shared<CFileItem>(details));
      success = true;
    }
  }
  if (episodeID > 0)
  {
    CVideoInfoTag details;
    if (videodatabase.GetEpisodeInfo("", details, episodeID) && !details.IsEmpty())
    {
      list.Add(std::make_shared<CFileItem>(details));
      success = true;
    }
  }
  if (musicVideoID > 0)
  {
    CVideoInfoTag details;
    videodatabase.GetMusicVideoInfo("", details, musicVideoID);
    if (!details.IsEmpty())
    {
      list.Add(std::make_shared<CFileItem>(details));
      success = true;
    }
  }
  if (recordingID > 0)
  {
    std::shared_ptr<CFileItem> recordingFileItem =
        CPVROperations::GetRecordingFileItem(recordingID);

    if (recordingFileItem)
    {
      list.Add(recordingFileItem);
      success = true;
    }
  }

  return success;
}

int CVideoLibrary::RequiresAdditionalDetails(const MediaType& mediaType,
                                             const CVariant& parameterObject)
{
  if (mediaType != MediaTypeMovie && mediaType != MediaTypeTvShow &&
      mediaType != MediaTypeEpisode && mediaType != MediaTypeMusicVideo)
    return VideoDbDetailsNone;

  return GetDetailsFromJsonParameters(parameterObject);
}

int CVideoLibrary::GetDetailsFromJsonParameters(const CVariant& parameterObject)
{
  const CVariant& properties = parameterObject["properties"];
  int details = VideoDbDetailsNone;
  for (CVariant::const_iterator_array itr = properties.begin_array(); itr != properties.end_array();
       ++itr)
  {
    std::string propertyValue = itr->asString();
    if (propertyValue == "cast")
      details = details | VideoDbDetailsCast;
    else if (propertyValue == "ratings")
      details = details | VideoDbDetailsRating;
    else if (propertyValue == "uniqueId")
      details = details | VideoDbDetailsUniqueID;
    else if (propertyValue == "showLink")
      details = details | VideoDbDetailsShowLink;
    else if (propertyValue == "streamDetails")
      details = details | VideoDbDetailsStream;
    else if (propertyValue == "tag")
      details = details | VideoDbDetailsTag;
  }
  return details;
}

JSONRPC_STATUS CVideoLibrary::HandleItems(const char* idProperty,
                                          const char* resultName,
                                          CFileItemList& items,
                                          const CVariant& parameterObject,
                                          CVariant& result,
                                          bool limit /* = true */)
{
  int size = items.Size();
  if (!limit && items.HasProperty("total") && items.GetProperty("total").asInteger() > size)
    size = static_cast<int>(items.GetProperty("total").asInteger());
  HandleFileItemList(idProperty, true, resultName, items, parameterObject, result, size, limit);

  return OK;
}

JSONRPC_STATUS CVideoLibrary::RemoveVideo(const CVariant& parameterObject)
{
  CVideoDatabase videodatabase;
  if (!videodatabase.Open())
    return InternalError;

  if (parameterObject.isMember("movieId"))
  {
    if (!videodatabase.DeleteMovie(static_cast<int>(parameterObject["movieId"].asInteger())))
      return InternalError;
  }
  else if (parameterObject.isMember("tvShowId"))
    videodatabase.DeleteTvShow(static_cast<int>(parameterObject["tvShowId"].asInteger()));
  else if (parameterObject.isMember("episodeId"))
    videodatabase.DeleteEpisode(static_cast<int>(parameterObject["episodeId"].asInteger()));
  else if (parameterObject.isMember("musicVideoId"))
    videodatabase.DeleteMusicVideo(static_cast<int>(parameterObject["musicVideoId"].asInteger()));

  CJSONRPCUtils::NotifyItemUpdated();
  return ACK;
}

JSONRPC_STATUS CVideoLibrary::RefreshVideo(const CVariant& identifier,
                                           const CVariant& parameterObject,
                                           CVariant& result)
{
  CVideoDatabase videodatabase;
  if (!videodatabase.Open())
  {
    return InternalError;
  }

  const std::shared_ptr<CFileItem> item = std::make_shared<CFileItem>();
  const JSONRPC_STATUS status = ResolveRefreshItem(identifier, videodatabase, *item, result);
  if (status != OK)
  {
    return status;
  }

  const bool ignoreNfo = parameterObject["ignoreNfo"].asBoolean();
  const bool refreshEpisodes = parameterObject["refreshEpisodes"].asBoolean();
  const std::string searchTitle = parameterObject["title"].asString();
  CVideoLibraryQueue::GetInstance().RefreshItem(item, ignoreNfo, true, refreshEpisodes,
                                                searchTitle);

  return ACK;
}

JSONRPC_STATUS CVideoLibrary::ResolveRefreshItem(const CVariant& identifier,
                                                 CVideoDatabase& videodatabase,
                                                 CFileItem& item,
                                                 CVariant& result)
{
  CVideoInfoTag details;

  if (identifier.isMember("setId"))
  {
    // GetSetInfo fills the item with the set's videodb:// path, which the tag cannot carry.
    return StatusFor(videodatabase.TryGetSetInfo(static_cast<int>(identifier["setId"].asInteger()),
                                                 details, &item),
                     result, Target("setId", identifier["setId"]));
  }

  CDatabase::GetResult lookup;
  std::string key;
  if (identifier.isMember("movieId"))
  {
    key = "movieId";
    //! @todo API support for video version id
    lookup = videodatabase.TryGetMovieInfo("", details,
                                           static_cast<int>(identifier["movieId"].asInteger()), -1);
  }
  else if (identifier.isMember("tvShowId"))
  {
    key = "tvShowId";
    lookup = videodatabase.TryGetTvShowInfo(
        "", details, static_cast<int>(identifier["tvShowId"].asInteger()), &item);
  }
  else if (identifier.isMember("seasonId"))
  {
    key = "seasonId";
    lookup = videodatabase.TryGetSeasonInfo(static_cast<int>(identifier["seasonId"].asInteger()),
                                            details, &item);
  }
  else if (identifier.isMember("episodeId"))
  {
    key = "episodeId";
    lookup = videodatabase.TryGetEpisodeInfo("", details,
                                             static_cast<int>(identifier["episodeId"].asInteger()));
  }
  else if (identifier.isMember("musicVideoId"))
  {
    key = "musicVideoId";
    lookup = videodatabase.TryGetMusicVideoInfo(
        "", details, static_cast<int>(identifier["musicVideoId"].asInteger()));
  }
  else
  {
    return InvalidParams;
  }

  if (const JSONRPC_STATUS status = StatusFor(lookup, result, Target(key, identifier[key]));
      status != OK)
    return status;

  item.SetFromVideoInfoTag(details);

  return OK;
}

CVideoLibrary::DetailsEdit CVideoLibrary::EditDetails(const CVariant& parameterObject,
                                                      CVideoInfoTag& details,
                                                      CVideoDatabase& videodatabase)
{
  DetailsEdit edit;
  videodatabase.GetArtForItem(details.m_iDbId, details.m_type, edit.artwork);
  UpdateVideoTag(parameterObject, details, edit.artwork, edit.removedArtwork, edit.updatedDetails);
  return edit;
}

void CVideoLibrary::StorePlaybackEdit(const CVariant& parameterObject,
                                      const PlaybackUpdate& before,
                                      CVideoInfoTag& details,
                                      CVideoDatabase& videodatabase)
{
  if (before.playCount != details.GetPlayCount() || before.lastPlayed != details.m_lastPlayed)
  {
    // restore the original playcount, or the new one won't be announced
    const int playCount = details.GetPlayCount();
    details.SetPlayCount(before.playCount);
    videodatabase.SetPlayCount(CFileItem(details), playCount, details.m_lastPlayed);
  }

  UpdateResumePoint(parameterObject, details, videodatabase);
}

void CVideoLibrary::UpdateResumePoint(const CVariant& parameterObject,
                                      CVideoInfoTag& details,
                                      CVideoDatabase& videodatabase)
{
  if (!parameterObject["resume"].isNull())
  {
    double position = parameterObject["resume"]["position"].asDouble();
    if (position == 0.0)
      videodatabase.ClearBookMarksOfFile(details.m_strFileNameAndPath, CBookmark::RESUME);
    else
    {
      CBookmark bookmark;
      double total = parameterObject["resume"]["total"].asDouble();
      if (total <= 0.0 && !videodatabase.GetResumeBookMark(details.m_strFileNameAndPath, bookmark))
        bookmark.totalTimeInSeconds = details.m_streamDetails.GetVideoDuration();
      else
        bookmark.totalTimeInSeconds = total;

      bookmark.timeInSeconds = position;
      videodatabase.AddBookMarkToFile(details.m_strFileNameAndPath, bookmark, CBookmark::RESUME);
    }
  }
}

void CVideoLibrary::UpdateVideoTagField(const CVariant& parameterObject,
                                        const std::string& fieldName,
                                        std::vector<std::string>& fieldValue,
                                        std::set<std::string, std::less<>>& updatedDetails)
{
  if (ParameterNotNull(parameterObject, fieldName))
  {
    CopyStringArray(parameterObject[fieldName], fieldValue);
    updatedDetails.insert(fieldName);
  }
}

void CVideoLibrary::UpdateVideoTag(const CVariant& parameterObject,
                                   CVideoInfoTag& details,
                                   KODI::ART::Artwork& artwork,
                                   std::set<std::string, std::less<>>& removedArtwork,
                                   std::set<std::string, std::less<>>& updatedDetails)
{
  if (ParameterNotNull(parameterObject, "title"))
    details.SetTitle(parameterObject["title"].asString());
  if (ParameterNotNull(parameterObject, "playCount"))
    details.SetPlayCount(static_cast<int>(parameterObject["playCount"].asInteger()));
  if (ParameterNotNull(parameterObject, "runtime"))
    details.SetDuration(static_cast<int>(parameterObject["runtime"].asInteger()));

  std::vector<std::string> director(details.m_director);
  UpdateVideoTagField(parameterObject, "director", director, updatedDetails);
  details.SetDirector(director);

  std::vector<std::string> studio(details.m_studio);
  UpdateVideoTagField(parameterObject, "studio", studio, updatedDetails);
  details.SetStudio(studio);

  if (ParameterNotNull(parameterObject, "plot"))
    details.SetPlot(parameterObject["plot"].asString());
  if (ParameterNotNull(parameterObject, "album"))
    details.SetAlbum(parameterObject["album"].asString());

  std::vector<std::string> artist(details.m_artist);
  UpdateVideoTagField(parameterObject, "artist", artist, updatedDetails);
  details.SetArtist(artist);

  std::vector<std::string> genre(details.m_genre);
  UpdateVideoTagField(parameterObject, "genre", genre, updatedDetails);
  details.SetGenre(genre);

  CopyIfGiven(parameterObject, "track", details.m_iTrack);
  if (ParameterNotNull(parameterObject, "rating"))
  {
    details.SetRating(parameterObject["rating"].asFloat());
    updatedDetails.insert("ratings");
  }
  if (ParameterNotNull(parameterObject, "votes"))
  {
    details.SetVotes(StringUtils::ReturnDigits(parameterObject["votes"].asString()));
    updatedDetails.insert(
        "ratings"); //Votes and ratings both need updates now, this will trigger those
  }
  if (ParameterNotNull(parameterObject, "ratings"))
  {
    CVariant ratings = parameterObject["ratings"];
    for (CVariant::const_iterator_map rIt = ratings.begin_map(); rIt != ratings.end_map(); ++rIt)
    {
      if (rIt->second.isObject() && ParameterNotNull(rIt->second, "rating"))
      {
        const auto& rating = rIt->second;
        if (ParameterNotNull(rating, "votes"))
        {
          details.SetRating(rating["rating"].asFloat(),
                            static_cast<int>(rating["votes"].asInteger()), rIt->first,
                            (ParameterNotNull(rating, "default") && rating["default"].asBoolean()));
        }
        else
          details.SetRating(rating["rating"].asFloat(), rIt->first,
                            (ParameterNotNull(rating, "default") && rating["default"].asBoolean()));

        updatedDetails.insert("ratings");
      }
      else if (rIt->second.isNull())
      {
        details.RemoveRating(rIt->first);
        updatedDetails.insert("ratings");
      }
    }
  }
  CopyIfGiven(parameterObject, "userRating", details.m_iUserRating);
  if (ParameterNotNull(parameterObject, "mpaa"))
    details.SetMPAARating(parameterObject["mpaa"].asString());
  if (ParameterNotNull(parameterObject, "imdbNumber"))
  {
    details.SetUniqueID(parameterObject["imdbNumber"].asString());
    updatedDetails.insert("uniqueId");
  }
  if (ParameterNotNull(parameterObject, "uniqueId"))
  {
    CVariant uniqueids = parameterObject["uniqueId"];
    for (CVariant::const_iterator_map idIt = uniqueids.begin_map(); idIt != uniqueids.end_map();
         ++idIt)
    {
      if (idIt->second.isString() && !idIt->second.asString().empty())
      {
        details.SetUniqueID(idIt->second.asString(), idIt->first);
        updatedDetails.insert("uniqueId");
      }
      else if (idIt->second.isNull() && idIt->first != details.GetDefaultUniqueID())
      {
        details.RemoveUniqueID(idIt->first);
        updatedDetails.insert("uniqueId");
      }
    }
  }
  if (ParameterNotNull(parameterObject, "premiered"))
  {
    CDateTime premiered;
    SetFromDBDate(parameterObject["premiered"], premiered);
    details.SetPremiered(premiered);
  }
  else if (ParameterNotNull(parameterObject, "year"))
    details.SetYear(static_cast<int>(parameterObject["year"].asInteger()));
  if (ParameterNotNull(parameterObject, "lastPlayed"))
    SetFromDBDateTime(parameterObject["lastPlayed"], details.m_lastPlayed);
  if (ParameterNotNull(parameterObject, "firstAired"))
    SetFromDBDate(parameterObject["firstAired"], details.m_firstAired);
  if (ParameterNotNull(parameterObject, "productionCode"))
    details.SetProductionCode(parameterObject["productionCode"].asString());
  CopyIfGiven(parameterObject, "season", details.m_iSeason);
  CopyIfGiven(parameterObject, "episode", details.m_iEpisode);
  if (ParameterNotNull(parameterObject, "originalTitle"))
    details.SetOriginalTitle(parameterObject["originalTitle"].asString());
  if (ParameterNotNull(parameterObject, "trailer"))
    details.SetTrailer(parameterObject["trailer"].asString());
  if (ParameterNotNull(parameterObject, "tagline"))
    details.SetTagLine(parameterObject["tagline"].asString());
  if (ParameterNotNull(parameterObject, "status"))
    details.SetStatus(parameterObject["status"].asString());
  if (ParameterNotNull(parameterObject, "plotOutline"))
    details.SetPlotOutline(parameterObject["plotOutline"].asString());

  std::vector<std::string> credits(details.m_writingCredits);
  UpdateVideoTagField(parameterObject, "writer", credits, updatedDetails);
  details.SetWritingCredits(credits);

  std::vector<std::string> country(details.m_country);
  UpdateVideoTagField(parameterObject, "country", country, updatedDetails);
  details.SetCountry(country);

  CopyIfGiven(parameterObject, "top250", details.m_iTop250);
  if (ParameterNotNull(parameterObject, "sortTitle"))
    details.SetSortTitle(parameterObject["sortTitle"].asString());
  if (ParameterNotNull(parameterObject, "episodeGuide"))
    details.SetEpisodeGuide(parameterObject["episodeGuide"].asString());
  if (ParameterNotNull(parameterObject, "set"))
  {
    details.SetSet(parameterObject["set"].asString());
    updatedDetails.insert("set");
  }

  std::vector<std::string> showLink(details.m_showLink);
  UpdateVideoTagField(parameterObject, "showLink", showLink, updatedDetails);
  details.SetShowLink(showLink);

  std::vector<std::string> tags(details.m_tags);
  UpdateVideoTagField(parameterObject, "tag", tags, updatedDetails);
  details.SetTags(tags);

  if (ParameterNotNull(parameterObject, "thumbnail"))
  {
    std::string value = parameterObject["thumbnail"].asString();
    artwork["thumb"] = StringUtils::Trim(value);
    updatedDetails.insert("art.altered");
  }
  if (ParameterNotNull(parameterObject, "fanart"))
  {
    std::string value = parameterObject["fanart"].asString();
    artwork["fanart"] = StringUtils::Trim(value);
    updatedDetails.insert("art.altered");
  }

  if (ParameterNotNull(parameterObject, "art"))
  {
    CVariant art = parameterObject["art"];
    for (CVariant::const_iterator_map artIt = art.begin_map(); artIt != art.end_map(); ++artIt)
    {
      if (artIt->second.isString() && !artIt->second.asString().empty())
      {
        artwork[artIt->first] = IMAGE_FILES::ToCacheKey(artIt->second.asString());
        updatedDetails.insert("art.altered");
      }
      else if (artIt->second.isNull())
      {
        artwork.erase(artIt->first);
        removedArtwork.insert(artIt->first);
      }
    }
  }

  if (ParameterNotNull(parameterObject, "dateAdded"))
  {
    SetFromDBDateTime(parameterObject["dateAdded"], details.m_dateAdded);
    updatedDetails.insert("dateAdded");
  }
}
