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
#include "interfaces/AnnouncementManager.h"
#include "messaging/ApplicationMessenger.h"
#include "utils/ArtTypes.h"
#include "utils/ContentNames.h"
#include "utils/DatabaseUtils.h"
#include "utils/SortUtils.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/Variant.h"
#include "video/VideoDatabase.h"
#include "video/VideoDbPaths.h"
#include "video/VideoDbUrl.h"
#include "video/VideoLibrary.h"
#include "video/VideoLibraryQueue.h"
#include "video/geometry/ContentGeometryScanner.h"
#include "video/geometry/GeometrySettings.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

using namespace JSONRPC;
namespace UPDATED_DETAIL = KODI::VIDEO::UPDATED_DETAIL;

namespace
{
using FilterField = CFileItemHandler::FilterField;
using KODI::MEDIA::MediaTypeFromName;

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

//! A video kind, and how its items are filtered
struct KindTraits : LibraryKind<VideoKind>
{
  const char* filter; //!< nullptr for a kind that takes no filter
  const char* rules; //!< the smart playlist type a filter's rules are written for
  std::span<const FilterField> filterFields;
};

constexpr KindTraits KIND_TABLE[] = {
    {{VideoKind::Movie, KODI::MEDIA::TYPE::MOVIE, "movieId", "Video.Fields.Movie",
      "Video.Details.Movie.Set"},
     "Video.Filter.Movies",
     KODI::MEDIA::CONTENT::MOVIES,
     MOVIE_FILTERS},
    {{VideoKind::Set, KODI::MEDIA::TYPE::VIDEO_COLLECTION, "setId", "Video.Fields.MovieSet",
      "Video.Details.MovieSet.Set"},
     nullptr,
     "",
     NO_FILTERS},
    {{VideoKind::TVShow, KODI::MEDIA::TYPE::TV_SHOW, "tvShowId", "Video.Fields.TVShow",
      "Video.Details.TVShow.Set"},
     "Video.Filter.TVShows",
     KODI::MEDIA::CONTENT::TVSHOWS,
     TVSHOW_FILTERS},
    {{VideoKind::Season, KODI::MEDIA::TYPE::SEASON, "seasonId", "Video.Fields.Season",
      "Video.Details.Season.Set"},
     nullptr,
     "",
     NO_FILTERS},
    {{VideoKind::Episode, KODI::MEDIA::TYPE::EPISODE, "episodeId", "Video.Fields.Episode",
      "Video.Details.Episode.Set"},
     "Video.Filter.Episodes",
     KODI::MEDIA::CONTENT::EPISODES,
     EPISODE_FILTERS},
    {{VideoKind::MusicVideo, KODI::MEDIA::TYPE::MUSIC_VIDEO, "musicVideoId", "Video.Fields.MusicVideo",
      "Video.Details.MusicVideo.Set"},
     "Video.Filter.MusicVideos",
     KODI::MEDIA::CONTENT::MUSICVIDEOS,
     MUSICVIDEO_FILTERS},
};

constexpr LibraryKinds<KindTraits, CVideoDatabase> KINDS{KIND_TABLE};

//! The kind whose id member \p item has, nullptr for none
const KindTraits* TraitsOfIdIn(const CVariant& item)
{
  const auto traits = std::ranges::find_if(KIND_TABLE, [&item](const KindTraits& kind)
                                           { return item.isMember(kind.id); });
  return traits == std::end(KIND_TABLE) ? nullptr : &*traits;
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

//! The play state an edit's \p properties set, as \p details hold them after it
CVideoDatabase::PlayStateEdit PlayStateEditOf(const CVariant& properties,
                                              const CVideoInfoTag& details)
{
  const auto given{[&properties](const char* name)
                   { return properties.isMember(name) && !properties[name].isNull(); }};

  CVideoDatabase::PlayStateEdit edit;
  if (given("playCount"))
    edit.playCount = details.GetPlayCount();
  if (given("lastPlayed"))
    edit.lastPlayed = details.m_lastPlayed;
  if (given("resume"))
    edit.resume = CVideoDatabase::PlayStateEdit::Resume{properties["resume"]["position"].asDouble(),
                                                        properties["resume"]["total"].asDouble()};
  return edit;
}

//! Asks the database with a connection of its own; nullopt if it cannot be opened
template<typename Question>
std::optional<bool> Ask(Question&& question)
{
  return KODI::VIDEO::LIBRARY::WithDatabase([&question](CVideoDatabase& db) -> std::optional<bool>
                                            { return question(db); });
}
} // unnamed namespace

bool CVideoLibrary::IsItemKind(KODI::MEDIA::TYPE type)
{
  return KINDS.Holds(type);
}

JSONRPC_STATUS CVideoLibrary::GetItems(const CVariant& parameterObject, CVariant& result)
{
  return GetItemsIn(KINDS, CheckForKind, Query, parameterObject, result);
}

JSONRPC_STATUS CVideoLibrary::Query(VideoKind kind,
                                    Listing listing, const CVariant &parameterObject, CVariant &result)
{
  const KindTraits& traits = KINDS.Of(kind);

  const int details = RequiresAdditionalDetails(traits.type, parameterObject);
  CFileItemList items;

  if (listing == Listing::RecentlyAdded)
  {
    const std::optional<bool> listed{Ask(
        [kind, details, &items](CVideoDatabase& db)
        {
          if (kind == VideoKind::Movie)
            return db.GetRecentlyAddedMoviesNav(KODI::VIDEO::DB_PATH::RECENTLY_ADDED_MOVIES, items,
                                                0, details);
          if (kind == VideoKind::Episode)
            return db.GetRecentlyAddedEpisodesNav(KODI::VIDEO::DB_PATH::RECENTLY_ADDED_EPISODES,
                                                  items, 0, details);
          if (kind == VideoKind::MusicVideo)
            return db.GetRecentlyAddedMusicVideosNav(
                KODI::VIDEO::DB_PATH::RECENTLY_ADDED_MUSICVIDEOS, items, 0, details);
          return false;
        })};
    if (!listed.value_or(false))
      return InternalError;

    return HandleItems(traits.id, items, parameterObject, result, true);
  }

  if (listing == Listing::InProgress)
  {
    if (!Ask(
             [details, &items](CVideoDatabase& db)
             {
               return db.GetInProgressTvShowsNav(KODI::VIDEO::DB_PATH::INPROGRESS_TVSHOWS, items,
                                                 details);
             })
             .value_or(false))
      return InternalError;

    return HandleItems(traits.id, items, parameterObject, result, true);
  }

  const int tvshowID = static_cast<int>(parameterObject["tvShowId"].asInteger());

  // Sets and seasons are grouped in memory, where the caller's sort and limits apply
  if (kind == VideoKind::Set || kind == VideoKind::Season)
  {
    const std::optional<bool> listed{Ask(
        [kind, tvshowID, &items](CVideoDatabase& db)
        {
          return kind == VideoKind::Set
                     ? db.GetSetsNav(KODI::VIDEO::DB_PATH::MOVIE_SETS, items,
                                     VideoDbContentType::MOVIES)
                     : db.GetSeasonsNav(StringUtils::Format(
                                            "{}{}/", KODI::VIDEO::DB_PATH::TVSHOW_TITLES, tvshowID),
                                        items, -1, -1, -1, -1, tvshowID, false);
        })};
    if (!listed.value_or(false))
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
    path = KODI::VIDEO::DB_PATH::MOVIE_TITLES;
  else if (kind == VideoKind::TVShow)
    path = KODI::VIDEO::DB_PATH::TVSHOW_TITLES;
  else if (kind == VideoKind::Episode)
    path = StringUtils::Format("{}{}/{}/", KODI::VIDEO::DB_PATH::TVSHOW_TITLES, tvshowID, season);
  else
    path = KODI::VIDEO::DB_PATH::MUSICVIDEO_TITLES;

  CVideoDbUrl videoUrl;
  if (!videoUrl.FromString(path))
    return InternalError;

  if (!ApplyFilter(parameterObject["filter"], traits.filterFields, traits.rules, videoUrl))
    return InvalidParams;

  if (kind == VideoKind::Episode)
  {
    // a season, and the genre and actor filters, narrow one show's episodes
    if (tvshowID <= 0 && (season > 0 || videoUrl.HasOption("genreid") || videoUrl.HasOption("genre") || videoUrl.HasOption("actor")))
      return InvalidParams;

    if (tvshowID > 0)
    {
      videoUrl.AddOption("tvshowid", tvshowID);
      if (season >= 0)
        videoUrl.AddOption("season", season);
    }
  }

  const std::string url = videoUrl.ToString();
  const std::optional<bool> listed{Ask(
      [kind, &url, &items, &sorting, details](CVideoDatabase& db)
      {
        if (kind == VideoKind::Movie)
          return db.GetMoviesByWhere(url, CDatabase::Filter(), items, sorting, details);
        if (kind == VideoKind::TVShow)
          return db.GetTvShowsByWhere(url, CDatabase::Filter(), items, sorting, details);
        if (kind == VideoKind::Episode)
          return db.GetEpisodesByWhere(url, CDatabase::Filter(), items, false, sorting, details);
        return db.GetMusicVideosByWhere(url, CDatabase::Filter(), items, true, sorting, details);
      })};
  if (!listed)
    return InternalError;
  if (!*listed)
    return kind == VideoKind::MusicVideo ? InternalError : InvalidParams;

  return HandleItems(traits.id, items, parameterObject, result, false);
}

JSONRPC_STATUS CVideoLibrary::GetItemProperties(const CVariant &parameterObject, CVariant &result)
{
  return GetItemPropertiesIn(KINDS, ReadItem, parameterObject, result);
}

JSONRPC_STATUS CVideoLibrary::SetItemProperties(const CVariant& parameterObject, CVariant& result)
{
  const KindTraits* traits = KINDS.Named(parameterObject["item"]["kind"].asString());
  if (!traits)
    return InvalidParams;

  CVariant properties;
  if (const JSONRPC_STATUS status =
          CheckAgainstType(traits->settable, "properties",
                           GivenMembers(parameterObject["properties"]), properties, result);
      status != OK)
    return status;

  const int id = static_cast<int>(parameterObject["item"]["id"].asInteger());

  CVideoInfoTag infos;
  //! @todo API support for video version id
  if (const JSONRPC_STATUS status =
          StatusFor(KODI::VIDEO::LIBRARY::TryGetDetailsByTypeAndId(traits->type, id, infos, nullptr,
                                                                   VideoDbDetailsAll, -1, -1),
                    result, KINDS.ItemTarget(traits->kind, id));
      status != OK)
    return status;

  const DetailsEdit edit = EditDetails(properties, infos);
  if (traits->kind == VideoKind::Season && ParameterNotNull(properties, "title"))
    infos.SetSortTitle(properties["title"].asString());

  switch (KODI::VIDEO::LIBRARY::EditItem(traits->type, id, infos, edit.artwork, edit.removedArtwork,
                                         edit.updatedDetails, PlayStateEditOf(properties, infos)))
  {
    case CVideoDatabase::EditResult::DONE:
      break;
    case CVideoDatabase::EditResult::REFUSED:
      return InvalidParams;
    case CVideoDatabase::EditResult::NOT_FOUND:
      return Fail(result, NotFound, Reason::NoSuchItem, KINDS.ItemTarget(traits->kind, id));
    case CVideoDatabase::EditResult::FAILED:
      return InternalError;
  }

  if (traits->kind == VideoKind::Movie)
    CJSONRPCUtils::NotifyItemUpdated(infos, edit.artwork);
  else
    CJSONRPCUtils::NotifyItemUpdated();

  const CVariant names{ReadableNames(properties, traits->fields)};
  if (const JSONRPC_STATUS status = ReadItemOwnConnection(traits->kind, id, names, result);
      status != OK)
    return status;

  AnnounceChange(ANNOUNCEMENT::VideoLibrary, traits->type, id, names, result);
  return OK;
}

JSONRPC_STATUS CVideoLibrary::AddItem(const CVariant& parameterObject, CVariant& result)
{
  const KindTraits* traits = KINDS.Named(parameterObject["kind"].asString());
  if (!traits || traits->kind == VideoKind::Set || traits->kind == VideoKind::Season)
    return InvalidParams;

  CVariant properties;
  if (const JSONRPC_STATUS status =
          CheckAgainstType(traits->settable, "properties",
                           GivenMembers(parameterObject["properties"]), properties, result);
      status != OK)
    return status;

  const int tvShowId{static_cast<int>(parameterObject["tvShowId"].asInteger())};
  if (tvShowId != -1 && traits->kind != VideoKind::Episode)
    return RefuseForKind("tvShowId", traits->type, result);
  if (tvShowId == -1 && traits->kind == VideoKind::Episode)
    return InvalidParams;

  std::string path{parameterObject["path"].asString()};
  if (traits->kind == VideoKind::TVShow)
    URIUtils::AddSlashAtEnd(path);

  CVideoInfoTag infos;
  DetailsEdit edit;
  UpdateVideoTag(properties, infos, edit.artwork, edit.removedArtwork, edit.updatedDetails);

  int id{-1};
  switch (KODI::VIDEO::LIBRARY::AddItem(traits->type, path, infos, edit.artwork, tvShowId,
                                        PlayStateEditOf(properties, infos), id))
  {
    case CVideoDatabase::AddResult::ADDED:
      break;
    case CVideoDatabase::AddResult::IN_LIBRARY:
      return Fail(result, FailedToExecute, Reason::AlreadyInLibrary, Target("path", path));
    case CVideoDatabase::AddResult::NO_SUCH_SHOW:
      return Fail(result, NotFound, Reason::NoSuchItem,
                  KINDS.ItemTarget(VideoKind::TVShow, tvShowId));
    case CVideoDatabase::AddResult::FAILED:
      return InternalError;
  }

  CVariant names{ReadableNames(properties, traits->fields)};
  if (!names.isArray())
    names = CVariant(CVariant::VariantTypeArray);
  names.push_back("title");
  if (const JSONRPC_STATUS status = ReadItemOwnConnection(traits->kind, id, names, result);
      status != OK)
    return status;

  CFileItem added;
  added.GetVideoInfoTag()->m_iDbId = id;
  added.GetVideoInfoTag()->SetMediaType(traits->type);
  CServiceBroker::GetAnnouncementManager()->Announce(
      ANNOUNCEMENT::VideoLibraryEvent{ANNOUNCEMENT::EVENT::LIBRARY::Update{
          .type = traits->type,
          .id = id,
          .item = std::make_shared<CFileItem>(added),
          .added = true}});
  return OK;
}

JSONRPC_STATUS CVideoLibrary::ReadItem(
    VideoKind kind, int id, const CVariant& fields, CVideoDatabase& videodatabase, CVariant& result)
{
  const KindTraits& traits = KINDS.Of(kind);

  CVariant request(CVariant::VariantTypeObject);
  request["properties"] = fields;
  const int details = RequiresAdditionalDetails(traits.type, request);

  CVideoInfoTag infos;
  const auto item = std::make_shared<CFileItem>();
  // A show's episode counts are answered from its item, which the tag cannot carry.
  //! @todo API support for video version id
  if (const JSONRPC_STATUS status = StatusFor(
          videodatabase.TryGetDetailsByTypeAndId(
              traits.type, id, infos, kind == VideoKind::TVShow ? item.get() : nullptr, details),
          result, KINDS.ItemTarget(kind, id));
      status != OK)
    return status;

  item->SetFromVideoInfoTag(infos);

  if (kind == VideoKind::Season && infos.m_iIdShow <= 0)
    return NotFound;

  if (kind == VideoKind::Episode)
  {
    // the show's path is what finds the fanart
    int tvshowid = infos.m_iIdShow;
    if (tvshowid <= 0)
      tvshowid = videodatabase.GetTvShowForEpisode(id);

    item->SetPath(StringUtils::Format("{}{}/{}/{}", KODI::VIDEO::DB_PATH::TVSHOW_TITLES, tvshowid, infos.m_iSeason, id));
  }

  CVariant answer;

  HandleFileItem(traits.id, kind != VideoKind::Set && kind != VideoKind::Season, "item", item,
                 request, fields, answer, false);
  result = std::move(answer["item"]);
  return OK;
}

JSONRPC_STATUS CVideoLibrary::ReadItemOwnConnection(VideoKind kind,
                                                    int id,
                                                    const CVariant& fields,
                                                    CVariant& result)
{
  return KODI::VIDEO::LIBRARY::WithDatabase([&](CVideoDatabase& db) -> std::optional<JSONRPC_STATUS>
                                            { return ReadItem(kind, id, fields, db, result); })
      .value_or(InternalError);
}

namespace
{
enum class Facet
{
  GENRES,
  TAGS,
};

//! The content a genre or tag listing of \p type reads, and the path of that listing
std::optional<std::pair<VideoDbContentType, std::string>> FacetListing(KODI::MEDIA::TYPE type, Facet facet)
{
  const bool genres{facet == Facet::GENRES};
  switch (type)
  {
    case KODI::MEDIA::TYPE::MOVIE:
      return {{VideoDbContentType::MOVIES,
               genres ? KODI::VIDEO::DB_PATH::MOVIE_GENRES : KODI::VIDEO::DB_PATH::MOVIE_TAGS}};
    case KODI::MEDIA::TYPE::TV_SHOW:
      return {{VideoDbContentType::TVSHOWS,
               genres ? KODI::VIDEO::DB_PATH::TVSHOW_GENRES : KODI::VIDEO::DB_PATH::TVSHOW_TAGS}};
    case KODI::MEDIA::TYPE::MUSIC_VIDEO:
      return {{VideoDbContentType::MUSICVIDEOS, genres ? KODI::VIDEO::DB_PATH::MUSICVIDEO_GENRES
                                                       : KODI::VIDEO::DB_PATH::MUSICVIDEO_TAGS}};
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

  CFileItemList items;
  if (!KODI::VIDEO::LIBRARY::GetGenresNav(strPath, items, idContent))
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

  CFileItemList items;
  if (!KODI::VIDEO::LIBRARY::GetTagsNav(strPath, items, idContent))
    return InternalError;

  /* need to set strTitle in each item*/
  for (int i = 0; i < items.Size(); i++)
    items[i]->GetVideoInfoTag()->m_strTitle = items[i]->GetLabel();

  HandleFileItemList("tagId", false, "tags", items, parameterObject, result);
  return OK;
}

JSONRPC_STATUS CVideoLibrary::GetAvailableArtTypes(const CVariant& parameterObject, CVariant& result)
{
  return GetAvailableArtTypesIn(KINDS, parameterObject, result);
}

JSONRPC_STATUS CVideoLibrary::GetAvailableArt(const CVariant& parameterObject, CVariant& result)
{
  return GetAvailableArtIn(KINDS, parameterObject, result);
}

JSONRPC_STATUS CVideoLibrary::Refresh(const CVariant &parameterObject, CVariant &result)
{
  const KindTraits* traits = KINDS.Named(parameterObject["item"]["kind"].asString());
  if (!traits)
    return InvalidParams;

  const int id = static_cast<int>(parameterObject["item"]["id"].asInteger());
  return RefreshVideo(traits->kind, id, KINDS.ItemTarget(traits->kind, id), parameterObject,
                      result);
}

JSONRPC_STATUS CVideoLibrary::RefreshMovie(const CVariant& parameterObject, CVariant& result)
{
  return RefreshById(parameterObject, result);
}

JSONRPC_STATUS CVideoLibrary::RefreshTVShow(const CVariant& parameterObject, CVariant& result)
{
  return RefreshById(parameterObject, result);
}

JSONRPC_STATUS CVideoLibrary::RefreshEpisode(const CVariant& parameterObject, CVariant& result)
{
  return RefreshById(parameterObject, result);
}

JSONRPC_STATUS CVideoLibrary::RefreshMusicVideo(const CVariant& parameterObject, CVariant& result)
{
  return RefreshById(parameterObject, result);
}

JSONRPC_STATUS CVideoLibrary::RefreshContentGeometry(const CVariant& parameterObject,
                                                     CVariant& result)
{
  if (!KODI::VIDEO::GEOMETRY::ContentGeometryEnabledFromSettings())
    return Fail(result, FailedToExecute, Reason::FeatureDisabled);

  const CVariant& item = parameterObject["item"];

  // A path is taken as given; an item is resolved to the file behind it.
  CFileItem fileItem;
  if (item.isMember("file"))
  {
    fileItem.SetPath(item["file"].asString());
  }
  else
  {
    const KindTraits* traits = KINDS.Named(item["kind"].asString());
    if (!traits)
      return InvalidParams;

    if (traits->kind == VideoKind::Set || traits->kind == VideoKind::TVShow ||
        traits->kind == VideoKind::Season)
      return RefuseForKind("item", traits->type, result);

    const int id = static_cast<int>(item["id"].asInteger());
    CVideoInfoTag infos;
    if (const JSONRPC_STATUS status =
            StatusFor(KODI::VIDEO::LIBRARY::TryGetDetailsByTypeAndId(
                          traits->type, id, infos, nullptr, VideoDbDetailsAll, -1, -1),
                      result, KINDS.ItemTarget(traits->kind, id));
        status != OK)
      return status;

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

    const std::optional<bool> isSource{KODI::VIDEO::LIBRARY::WithDatabase(
        [&directory](CVideoDatabase& db) -> std::optional<bool>
        {
          std::string sourcePath;
          return db.GetSourcePath(directory, sourcePath);
        })};
    if (!isSource)
      return InternalError;
    if (!*isSource)
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

  ADDON::ScraperPtr scraper;
  if (parsed.content != ADDON::ContentType::NONE)
  {
    if (const JSONRPC_STATUS resolved = ResolveScraper(parsed.scraperId, parsed.content, parsed.scraperSettings, scraper, result);
        resolved != OK)
    {
      return resolved;
    }
  }

  KODI::VIDEO::LIBRARY::SetSourceContent(parsed.path, scraper, parsed.settings,
                                         parsed.content == ADDON::ContentType::NONE &&
                                             parsed.clearMode == SourceContentClearMode::REMOVE);

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

    std::set<int> paths;
    if (!KODI::VIDEO::LIBRARY::GetPathsForCleaning(
            directory, contentParam == "video" ? "" : contentParam, paths))
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

bool CVideoLibrary::FillFileItem(const std::string& strFilename, std::shared_ptr<CFileItem>& item)
{
  if (strFilename.empty())
    return false;

  const bool filled{KODI::VIDEO::LIBRARY::DescribePath(strFilename, *item)};

  if (item->GetLabel().empty())
  {
    item->SetLabel(CUtil::GetTitleFromPath(strFilename, false));
    if (item->GetLabel().empty())
      item->SetLabel(URIUtils::GetFileName(strFilename));
  }

  return filled;
}

bool CVideoLibrary::FillFileItemList(const CVariant &parameterObject, CFileItemList &list)
{
  std::string file = parameterObject["file"].asString();
  int recordingID = static_cast<int>(parameterObject["recordingId"].asInteger());

  bool success = false;
  CFileItemPtr fileItem(new CFileItem());
  if (FillFileItem(file, fileItem))
  {
    success = true;
    list.Add(fileItem);
  }

  for (const VideoKind kind : {VideoKind::Movie, VideoKind::Episode, VideoKind::MusicVideo})
  {
    const KindTraits& traits = KINDS.Of(kind);
    const int id = static_cast<int>(parameterObject[traits.id].asInteger(-1));
    CVideoInfoTag details; //! @todo API support for video version id
    if (id > 0 && KODI::VIDEO::LIBRARY::TryGetDetailsByTypeAndId(traits.type, id, details, nullptr,
                                                                 VideoDbDetailsAll, -1,
                                                                 -1) == CDatabase::GetResult::Ok)
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

int CVideoLibrary::RequiresAdditionalDetails(KODI::MEDIA::TYPE mediaType, const CVariant &parameterObject)
{
  if (mediaType != KODI::MEDIA::TYPE::MOVIE && mediaType != KODI::MEDIA::TYPE::TV_SHOW && mediaType != KODI::MEDIA::TYPE::EPISODE && mediaType != KODI::MEDIA::TYPE::MUSIC_VIDEO)
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

JSONRPC_STATUS CVideoLibrary::HandleItems(const char *idProperty, CFileItemList &items, const CVariant &parameterObject, CVariant &result, bool limit /* = true */)
{
  int size = items.Size();
  if (!limit && items.HasProperty("total") && items.GetProperty("total").asInteger() > size)
    size = static_cast<int>(items.GetProperty("total").asInteger());
  HandleFileItemList(idProperty, true, "items", items, parameterObject, result, size, limit);

  return OK;
}

JSONRPC_STATUS CVideoLibrary::RemoveVideo(const CVariant &parameterObject)
{
  // each Remove method's schema requires its kind's id
  const KindTraits& traits = *TraitsOfIdIn(parameterObject);
  const int id = static_cast<int>(parameterObject[traits.id].asInteger());
  if (!KODI::VIDEO::LIBRARY::DeleteItem(DatabaseUtils::VideoContentTypeFromMediaType(traits.type),
                                        id))
    return InternalError;

  CJSONRPCUtils::NotifyItemUpdated();
  return ACK;
}

JSONRPC_STATUS CVideoLibrary::RefreshById(const CVariant& parameterObject, CVariant& result)
{
  const KindTraits* traits = TraitsOfIdIn(parameterObject);
  if (!traits)
    return InvalidParams;

  const CVariant& id = parameterObject[traits->id];
  return RefreshVideo(traits->kind, static_cast<int>(id.asInteger()), Target(traits->id, id),
                      parameterObject, result);
}

JSONRPC_STATUS CVideoLibrary::RefreshVideo(VideoKind kind,
                                           int id,
                                           const CVariant& target,
                                           const CVariant& parameterObject,
                                           CVariant& result)
{
  const std::shared_ptr<CFileItem> item = std::make_shared<CFileItem>();
  const JSONRPC_STATUS status = ResolveRefreshItem(kind, id, target, *item, result);
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

JSONRPC_STATUS CVideoLibrary::ResolveRefreshItem(VideoKind kind,
                                                 int id,
                                                 const CVariant& target,
                                                 CFileItem& item,
                                                 CVariant& result)
{
  CVideoInfoTag details;
  //! @todo API support for video version id
  if (const JSONRPC_STATUS status =
          StatusFor(KODI::VIDEO::LIBRARY::TryGetDetailsByTypeAndId(
                        KINDS.Of(kind).type, id, details, &item, VideoDbDetailsAll, -1, -1),
                    result, target);
      status != OK)
    return status;

  // A set's item holds its videodb:// path, which the tag cannot carry.
  if (kind != VideoKind::Set)
    item.SetFromVideoInfoTag(details);

  return OK;
}

CVideoLibrary::DetailsEdit CVideoLibrary::EditDetails(const CVariant& parameterObject,
                                                      CVideoInfoTag& details)
{
  DetailsEdit edit;
  KODI::VIDEO::LIBRARY::GetArtForItem(details.m_iDbId, details.GetMediaType(), edit.artwork);
  UpdateVideoTag(parameterObject, details, edit.artwork, edit.removedArtwork, edit.updatedDetails);
  return edit;
}

void CVideoLibrary::UpdateVideoTagField(const CVariant& parameterObject,
                                        const std::string& fieldName,
                                        std::string_view detail,
                                        std::vector<std::string>& fieldValue,
                                        std::set<std::string, std::less<>>& updatedDetails)
{
  if (ParameterNotNull(parameterObject, fieldName))
  {
    CopyStringArray(parameterObject[fieldName], fieldValue);
    updatedDetails.emplace(detail);
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
  UpdateVideoTagField(parameterObject, "director", UPDATED_DETAIL::DIRECTOR, director, updatedDetails);
  details.SetDirector(director);

  std::vector<std::string> studio(details.m_studio);
  UpdateVideoTagField(parameterObject, "studio", UPDATED_DETAIL::STUDIO, studio, updatedDetails);
  details.SetStudio(studio);

  if (ParameterNotNull(parameterObject, "plot"))
    details.SetPlot(parameterObject["plot"].asString());
  if (ParameterNotNull(parameterObject, "album"))
    details.SetAlbum(parameterObject["album"].asString());

  std::vector<std::string> artist(details.m_artist);
  UpdateVideoTagField(parameterObject, "artist", UPDATED_DETAIL::ARTIST, artist, updatedDetails);
  details.SetArtist(artist);

  std::vector<std::string> genre(details.m_genre);
  UpdateVideoTagField(parameterObject, "genre", UPDATED_DETAIL::GENRE, genre, updatedDetails);
  details.SetGenre(genre);

  CopyIfGiven(parameterObject, "track", details.m_iTrack);
  if (ParameterNotNull(parameterObject, "rating"))
  {
    details.SetRating(parameterObject["rating"].asFloat());
    updatedDetails.insert(UPDATED_DETAIL::RATINGS);
  }
  if (ParameterNotNull(parameterObject, "votes"))
  {
    details.SetVotes(StringUtils::ReturnDigits(parameterObject["votes"].asString()));
    updatedDetails.insert(UPDATED_DETAIL::RATINGS); //Votes and ratings both need updates now, this will trigger those
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
                            static_cast<int>(rating["votes"].asInteger()),
                            rIt->first,
                            (ParameterNotNull(rating, "default") && rating["default"].asBoolean()));
        }
        else
          details.SetRating(rating["rating"].asFloat(), rIt->first, (ParameterNotNull(rating, "default") && rating["default"].asBoolean()));

        updatedDetails.insert(UPDATED_DETAIL::RATINGS);
      }
      else if (rIt->second.isNull())
      {
        details.RemoveRating(rIt->first);
        updatedDetails.insert(UPDATED_DETAIL::RATINGS);
      }
    }
  }
  CopyIfGiven(parameterObject, "userRating", details.m_iUserRating);
  if (ParameterNotNull(parameterObject, "mpaa"))
    details.SetMPAARating(parameterObject["mpaa"].asString());
  if (ParameterNotNull(parameterObject, "imdbNumber"))
  {
    details.SetUniqueID(parameterObject["imdbNumber"].asString());
    updatedDetails.insert(UPDATED_DETAIL::UNIQUE_ID);
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
        updatedDetails.insert(UPDATED_DETAIL::UNIQUE_ID);
      }
      else if (idIt->second.isNull() && idIt->first != details.GetDefaultUniqueID())
      {
        details.RemoveUniqueID(idIt->first);
        updatedDetails.insert(UPDATED_DETAIL::UNIQUE_ID);
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
  UpdateVideoTagField(parameterObject, "writer", UPDATED_DETAIL::WRITER, credits, updatedDetails);
  details.SetWritingCredits(credits);

  std::vector<std::string> country(details.m_country);
  UpdateVideoTagField(parameterObject, "country", UPDATED_DETAIL::COUNTRY, country, updatedDetails);
  details.SetCountry(country);

  CopyIfGiven(parameterObject, "top250", details.m_iTop250);
  if (ParameterNotNull(parameterObject, "sortTitle"))
    details.SetSortTitle(parameterObject["sortTitle"].asString());
  if (ParameterNotNull(parameterObject, "episodeGuide"))
    details.SetEpisodeGuide(parameterObject["episodeGuide"].asString());
  if (ParameterNotNull(parameterObject, "set"))
  {
    details.SetSet(parameterObject["set"].asString());
    updatedDetails.insert(UPDATED_DETAIL::SET);
  }

  std::vector<std::string> showLink(details.m_showLink);
  UpdateVideoTagField(parameterObject, "showLink", UPDATED_DETAIL::SHOW_LINK, showLink, updatedDetails);
  details.SetShowLink(showLink);

  std::vector<std::string> tags(details.m_tags);
  UpdateVideoTagField(parameterObject, "tag", UPDATED_DETAIL::TAG, tags, updatedDetails);
  details.SetTags(tags);

  if (ParameterNotNull(parameterObject, "thumbnail"))
  {
    std::string value = parameterObject["thumbnail"].asString();
    artwork[KODI::ART::TYPE::THUMB] = StringUtils::Trim(value);
    updatedDetails.insert(UPDATED_DETAIL::ART);
  }
  if (ParameterNotNull(parameterObject, "fanart"))
  {
    std::string value = parameterObject["fanart"].asString();
    artwork[KODI::ART::TYPE::FANART] = StringUtils::Trim(value);
    updatedDetails.insert(UPDATED_DETAIL::ART);
  }

  if (ParameterNotNull(parameterObject, "art") &&
      EditArtwork(parameterObject["art"], artwork, removedArtwork))
    updatedDetails.insert(UPDATED_DETAIL::ART);

  if (ParameterNotNull(parameterObject, "dateAdded"))
  {
    SetFromDBDateTime(parameterObject["dateAdded"], details.m_dateAdded);
    updatedDetails.insert(UPDATED_DETAIL::DATE_ADDED);
  }
}
