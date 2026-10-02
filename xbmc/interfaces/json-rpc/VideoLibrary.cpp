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
#include "video/VideoDbPaths.h"
#include "video/VideoDbUrl.h"
#include "video/VideoLibraryQueue.h"
#include "video/geometry/ContentGeometryScanner.h"
#include "video/geometry/GeometrySettings.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

using namespace JSONRPC;

namespace
{
using FilterField = CFileItemHandler::FilterField;
using KODI::MEDIA::MediaType;
using KODI::MEDIA::MediaTypeFromName;
using KODI::MEDIA::MediaTypeOf;
using KODI::MEDIA::NameOf;

constexpr FilterField MOVIE_FILTERS[] = {FilterField::Number("genreId", "genreid"),
                                         FilterField::Text("genre"),
                                         FilterField::Number("year"),
                                         FilterField::Text("actor"),
                                         FilterField::Text("director"),
                                         FilterField::Text("studio"),
                                         FilterField::Text("country"),
                                         FilterField::Number("setId", "setid"),
                                         FilterField::Text("set"),
                                         FilterField::Text("tag")};
constexpr FilterField TVSHOW_FILTERS[] = {FilterField::Number("genreId", "genreid"),
                                          FilterField::Text("genre"),
                                          FilterField::Number("year"),
                                          FilterField::Text("actor"),
                                          FilterField::Text("studio"),
                                          FilterField::Text("tag")};
constexpr FilterField EPISODE_FILTERS[] = {
    FilterField::Number("genreId", "genreid"), FilterField::Text("genre"),
    FilterField::Number("year"), FilterField::Text("actor"), FilterField::Text("director")};
constexpr FilterField MUSICVIDEO_FILTERS[] = {
    FilterField::Text("artist"),   FilterField::Number("genreId", "genreid"),
    FilterField::Text("genre"),    FilterField::Number("year"),
    FilterField::Text("director"), FilterField::Text("studio"),
    FilterField::Text("tag")};
constexpr std::span<const FilterField> NO_FILTERS;

//! What a client names a kind by, and the names and types of its items
struct KindTraits
{
  VideoKind kind;
  MediaType type; //!< the media type, whose name the kind goes by on the wire
  const char* id;
  const char* fields;
  const char* filter; //!< nullptr for a kind that takes no filter
  const char* rules; //!< the smart playlist type a filter's rules are written for
  std::span<const FilterField> filterFields;
  const char* settable;
};

constexpr KindTraits KINDS[] = {
    {VideoKind::Movie, MediaType::MOVIE, "movieId", "Video.Fields.Movie", "Video.Filter.Movies",
     "movies", MOVIE_FILTERS, "Video.Details.Movie.Set"},
    {VideoKind::Set, MediaType::VIDEO_COLLECTION, "setId", "Video.Fields.MovieSet", nullptr, "",
     NO_FILTERS, "Video.Details.MovieSet.Set"},
    {VideoKind::TVShow, MediaType::TV_SHOW, "tvShowId", "Video.Fields.TVShow",
     "Video.Filter.TVShows", "tvshows", TVSHOW_FILTERS, "Video.Details.TVShow.Set"},
    {VideoKind::Season, MediaType::SEASON, "seasonId", "Video.Fields.Season", nullptr, "",
     NO_FILTERS, "Video.Details.Season.Set"},
    {VideoKind::Episode, MediaType::EPISODE, "episodeId", "Video.Fields.Episode",
     "Video.Filter.Episodes", "episodes", EPISODE_FILTERS, "Video.Details.Episode.Set"},
    {VideoKind::MusicVideo, MediaType::MUSIC_VIDEO, "musicVideoId", "Video.Fields.MusicVideo",
     "Video.Filter.MusicVideos", "musicvideos", MUSICVIDEO_FILTERS, "Video.Details.MusicVideo.Set"},
};

const KindTraits& TraitsOf(VideoKind kind)
{
  return *std::ranges::find(KINDS, kind, &KindTraits::kind);
}

//! The error target for an item, in the addressing the caller used
CVariant ItemTarget(VideoKind kind, int id)
{
  CVariant item(CVariant::VariantTypeObject);
  item["kind"] = NameOf(TraitsOf(kind).type);
  item["id"] = id;
  return Target("item", item);
}

const KindTraits* TraitsNamed(std::string_view name)
{
  const auto traits = std::ranges::find(KINDS, MediaTypeOf(name), &KindTraits::type);
  return traits == std::end(KINDS) ? nullptr : &*traits;
}

//! Checks the query's parameters against the kind the caller named; \p checked has them filled
JSONRPC_STATUS CheckForKind(const KindTraits& traits,
                            const CVariant& parameterObject,
                            CVariant& checked,
                            CVariant& errorData)
{
  if (const JSONRPC_STATUS status = CFileItemHandler::CheckAgainstType(
          traits.fields, "properties", parameterObject["properties"], checked["properties"],
          errorData);
      status != OK)
    return status;

  if (!parameterObject["filter"].isNull())
  {
    if (!traits.filter)
      return CFileItemHandler::RefuseForKind("filter", traits.type, errorData);

    if (const JSONRPC_STATUS status = CFileItemHandler::CheckAgainstType(
            traits.filter, "filter", parameterObject["filter"], checked["filter"], errorData);
        status != OK)
      return status;
  }

  if (parameterObject["tvShowId"].asInteger() != -1 && traits.kind != VideoKind::Season &&
      traits.kind != VideoKind::Episode)
    return CFileItemHandler::RefuseForKind("tvShowId", traits.type, errorData);

  if (parameterObject["season"].asInteger() != -1 && traits.kind != VideoKind::Episode)
    return CFileItemHandler::RefuseForKind("season", traits.type, errorData);

  return OK;
}
} // unnamed namespace

JSONRPC_STATUS CVideoLibrary::GetItems(const CVariant& parameterObject, CVariant& result)
{
  const KindTraits* traits = TraitsNamed(parameterObject["kind"].asString());
  if (!traits)
    return InvalidParams;

  CVariant checked(parameterObject);
  if (const JSONRPC_STATUS status = CheckForKind(*traits, parameterObject, checked, result);
      status != OK)
    return status;

  return Query(traits->kind, Listing::All, checked, result);
}

JSONRPC_STATUS CVideoLibrary::Query(VideoKind kind,
                                    Listing listing,
                                    const CVariant& parameterObject,
                                    CVariant& result)
{
  const KindTraits& traits = TraitsOf(kind);

  CVideoDatabase videodatabase;
  if (!videodatabase.Open())
    return InternalError;

  const int details = RequiresAdditionalDetails(traits.type, parameterObject);
  CFileItemList items;

  if (listing == Listing::RecentlyAdded)
  {
    bool listed = false;
    if (kind == VideoKind::Movie)
      listed = videodatabase.GetRecentlyAddedMoviesNav(KODI::VIDEODB::RECENTLY_ADDED_MOVIES, items,
                                                       0, details);
    else if (kind == VideoKind::Episode)
      listed = videodatabase.GetRecentlyAddedEpisodesNav(KODI::VIDEODB::RECENTLY_ADDED_EPISODES,
                                                         items, 0, details);
    else if (kind == VideoKind::MusicVideo)
      listed = videodatabase.GetRecentlyAddedMusicVideosNav(
          KODI::VIDEODB::RECENTLY_ADDED_MUSICVIDEOS, items, 0, details);
    if (!listed)
      return InternalError;

    return HandleItems(traits.id, "items", items, parameterObject, result, true);
  }

  if (listing == Listing::InProgress)
  {
    if (!videodatabase.GetInProgressTvShowsNav(KODI::VIDEODB::INPROGRESS_TVSHOWS, items, details))
      return InternalError;

    return HandleItems(traits.id, "items", items, parameterObject, result, true);
  }

  const int tvshowID = static_cast<int>(parameterObject["tvShowId"].asInteger());

  // Sets and seasons are grouped in memory, where the caller's sort and limits apply
  if (kind == VideoKind::Set || kind == VideoKind::Season)
  {
    const bool listed =
        kind == VideoKind::Set
            ? videodatabase.GetSetsNav(KODI::VIDEODB::MOVIE_SETS, items, VideoDbContentType::MOVIES)
            : videodatabase.GetSeasonsNav(
                  StringUtils::Format("{}{}/", KODI::VIDEODB::TVSHOW_TITLES, tvshowID), items, -1,
                  -1, -1, -1, tvshowID, false);
    if (!listed)
      return InternalError;

    HandleFileItemList(traits.id, false, "items", items, parameterObject, result);
    return OK;
  }

  SortDescription sorting;
  ParseLimits(parameterObject, sorting.limitStart, sorting.limitEnd);
  if (!ParseSorting(parameterObject, sorting.sortBy, sorting.sortOrder, sorting.sortAttributes))
    return InvalidParams;

  const int season = static_cast<int>(parameterObject["season"].asInteger());
  std::string path;
  if (kind == VideoKind::Movie)
    path = KODI::VIDEODB::MOVIE_TITLES;
  else if (kind == VideoKind::TVShow)
    path = KODI::VIDEODB::TVSHOW_TITLES;
  else if (kind == VideoKind::Episode)
    path = StringUtils::Format("{}{}/{}/", KODI::VIDEODB::TVSHOW_TITLES, tvshowID, season);
  else
    path = KODI::VIDEODB::MUSICVIDEO_TITLES;

  CVideoDbUrl videoUrl;
  if (!videoUrl.FromString(path))
    return InternalError;

  if (!ApplyFilter(parameterObject["filter"], traits.filterFields, traits.rules, videoUrl))
    return InvalidParams;

  if (kind == VideoKind::Episode)
  {
    // a season, and the genre and actor filters, narrow one show's episodes
    if (tvshowID <= 0 && (season > 0 || videoUrl.HasOption("genreid") ||
                          videoUrl.HasOption("genre") || videoUrl.HasOption("actor")))
      return InvalidParams;

    if (tvshowID > 0)
    {
      videoUrl.AddOption("tvshowid", tvshowID);
      if (season >= 0)
        videoUrl.AddOption("season", season);
    }
  }

  const std::string url = videoUrl.ToString();
  if (kind == VideoKind::Movie)
  {
    if (!videodatabase.GetMoviesByWhere(url, CDatabase::Filter(), items, sorting, details))
      return InvalidParams;
  }
  else if (kind == VideoKind::TVShow)
  {
    if (!videodatabase.GetTvShowsByWhere(url, CDatabase::Filter(), items, sorting, details))
      return InvalidParams;
  }
  else if (kind == VideoKind::Episode)
  {
    if (!videodatabase.GetEpisodesByWhere(url, CDatabase::Filter(), items, false, sorting, details))
      return InvalidParams;
  }
  else if (!videodatabase.GetMusicVideosByWhere(url, CDatabase::Filter(), items, true, sorting,
                                                details))
    return InternalError;

  return HandleItems(traits.id, "items", items, parameterObject, result, false);
}

JSONRPC_STATUS CVideoLibrary::GetItemProperties(const CVariant& parameterObject, CVariant& result)
{
  const KindTraits* traits = TraitsNamed(parameterObject["item"]["kind"].asString());
  if (!traits)
    return InvalidParams;

  CVariant fields;
  if (const JSONRPC_STATUS status = CheckAgainstType(traits->fields, "properties",
                                                     parameterObject["properties"], fields, result);
      status != OK)
    return status;

  CVideoDatabase videodatabase;
  if (!videodatabase.Open())
    return InternalError;

  return ReadItem(traits->kind, static_cast<int>(parameterObject["item"]["id"].asInteger()), fields,
                  videodatabase, result);
}

JSONRPC_STATUS CVideoLibrary::SetItemProperties(const CVariant& parameterObject, CVariant& result)
{
  const KindTraits* traits = TraitsNamed(parameterObject["item"]["kind"].asString());
  if (!traits)
    return InvalidParams;

  CVariant properties;
  if (const JSONRPC_STATUS status =
          CheckAgainstType(traits->settable, "properties",
                           GivenMembers(parameterObject["properties"]), properties, result);
      status != OK)
    return status;

  CVideoDatabase videodatabase;
  if (!videodatabase.Open())
    return InternalError;

  // Announced once below, with what changed
  videodatabase.SetAnnounceUpdates(false);

  const int id = static_cast<int>(parameterObject["item"]["id"].asInteger());
  JSONRPC_STATUS status{InternalError};
  switch (traits->kind)
  {
    case VideoKind::Movie:
      status = SetMovieDetails(id, properties, videodatabase, result);
      break;
    case VideoKind::Set:
      status = SetMovieSetDetails(id, properties, videodatabase, result);
      break;
    case VideoKind::TVShow:
      status = SetTVShowDetails(id, properties, videodatabase, result);
      break;
    case VideoKind::Season:
      status = SetSeasonDetails(id, properties, videodatabase, result);
      break;
    case VideoKind::Episode:
      status = SetEpisodeDetails(id, properties, videodatabase, result);
      break;
    case VideoKind::MusicVideo:
      status = SetMusicVideoDetails(id, properties, videodatabase, result);
      break;
  }
  if (status != OK)
    return status;

  const CVariant names{ReadableNames(properties, traits->fields)};
  status = ReadItem(traits->kind, id, names, videodatabase, result);
  if (status != OK)
    return status;

  AnnounceChange(ANNOUNCEMENT::VideoLibrary, traits->type, id, names, result);
  return OK;
}

JSONRPC_STATUS CVideoLibrary::ReadItem(
    VideoKind kind, int id, const CVariant& fields, CVideoDatabase& videodatabase, CVariant& result)
{
  const KindTraits& traits = TraitsOf(kind);

  CVariant request(CVariant::VariantTypeObject);
  request["properties"] = fields;
  const int details = RequiresAdditionalDetails(traits.type, request);

  CVideoInfoTag infos;
  std::shared_ptr<CFileItem> item;
  CDatabase::GetResult lookup{CDatabase::GetResult::Error};
  switch (kind)
  {
    case VideoKind::Movie:
      //! @todo API support for video version id
      lookup = videodatabase.TryGetMovieInfo("", infos, id, -1, -1, details);
      break;
    case VideoKind::Set:
      lookup = videodatabase.TryGetSetInfo(id, infos);
      break;
    case VideoKind::TVShow:
      item = std::make_shared<CFileItem>();
      lookup = videodatabase.TryGetTvShowInfo("", infos, id, item.get(), details);
      break;
    case VideoKind::Season:
      lookup = videodatabase.TryGetSeasonInfo(id, infos);
      break;
    case VideoKind::Episode:
      lookup = videodatabase.TryGetEpisodeInfo("", infos, id, details);
      break;
    case VideoKind::MusicVideo:
      lookup = videodatabase.TryGetMusicVideoInfo("", infos, id, details);
      break;
  }
  if (const JSONRPC_STATUS status = StatusFor(lookup, result, ItemTarget(kind, id)); status != OK)
    return status;

  if (item)
    item->SetFromVideoInfoTag(infos);
  else
    item = std::make_shared<CFileItem>(infos);

  if (kind == VideoKind::Season && infos.m_iIdShow <= 0)
    return NotFound;

  if (kind == VideoKind::Episode)
  {
    // the show's path is what finds the fanart
    int tvshowid = infos.m_iIdShow;
    if (tvshowid <= 0)
      tvshowid = videodatabase.GetTvShowForEpisode(id);

    item->SetPath(StringUtils::Format("{}{}/{}/{}", KODI::VIDEODB::TVSHOW_TITLES, tvshowid,
                                      infos.m_iSeason, id));
  }

  CVariant answer;
  HandleFileItem(traits.id, kind != VideoKind::Set && kind != VideoKind::Season, "item", item,
                 request, fields, answer, false);
  result = std::move(answer["item"]);
  return OK;
}

namespace
{
enum class Facet
{
  GENRES,
  TAGS,
};

//! The content a genre or tag listing of \p type reads, and the path of that listing
std::optional<std::pair<VideoDbContentType, std::string>> FacetListing(MediaType type, Facet facet)
{
  const bool genres{facet == Facet::GENRES};
  switch (type)
  {
    case MediaType::MOVIE:
      return {{VideoDbContentType::MOVIES,
               genres ? KODI::VIDEODB::MOVIE_GENRES : KODI::VIDEODB::MOVIE_TAGS}};
    case MediaType::TV_SHOW:
      return {{VideoDbContentType::TVSHOWS,
               genres ? KODI::VIDEODB::TVSHOW_GENRES : KODI::VIDEODB::TVSHOW_TAGS}};
    case MediaType::MUSIC_VIDEO:
      return {{VideoDbContentType::MUSICVIDEOS,
               genres ? KODI::VIDEODB::MUSICVIDEO_GENRES : KODI::VIDEODB::MUSICVIDEO_TAGS}};
    default:
      return {};
  }
}
} // unnamed namespace

JSONRPC_STATUS CVideoLibrary::GetGenres(const CVariant& parameterObject, CVariant& result)
{
  const auto listing{
      FacetListing(MediaTypeFromName(parameterObject["type"].asString()), Facet::GENRES)};
  if (!listing)
    return InvalidParams;
  const auto& [idContent, strPath] = *listing;

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
  const auto listing{
      FacetListing(MediaTypeFromName(parameterObject["type"].asString()), Facet::TAGS)};
  if (!listing)
    return InvalidParams;
  const auto& [idContent, strPath] = *listing;

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
const std::map<std::string, MediaType> mediaIDTypes = {
    {"episodeId", MediaType::EPISODE},      {"tvShowId", MediaType::TV_SHOW},
    {"seasonId", MediaType::SEASON},        {"movieId", MediaType::MOVIE},
    {"setId", MediaType::VIDEO_COLLECTION}, {"musicVideoId", MediaType::MUSIC_VIDEO},
};

//! The type and id of the library item \p item names, or an id of -1
std::pair<MediaType, int> ArtItemOf(const CVariant& item)
{
  for (const auto& [member, type] : mediaIDTypes)
  {
    if (item.isMember(member))
      return {type, item[member].asInteger32()};
  }
  return {MediaType::NONE, -1};
}
} // unnamed namespace

JSONRPC_STATUS CVideoLibrary::GetAvailableArtTypes(const CVariant& parameterObject,
                                                   CVariant& result)
{
  const auto [mediaType, mediaID] = ArtItemOf(parameterObject["item"]);
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
  const auto [mediaType, mediaID] = ArtItemOf(parameterObject["item"]);
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

JSONRPC_STATUS CVideoLibrary::SetMovieDetails(int id,
                                              const CVariant& properties,
                                              CVideoDatabase& videodatabase,
                                              CVariant& result)
{
  CVideoInfoTag infos;
  //! @todo API support for video version id
  if (const JSONRPC_STATUS status = StatusFor(videodatabase.TryGetMovieInfo("", infos, id, -1),
                                              result, ItemTarget(VideoKind::Movie, id));
      status != OK)
    return status;

  const PlaybackUpdate before{infos.GetPlayCount(), infos.m_lastPlayed};

  const DetailsEdit edit = EditDetails(properties, infos, videodatabase);

  if (videodatabase.UpdateDetailsForMovie(id, infos, edit.artwork, edit.updatedDetails) <= 0)
    return InternalError;

  if (!videodatabase.RemoveArtForItem(infos.m_iDbId, MediaType::MOVIE, edit.removedArtwork))
    return InternalError;

  StorePlaybackEdit(properties, before, infos, videodatabase);

  CJSONRPCUtils::NotifyItemUpdated(infos, edit.artwork);
  return OK;
}

JSONRPC_STATUS CVideoLibrary::SetMovieSetDetails(int id,
                                                 const CVariant& properties,
                                                 CVideoDatabase& videodatabase,
                                                 CVariant& result)
{
  CVideoInfoTag infos;
  if (const JSONRPC_STATUS status =
          StatusFor(videodatabase.TryGetSetInfo(id, infos), result, ItemTarget(VideoKind::Set, id));
      status != OK)
    return status;

  const DetailsEdit edit = EditDetails(properties, infos, videodatabase);

  if (videodatabase.SetDetailsForMovieSet(infos, edit.artwork, id) <= 0)
    return InternalError;

  if (!videodatabase.RemoveArtForItem(infos.m_iDbId, MediaType::VIDEO_COLLECTION,
                                      edit.removedArtwork))
    return InternalError;

  CJSONRPCUtils::NotifyItemUpdated();
  return OK;
}

JSONRPC_STATUS CVideoLibrary::SetTVShowDetails(int id,
                                               const CVariant& properties,
                                               CVideoDatabase& videodatabase,
                                               CVariant& result)
{
  CVideoInfoTag infos;
  if (const JSONRPC_STATUS status = StatusFor(videodatabase.TryGetTvShowInfo("", infos, id), result,
                                              ItemTarget(VideoKind::TVShow, id));
      status != OK)
    return status;

  KODI::ART::SeasonsArtwork seasonArt;
  videodatabase.GetTvShowSeasonArt(infos.m_iDbId, seasonArt);

  const DetailsEdit edit = EditDetails(properties, infos, videodatabase);

  // we need to manually remove tags/taglinks for now because they aren't replaced
  // due to scrapers not supporting them
  videodatabase.RemoveTagsFromItem(id, MediaType::TV_SHOW);

  if (!videodatabase.UpdateDetailsForTvShow(id, infos, edit.artwork, seasonArt))
    return InternalError;

  if (!videodatabase.RemoveArtForItem(infos.m_iDbId, MediaType::TV_SHOW, edit.removedArtwork))
    return InternalError;

  const bool updatePlaycount = ParameterNotNull(properties, "playCount");
  const bool updateLastplayed = ParameterNotNull(properties, "lastPlayed");
  if (updatePlaycount || updateLastplayed)
  {
    // a tvshow has no file row of its own - its playcount is derived from its
    // episodes, so the new values have to be applied to every episode of the show
    CVideoDbUrl videoUrl;
    if (!videoUrl.FromString(StringUtils::Format("{}{}/-1/", KODI::VIDEODB::TVSHOW_TITLES, id)))
      return InternalError;
    videoUrl.AddOption("tvshowid", id);

    CFileItemList episodes;
    if (!videodatabase.GetEpisodesByWhere(videoUrl.ToString(), CDatabase::Filter(), episodes,
                                          false))
      return InternalError;

    // each episode is an item of its own, which the show's announcement does not cover
    videodatabase.SetAnnounceUpdates(true);
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
  return OK;
}

JSONRPC_STATUS CVideoLibrary::SetSeasonDetails(int id,
                                               const CVariant& properties,
                                               CVideoDatabase& videodatabase,
                                               CVariant& result)
{
  CVideoInfoTag infos;
  if (const JSONRPC_STATUS status = StatusFor(videodatabase.TryGetSeasonInfo(id, infos), result,
                                              ItemTarget(VideoKind::Season, id));
      status != OK)
    return status;
  if (infos.m_iIdShow <= 0)
    return Fail(result, NotFound, Reason::NoSuchItem, ItemTarget(VideoKind::Season, id));

  const DetailsEdit edit = EditDetails(properties, infos, videodatabase);
  if (ParameterNotNull(properties, "title"))
    infos.SetSortTitle(properties["title"].asString());

  if (videodatabase.SetDetailsForSeason(infos, edit.artwork, infos.m_iIdShow, id) <= 0)
    return InternalError;

  if (!videodatabase.RemoveArtForItem(infos.m_iDbId, MediaType::SEASON, edit.removedArtwork))
    return InternalError;

  CJSONRPCUtils::NotifyItemUpdated();
  return OK;
}

JSONRPC_STATUS CVideoLibrary::SetEpisodeDetails(int id,
                                                const CVariant& properties,
                                                CVideoDatabase& videodatabase,
                                                CVariant& result)
{
  CVideoInfoTag infos;
  if (const JSONRPC_STATUS status = StatusFor(videodatabase.TryGetEpisodeInfo("", infos, id),
                                              result, ItemTarget(VideoKind::Episode, id));
      status != OK)
    return status;

  int tvshowid = videodatabase.GetTvShowForEpisode(id);
  if (tvshowid <= 0)
    return Fail(result, NotFound, Reason::NoSuchItem, ItemTarget(VideoKind::Episode, id));

  const PlaybackUpdate before{infos.GetPlayCount(), infos.m_lastPlayed};

  const DetailsEdit edit = EditDetails(properties, infos, videodatabase);

  if (videodatabase.SetDetailsForEpisode(infos, edit.artwork, tvshowid, id) <= 0)
    return InternalError;

  if (!videodatabase.RemoveArtForItem(infos.m_iDbId, MediaType::EPISODE, edit.removedArtwork))
    return InternalError;

  StorePlaybackEdit(properties, before, infos, videodatabase);

  CJSONRPCUtils::NotifyItemUpdated();
  return OK;
}

JSONRPC_STATUS CVideoLibrary::SetMusicVideoDetails(int id,
                                                   const CVariant& properties,
                                                   CVideoDatabase& videodatabase,
                                                   CVariant& result)
{
  CVideoInfoTag infos;
  if (const JSONRPC_STATUS status = StatusFor(videodatabase.TryGetMusicVideoInfo("", infos, id),
                                              result, ItemTarget(VideoKind::MusicVideo, id));
      status != OK)
    return status;

  const PlaybackUpdate before{infos.GetPlayCount(), infos.m_lastPlayed};

  const DetailsEdit edit = EditDetails(properties, infos, videodatabase);

  // we need to manually remove tags/taglinks for now because they aren't replaced
  // due to scrapers not supporting them
  videodatabase.RemoveTagsFromItem(id, MediaType::MUSIC_VIDEO);

  if (videodatabase.SetDetailsForMusicVideo(infos, edit.artwork, id) <= 0)
    return InternalError;

  if (!videodatabase.RemoveArtForItem(infos.m_iDbId, MediaType::MUSIC_VIDEO, edit.removedArtwork))
    return InternalError;

  StorePlaybackEdit(properties, before, infos, videodatabase);

  CJSONRPCUtils::NotifyItemUpdated();
  return OK;
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
    return Fail(result, FailedToExecute, Reason::FeatureDisabled);

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
      return Fail(result, NotFound, Reason::NoSuchItem, Target(key, item[key]));

    fileItem.SetFromVideoInfoTag(infos);
  }

  const KODI::VIDEO::GEOMETRY::SamplingDepth depth{
      parameterObject["thorough"].asBoolean(false) ? KODI::VIDEO::GEOMETRY::SamplingDepth::Thorough
                                                   : KODI::VIDEO::GEOMETRY::SamplingDepth::Normal};

  if (!KODI::VIDEO::GEOMETRY::RemeasureContentGeometry(fileItem, depth))
    return Fail(result, Unavailable, Reason::MeasureFailed);

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
      return Fail(result, NotFound, Reason::NoSuchSource,
                  Target("directory", parameterObject["directory"]));
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
        return Fail(result, NotFound, Reason::NoSuchAddon,
                    Target("scraperId", parameterObject["scraperId"]));
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
      return Fail(result, NotFound, Reason::NotInLibrary,
                  Target("directory", parameterObject["directory"]));
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

int CVideoLibrary::RequiresAdditionalDetails(MediaType mediaType, const CVariant& parameterObject)
{
  if (mediaType != MediaType::MOVIE && mediaType != MediaType::TV_SHOW &&
      mediaType != MediaType::EPISODE && mediaType != MediaType::MUSIC_VIDEO)
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
    videodatabase.SetPlayCount(CFileItem(details), details.GetPlayCount(), details.m_lastPlayed);

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
