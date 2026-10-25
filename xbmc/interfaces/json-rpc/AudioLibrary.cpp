/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "AudioLibrary.h"

#include "FileItem.h"
#include "FileItemList.h"
#include "ServiceBroker.h"
#include "Util.h"
#include "addons/AddonManager.h"
#include "addons/Scraper.h"
#include "filesystem/Directory.h"
#include "imagefiles/ImageFileURL.h"
#include "messaging/ApplicationMessenger.h"
#include "music/Album.h"
#include "music/Artist.h"
#include "music/MusicDatabase.h"
#include "music/MusicDbPaths.h"
#include "music/MusicDbUrl.h"
#include "music/MusicLibraryQueue.h"
#include "music/MusicThumbLoader.h"
#include "music/Song.h"
#include "music/tags/MusicInfoTag.h"
#include "settings/AdvancedSettings.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"
#include "utils/ArtTypes.h"
#include "utils/Artwork.h"
#include "utils/ContentNames.h"
#include "utils/SortUtils.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/Variant.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

using namespace MUSIC_INFO;
using namespace JSONRPC;
using namespace XFILE;

namespace
{
//! A property the music library keeps, and how it is read
struct LibraryProperty
{
  std::string_view name;
  std::string (CMusicDatabase::*read)() const;
};

constexpr LibraryProperty LIBRARY_PROPERTIES[] = {
    {"libraryLastUpdated", &CMusicDatabase::GetLibraryLastUpdated},
    {"libraryLastCleaned", &CMusicDatabase::GetLibraryLastCleaned},
    {"artistLinksUpdated", &CMusicDatabase::GetArtistLinksUpdated},
    {"songsLastAdded", &CMusicDatabase::GetSongsLastAdded},
    {"albumsLastAdded", &CMusicDatabase::GetAlbumsLastAdded},
    {"artistsLastAdded", &CMusicDatabase::GetArtistsLastAdded},
    {"genresLastAdded", &CMusicDatabase::GetGenresLastAdded},
    {"songsModified", &CMusicDatabase::GetSongsLastModified},
    {"albumsModified", &CMusicDatabase::GetAlbumsLastModified},
    {"artistsModified", &CMusicDatabase::GetArtistsLastModified},
};

const LibraryProperty* LibraryPropertyNamed(std::string_view name)
{
  const auto it = std::ranges::find(LIBRARY_PROPERTIES, name, &LibraryProperty::name);
  return it != std::end(LIBRARY_PROPERTIES) ? &*it : nullptr;
}
} // unnamed namespace

JSONRPC_STATUS CAudioLibrary::GetProperties(const CVariant &parameterObject, CVariant &result)
{
  const CVariant& names{parameterObject["properties"]};

  // Make db connection once if one or more properties needs db access
  CMusicDatabase musicdatabase;
  if (std::any_of(names.begin_array(), names.end_array(), [](const CVariant& name)
                  { return LibraryPropertyNamed(name.asString()) != nullptr; }) &&
      !musicdatabase.Open())
    return InternalError;

  CVariant properties = CVariant(CVariant::VariantTypeObject);
  for (CVariant::const_iterator_array it = names.begin_array(); it != names.end_array(); ++it)
  {
    const std::string propertyName = it->asString();
    CVariant property;
    if (propertyName == "missingArtistId")
      property = static_cast<int>(BLANKARTIST_ID);
    else if (const LibraryProperty* library = LibraryPropertyNamed(propertyName); library)
      property = (musicdatabase.*library->read)();

    properties[propertyName] = property;
  }

  result = properties;
  return OK;
}

namespace
{
using FilterField = CFileItemHandler::FilterField;
using KODI::MEDIA::MediaTypeFromName;

constexpr FilterField ARTIST_FILTERS[] = {
    FilterField::Number("genreId", "genreid"),     FilterField::Text("genre"),
    FilterField::Number("songGenreId", "genreid"), FilterField::Text("songGenre", "genre"),
    FilterField::Number("albumId", "albumid"),     FilterField::Text("album"),
    FilterField::Number("songId", "songid")};
constexpr FilterField ALBUM_FILTERS[] = {
    FilterField::Number("artistId", "artistid"), FilterField::Text("artist"),
    FilterField::Number("genreId", "genreid"), FilterField::Text("genre")};
constexpr FilterField SONG_FILTERS[] = {
    FilterField::Number("artistId", "artistid"), FilterField::Text("artist"),
    FilterField::Number("genreId", "genreid"),   FilterField::Text("genre"),
    FilterField::Number("albumId", "albumid"),   FilterField::Text("album")};

//! A music kind, where its items are listed, and how they are filtered
struct KindTraits : LibraryKind<AudioKind>
{
  const char* list; //!< what the JSON listing answers under, and the rules' playlist type
  const char* path;
  const char* filter;
  std::span<const FilterField> filterFields;
};

constexpr KindTraits KIND_TABLE[] = {
    {{AudioKind::Artist, KODI::MEDIA::TYPE::ARTIST, "artistId", "Audio.Fields.Artist",
      "Audio.Details.Artist.Set"},
     KODI::MEDIA::CONTENT::ARTISTS,
     KODI::MUSIC::DB_PATH::ARTISTS,
     "Audio.Filter.Artists",
     ARTIST_FILTERS},
    {{AudioKind::Album, KODI::MEDIA::TYPE::ALBUM, "albumId", "Audio.Fields.Album",
      "Audio.Details.Album.Set"},
     KODI::MEDIA::CONTENT::ALBUMS,
     KODI::MUSIC::DB_PATH::ALBUMS,
     "Audio.Filter.Albums",
     ALBUM_FILTERS},
    {{AudioKind::Song, KODI::MEDIA::TYPE::SONG, "songId", "Audio.Fields.Song",
      "Audio.Details.Song.Set"},
     KODI::MEDIA::CONTENT::SONGS,
     KODI::MUSIC::DB_PATH::SONGS,
     "Audio.Filter.Songs",
     SONG_FILTERS},
};

constexpr LibraryKinds<KindTraits, CMusicDatabase> KINDS{KIND_TABLE};

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
    if (const JSONRPC_STATUS status = CFileItemHandler::CheckAgainstType(
            traits.filter, "filter", parameterObject["filter"], checked["filter"], errorData);
        status != OK)
      return status;
  }

  if (!parameterObject["albumArtistsOnly"].isNull() && traits.kind != AudioKind::Artist)
    return CFileItemHandler::RefuseForKind("albumArtistsOnly", traits.type, errorData);

  if (!parameterObject["includeSingles"].isNull() && traits.kind == AudioKind::Artist)
    return CFileItemHandler::RefuseForKind("includeSingles", traits.type, errorData);

  if (!parameterObject["singlesOnly"].isNull() && traits.kind != AudioKind::Song)
    return CFileItemHandler::RefuseForKind("singlesOnly", traits.type, errorData);

  return OK;
}
} // unnamed namespace

bool CAudioLibrary::IsItemKind(KODI::MEDIA::TYPE type)
{
  return KINDS.Holds(type);
}

JSONRPC_STATUS CAudioLibrary::GetItems(const CVariant& parameterObject, CVariant& result)
{
  return GetItemsIn(KINDS, CheckForKind, Query, parameterObject, result);
}

JSONRPC_STATUS CAudioLibrary::Query(
    AudioKind kind, Listing listing, const CVariant& parameterObject, CVariant& result)
{
  const KindTraits& traits = KINDS.Of(kind);

  CMusicDatabase musicdatabase;
  if (!musicdatabase.Open())
    return InternalError;

  if (listing == Listing::RecentlyAdded || listing == Listing::RecentlyPlayed)
  {
    const bool added = listing == Listing::RecentlyAdded;
    CFileItemList items;
    JSONRPC_STATUS ret;
    if (kind == AudioKind::Album)
    {
      std::vector<CAlbum> albums;
      if (!(added ? musicdatabase.GetRecentlyAddedAlbums(albums)
                  : musicdatabase.GetRecentlyPlayedAlbums(albums)))
        return InternalError;

      for (const CAlbum& album : albums)
      {
        const std::string path =
            StringUtils::Format("{}{}/",
                                added ? KODI::MUSIC::DB_PATH::RECENTLY_ADDED_ALBUMS
                                      : KODI::MUSIC::DB_PATH::RECENTLY_PLAYED_ALBUMS,
                                album.idAlbum);

        CFileItemPtr item;
        FillAlbumItem(album, path, item);
        items.Add(item);
      }

      ret = GetAdditionalAlbumDetails(parameterObject, items, musicdatabase);
    }
    else
    {
      if (added)
      {
        int amount = static_cast<int>(parameterObject["albumLimit"].asInteger());
        if (amount < 0)
          amount = 0;

        if (!musicdatabase.GetRecentlyAddedAlbumSongs(traits.path, items,
                                                      static_cast<unsigned int>(amount)))
          return InternalError;
      }
      else if (!musicdatabase.GetRecentlyPlayedAlbumSongs(traits.path, items))
        return InternalError;

      ret = GetAdditionalSongDetails(parameterObject, items, musicdatabase);
    }
    if (ret != OK)
      return ret;

    HandleFileItemList(traits.id, kind == AudioKind::Song, "items", items, parameterObject, result);
    return OK;
  }

  CMusicDbUrl musicUrl;
  if (!musicUrl.FromString(traits.path))
    return InternalError;

  if (kind == AudioKind::Album)
  {
    if (parameterObject["includeSingles"].asBoolean(false))
      musicUrl.AddOption("show_singles", true);
  }
  else if (kind == AudioKind::Song)
  {
    if (parameterObject["singlesOnly"].asBoolean(false))
      musicUrl.AddOption("singles", true);
    else if (!parameterObject["includeSingles"].asBoolean(true))
      musicUrl.AddOption("singles", false);
  }

  ApplyRoleFilter(parameterObject, musicUrl);
  if (!ApplyFilter(parameterObject["filter"], traits.filterFields, traits.list, musicUrl))
    return InvalidParams;

  if (kind == AudioKind::Artist)
  {
    bool albumArtistsOnly = !CServiceBroker::GetSettingsComponent()->GetSettings()->GetBool(
        CSettings::SETTING_MUSICLIBRARY_SHOWCOMPILATIONARTISTS);
    if (parameterObject["albumArtistsOnly"].isBoolean())
      albumArtistsOnly = parameterObject["albumArtistsOnly"].asBoolean();
    musicUrl.AddOption("albumartistsonly", albumArtistsOnly);
  }

  SortDescription sorting;
  ParseLimits(parameterObject, sorting.limitStart, sorting.limitEnd);
  if (!ParseSorting(parameterObject, sorting.sortBy, sorting.sortOrder, sorting.sortAttributes))
    return InvalidParams;

  int total;
  std::set<std::string, std::less<>> fields;
  if (parameterObject.isMember("properties") && parameterObject["properties"].isArray())
  {
    for (CVariant::const_iterator_array field = parameterObject["properties"].begin_array();
         field != parameterObject["properties"].end_array(); ++field)
      fields.insert(field->asString());
  }

  bool listed;
  if (kind == AudioKind::Artist)
  {
    musicdatabase.SetTranslateBlankArtist(false);
    listed =
        musicdatabase.GetArtistsByWhereJSON(fields, musicUrl.ToString(), result, total, sorting);
  }
  else if (kind == AudioKind::Album)
    listed =
        musicdatabase.GetAlbumsByWhereJSON(fields, musicUrl.ToString(), result, total, sorting);
  else
    listed = musicdatabase.GetSongsByWhereJSON(fields, musicUrl.ToString(), result, total, sorting);
  if (!listed)
    return InternalError;

  RenameList(result, traits.list, "items");
  if (kind != AudioKind::Artist)
    FillListArt(result["items"], fields, traits.id, traits.type);

  int start, end;
  HandleLimits(parameterObject, result, total, start, end);

  return OK;
}

void CAudioLibrary::FillListArt(CVariant& list,
                                const std::set<std::string, std::less<>>& fields,
                                const char* idName,
                                KODI::MEDIA::TYPE mediaType)
{
  const bool song = mediaType == KODI::MEDIA::TYPE::SONG;
  const bool fetchArt = fields.contains("art");
  const bool fetchFanart = fields.contains("fanart");
  const bool fetchThumb = song && fields.contains("thumbnail");
  if (!fetchArt && !fetchFanart && !fetchThumb)
    return;

  CMusicThumbLoader thumbLoader;
  thumbLoader.OnLoaderStart();

  for (unsigned int index = 0; index < list.size(); index++)
  {
    CVariant& entry = list[index];

    // Art is looked up by id alone; a song's is found quicker when its album id is known
    CFileItem item;
    item.GetMusicInfoTag()->SetDatabaseId(entry[idName].asInteger32(), mediaType);
    if (song)
      item.GetMusicInfoTag()->SetAlbumId(entry.isMember("albumId") ? entry["albumId"].asInteger32()
                                                                   : -1);

    thumbLoader.FillLibraryArt(item);

    if (fetchThumb)
      entry["thumbnail"] = item.HasArt(KODI::ART::TYPE::THUMB)
                               ? IMAGE_FILES::URLFromFile(item.GetArt(KODI::ART::TYPE::THUMB))
                               : "";
    if (fetchFanart)
      entry["fanart"] = item.HasArt(KODI::ART::TYPE::FANART)
                            ? IMAGE_FILES::URLFromFile(item.GetArt(KODI::ART::TYPE::FANART))
                            : "";
    if (fetchArt)
    {
      CVariant artObj(CVariant::VariantTypeObject);
      for (const auto& [type, url] : item.GetArt())
      {
        if (!url.empty())
          artObj[type] = IMAGE_FILES::URLFromFile(url);
      }
      entry["art"] = artObj;
    }
  }
}

JSONRPC_STATUS CAudioLibrary::GetItemProperties(const CVariant &parameterObject, CVariant &result)
{
  return GetItemPropertiesIn(KINDS, ReadItem, parameterObject, result);
}

JSONRPC_STATUS CAudioLibrary::SetItemProperties(const CVariant& parameterObject, CVariant& result)
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

  CMusicDatabase musicdatabase;
  if (!musicdatabase.Open())
    return InternalError;

  // Announced once below, with what changed
  musicdatabase.SetAnnounceUpdates(false);

  const int id = static_cast<int>(parameterObject["item"]["id"].asInteger());
  JSONRPC_STATUS status{InternalError};
  switch (traits->kind)
  {
    case AudioKind::Artist:
      status = SetArtistDetails(id, properties, musicdatabase, result);
      break;
    case AudioKind::Album:
      status = SetAlbumDetails(id, properties, musicdatabase, result);
      break;
    case AudioKind::Song:
      status = SetSongDetails(id, properties, musicdatabase, result);
      break;
  }
  if (status != OK)
    return status;

  const CVariant names{ReadableNames(properties, traits->fields)};
  status = ReadItem(traits->kind, id, names, musicdatabase, result);
  if (status != OK)
    return status;

  AnnounceChange(ANNOUNCEMENT::AudioLibrary, traits->type, id, names, result);

  return OK;
}

JSONRPC_STATUS CAudioLibrary::ReadItem(
    AudioKind kind, int id, const CVariant& fields, CMusicDatabase& musicdatabase, CVariant& result)
{
  const KindTraits& traits = KINDS.Of(kind);

  CVariant request(CVariant::VariantTypeObject);
  request["properties"] = fields;

  CFileItemList items;
  JSONRPC_STATUS status;
  if (kind == AudioKind::Artist)
  {
    CMusicDbUrl musicUrl;
    if (!musicUrl.FromString(traits.path))
      return InternalError;
    musicUrl.AddOption("artistid", id);

    CDatabase::Filter filter;
    if (!musicdatabase.GetArtistsByWhere(musicUrl.ToString(), items, SortDescription(), filter))
      return InternalError;
    if (items.Size() != 1)
      return Fail(result, NotFound, Reason::NoSuchItem, KINDS.ItemTarget(kind, id));

    status = GetAdditionalArtistDetails(request, items, musicdatabase);

    // an artist's name is always answered
    request["properties"].append("artist");
  }
  else if (kind == AudioKind::Album)
  {
    CAlbum album;
    if (status = StatusFor(musicdatabase.TryGetAlbum(id, album, false), result,
                           KINDS.ItemTarget(kind, id));
        status != OK)
      return status;

    CFileItemPtr albumItem;
    FillAlbumItem(album, StringUtils::Format("{}{}/", KODI::MUSIC::DB_PATH::ALBUMS, id), albumItem);
    items.Add(albumItem);

    status = GetAdditionalAlbumDetails(request, items, musicdatabase);
  }
  else
  {
    CSong song;
    if (status = StatusFor(musicdatabase.TryGetSong(id, song), result, KINDS.ItemTarget(kind, id));
        status != OK)
      return status;

    CFileItemPtr item = std::make_shared<CFileItem>(song);
    FillItemArtistIDs(song.GetArtistIDArray(), item);
    items.Add(item);

    status = GetAdditionalSongDetails(request, items, musicdatabase);
  }
  if (status != OK)
    return status;

  CVariant answer;

  HandleFileItem(traits.id, kind == AudioKind::Song, "item", items[0], request,
                 request["properties"], answer, false);
  result = std::move(answer["item"]);
  return OK;
}

JSONRPC_STATUS CAudioLibrary::GetGenres(const CVariant &parameterObject, CVariant &result)
{
  CMusicDatabase musicdatabase;
  if (!musicdatabase.Open())
    return InternalError;

  // Check if sources for genre wanted
  bool sourcesneeded(false);
  std::set<std::string> checkProperties;
  checkProperties.insert("sourceId");
  std::set<std::string> additionalProperties;
  if (CheckForAdditionalProperties(parameterObject["properties"], checkProperties, additionalProperties))
    sourcesneeded = (additionalProperties.contains("sourceId"));

  CFileItemList items;
  if (!musicdatabase.GetGenresJSON(items, sourcesneeded))
    return InternalError;

  HandleFileItemList("genreId", false, "genres", items, parameterObject, result);
  return OK;
}

JSONRPC_STATUS CAudioLibrary::GetRoles(const CVariant &parameterObject, CVariant &result)
{
  CMusicDatabase musicdatabase;
  if (!musicdatabase.Open())
    return InternalError;

  CFileItemList items;
  if (!musicdatabase.GetRolesNav(KODI::MUSIC::DB_PATH::SONGS, items))
    return InternalError;

  /* need to set strTitle in each item*/
  for (unsigned int i = 0; i < static_cast<unsigned int>(items.Size()); i++)
    items[i]->GetMusicInfoTag()->SetTitle(items[i]->GetLabel());

  HandleFileItemList("roleId", false, "roles", items, parameterObject, result);
  return OK;
}

JSONRPC_STATUS JSONRPC::CAudioLibrary::GetSources(const CVariant& parameterObject, CVariant& result)
{
  CMusicDatabase musicdatabase;
  if (!musicdatabase.Open())
    return InternalError;

  // Add "file" to "properties" array by default
  CVariant param = parameterObject;
  if (!param.isMember("properties"))
    param["properties"] = CVariant(CVariant::VariantTypeArray);
  if (!param["properties"].isMember("file"))
    param["properties"].append("file");

  CFileItemList items;
  if (!musicdatabase.GetSources(items))
    return InternalError;

  HandleFileItemList("sourceId", true, "sources", items, param, result);
  return OK;
}

JSONRPC_STATUS CAudioLibrary::GetAvailableArtTypes(const CVariant& parameterObject,
                                                   CVariant& result)
{
  return GetAvailableArtTypesIn(KINDS, parameterObject, result);
}

JSONRPC_STATUS CAudioLibrary::GetAvailableArt(const CVariant& parameterObject, CVariant& result)
{
  return GetAvailableArtIn(KINDS, parameterObject, result);
}

JSONRPC_STATUS CAudioLibrary::SetArtistDetails(int id, const CVariant & properties,
                                               CMusicDatabase& musicdatabase, CVariant &result)
{
  CArtist artist;
  if (const JSONRPC_STATUS status = StatusFor(musicdatabase.TryGetArtist(id, artist), result,
                                              KINDS.ItemTarget(AudioKind::Artist, id));
      status != OK)
    return status;

  CopyIfGiven(properties, "artist", artist.strArtist);
  CopyIfGiven(properties, "instrument", artist.instruments);
  CopyIfGiven(properties, "style", artist.styles);
  CopyIfGiven(properties, "mood", artist.moods);
  CopyIfGiven(properties, "born", artist.strBorn);
  CopyIfGiven(properties, "formed", artist.strFormed);
  CopyIfGiven(properties, "description", artist.strBiography);
  CopyIfGiven(properties, "genre", artist.genre);
  CopyIfGiven(properties, "died", artist.strDied);
  CopyIfGiven(properties, "disbanded", artist.strDisbanded);
  CopyIfGiven(properties, "yearsActive", artist.yearsActive);
  CopyIfGiven(properties, "musicBrainzArtistId", artist.strMusicBrainzArtistID);
  CopyIfGiven(properties, "sortName", artist.strSortName);
  CopyIfGiven(properties, "type", artist.strType);
  CopyIfGiven(properties, "gender", artist.strGender);
  CopyIfGiven(properties, "disambiguation", artist.strDisambiguation);

  // Update existing art. Any existing artwork that isn't specified in this request stays as is.
  // If the value is null then the existing art with that type is removed.
  if (ParameterNotNull(properties, "art"))
  {
    // Get current artwork
    musicdatabase.GetArtForItem(artist.idArtist, KODI::MEDIA::TYPE::ARTIST, artist.art);

    std::set<std::string, std::less<>> removedArtwork;
    EditArtwork(properties["art"], artist.art, removedArtwork);
    // Remove null art now, as not done by update
    if (!musicdatabase.RemoveArtForItem(artist.idArtist, KODI::MEDIA::TYPE::ARTIST, removedArtwork))
      return InternalError;
  }

  // Update artist including adding or replacing (but not removing) art
  musicdatabase.SetLibraryLastUpdated();
  const std::string itemSeparator =
      CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_musicItemSeparator;
  if (musicdatabase.UpdateArtist(
          artist.idArtist, artist.strArtist, artist.strSortName, artist.strMusicBrainzArtistID,
          artist.bScrapedMBID, artist.strType, artist.strGender, artist.strDisambiguation,
          artist.strBorn, artist.strFormed, StringUtils::Join(artist.genre, itemSeparator),
          StringUtils::Join(artist.moods, itemSeparator),
          StringUtils::Join(artist.styles, itemSeparator),
          StringUtils::Join(artist.instruments, itemSeparator), artist.strBiography, artist.strDied,
          artist.strDisbanded, StringUtils::Join(artist.yearsActive, itemSeparator),
          artist.thumbURL.GetData()) <= 0)
    return InternalError;

  if (!artist.art.empty())
    musicdatabase.SetArtForItem(artist.idArtist, KODI::MEDIA::TYPE::ARTIST, artist.art);

  CJSONRPCUtils::NotifyItemUpdated();
  return OK;
}

JSONRPC_STATUS CAudioLibrary::SetAlbumDetails(int id, const CVariant & properties,
                                              CMusicDatabase& musicdatabase, CVariant &result)
{
  CAlbum album;
  // Get current album details, but not songs as we do not want to update them here
  if (const JSONRPC_STATUS status = StatusFor(musicdatabase.TryGetAlbum(id, album, false), result,
                                              KINDS.ItemTarget(AudioKind::Album, id));
      status != OK)
    return status;

  CopyIfGiven(properties, "title", album.strAlbum);
  CopyIfGiven(properties, "displayArtist", album.strArtistDesc);
  // Set album sort string before processing artist credits
  CopyIfGiven(properties, "sortArtist", album.strArtistSort);

  // Match up artist names and mbids to make new artist credits
  // Mbid values only apply if there are names
  if (ParameterNotNull(properties, "artist"))
  {
    std::vector<std::string> artists;
    std::vector<std::string> mbids;
    CopyStringArray(properties["artist"], artists);
    // Check for Musicbrainz ids
    CopyIfGiven(properties, "musicBrainzAlbumArtistId", mbids);
    // When display artist is not provided and yet artists is changing make by concatenation
    if (!ParameterNotNull(properties, "displayArtist"))
      album.strArtistDesc = StringUtils::Join(artists, CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_musicItemSeparator);
    album.SetArtistCredits(artists, std::vector<std::string>(), mbids);
    // On updatealbum artists will be changed
    album.bArtistSongMerge = true;
  }

  CopyIfGiven(properties, "description", album.strReview);
  CopyIfGiven(properties, "genre", album.genre);
  CopyIfGiven(properties, "theme", album.themes);
  CopyIfGiven(properties, "mood", album.moods);
  CopyIfGiven(properties, "style", album.styles);
  CopyIfGiven(properties, "type", album.strType);
  CopyIfGiven(properties, "albumLabel", album.strLabel);
  CopyIfGiven(properties, "rating", album.fRating);
  CopyIfGiven(properties, "userRating", album.iUserrating);
  CopyIfGiven(properties, "votes", album.iVotes);
  CopyIfGiven(properties, "year", album.strReleaseDate);
  CopyIfGiven(properties, "musicBrainzAlbumId", album.strMusicBrainzAlbumID);
  CopyIfGiven(properties, "musicBrainzReleaseGroupId", album.strReleaseGroupMBID);
  CopyIfGiven(properties, "isBoxSet", album.bBoxedSet);
  CopyIfGiven(properties, "originalDate", album.strOrigReleaseDate);
  CopyIfGiven(properties, "releaseDate", album.strReleaseDate);
  CopyIfGiven(properties, "albumStatus", album.strReleaseStatus);

  // Update existing art. Any existing artwork that isn't specified in this request stays as is.
  // If the value is null then the existing art with that type is removed.
  if (ParameterNotNull(properties, "art"))
  {
    // Get current artwork
    musicdatabase.GetArtForItem(album.idAlbum, KODI::MEDIA::TYPE::ALBUM, album.art);

    std::set<std::string, std::less<>> removedArtwork;
    EditArtwork(properties["art"], album.art, removedArtwork);
    // Remove null art now, as not done by update
    if (!musicdatabase.RemoveArtForItem(album.idAlbum, KODI::MEDIA::TYPE::ALBUM, removedArtwork))
      return InternalError;
  }

  // Update artist including adding or replacing (but not removing) art
  if (!musicdatabase.UpdateAlbum(album))
    return InternalError;

  CJSONRPCUtils::NotifyItemUpdated();
  return OK;
}

JSONRPC_STATUS CAudioLibrary::SetSongDetails(int id, const CVariant & properties,
                                             CMusicDatabase& musicdatabase, CVariant &result)
{
  CSong song;
  if (const JSONRPC_STATUS status = StatusFor(musicdatabase.TryGetSong(id, song), result,
                                              KINDS.ItemTarget(AudioKind::Song, id));
      status != OK)
    return status;

  CopyIfGiven(properties, "title", song.strTitle);

  CopyIfGiven(properties, "displayArtist", song.strArtistDesc);
  // Set album sort string before processing artist credits
  CopyIfGiven(properties, "sortArtist", song.strArtistSort);

  // Match up artist names and mbids to make new artist credits
  // Mbid values only apply if there are names
  bool updateartists = false;
  if (ParameterNotNull(properties, "artist"))
  {
    std::vector<std::string> artists, mbids;
    updateartists = true;
    CopyStringArray(properties["artist"], artists);
    // Check for Musicbrainz ids
    CopyIfGiven(properties, "musicBrainzArtistId", mbids);
    // When display artist is not provided and yet artists is changing make by concatenation
    if (!ParameterNotNull(properties, "displayArtist"))
      song.strArtistDesc = StringUtils::Join(artists, CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_musicItemSeparator);
    song.SetArtistCredits(artists, std::vector<std::string>(), mbids);
  }

  CopyIfGiven(properties, "genre", song.genre);
  CopyIfGiven(properties, "year", song.strReleaseDate);
  CopyIfGiven(properties, "rating", song.rating);
  CopyIfGiven(properties, "userRating", song.userrating);
  CopyIfGiven(properties, "votes", song.votes);
  if (ParameterNotNull(properties, "track"))
    song.iTrack = (song.iTrack & 0xffff0000) |
                  (static_cast<int>(properties["track"].asInteger()) & 0xffff);
  if (ParameterNotNull(properties, "disc"))
    song.iTrack = (song.iTrack & 0xffff) | (static_cast<int>(properties["disc"].asInteger()) << 16);
  CopyIfGiven(properties, "duration", song.iDuration);
  CopyIfGiven(properties, "comment", song.strComment);
  CopyIfGiven(properties, "musicBrainzTrackId", song.strMusicBrainzTrackID);
  CopyIfGiven(properties, "playCount", song.iTimesPlayed);
  if (ParameterNotNull(properties, "lastPlayed"))
    song.lastPlayed.SetFromDBDateTime(properties["lastPlayed"].asString());
  CopyIfGiven(properties, "mood", song.strMood);
  CopyIfGiven(properties, "discTitle", song.strDiscSubtitle);
  CopyIfGiven(properties, "bpm", song.iBPM);
  CopyIfGiven(properties, "originalDate", song.strOrigReleaseDate);
  CopyIfGiven(properties, "releaseDate", song.strReleaseDate);
  CopyIfGiven(properties, "songVideoUrl", song.songVideoURL);

  // Update existing art. Any existing artwork that isn't specified in this request stays as is.
  // If the value is null then the existing art with that type is removed.
  if (ParameterNotNull(properties, "art"))
  {
    // Get current artwork
    KODI::ART::Artwork artwork;
    musicdatabase.GetArtForItem(song.idSong, KODI::MEDIA::TYPE::SONG, artwork);

    std::set<std::string, std::less<>> removedArtwork;
    EditArtwork(properties["art"], artwork, removedArtwork);
    //Update artwork, not done in update song
    musicdatabase.SetArtForItem(song.idSong, KODI::MEDIA::TYPE::SONG, artwork);
    if (!musicdatabase.RemoveArtForItem(song.idSong, KODI::MEDIA::TYPE::SONG, removedArtwork))
      return InternalError;
  }

  // Update song (not including artwork)
  if (!musicdatabase.UpdateSong(song, updateartists))
    return InternalError;

  const auto item = std::make_shared<CFileItem>(song);
  CJSONRPCUtils::NotifyItemUpdated(item);
  return OK;
}

JSONRPC_STATUS CAudioLibrary::Scan(const CVariant &parameterObject, CVariant &result)
{
  std::string directory = parameterObject["directory"].asString();
  std::string cmd =
      StringUtils::Format("updatelibrary(music, {}, {})", StringUtils::Paramify(directory),
                          parameterObject["showDialogs"].asBoolean() ? "true" : "false");

  CServiceBroker::GetAppMessenger()->SendMsg(TMSG_EXECUTE_BUILT_IN, -1, -1, nullptr, cmd);
  return ACK;
}

JSONRPC_STATUS CAudioLibrary::Export(const CVariant &parameterObject, CVariant &result)
{
  std::string cmd;
  if (parameterObject["options"].isMember("path"))
    cmd = StringUtils::Format("exportlibrary2(music, singlefile, {}, albums, albumartists)",
                              StringUtils::Paramify(parameterObject["options"]["path"].asString()));
  else
  {
    cmd = "exportlibrary2(music, library, dummy, albums, albumartists";
    if (parameterObject["options"]["images"].isBoolean() &&
        parameterObject["options"]["images"].asBoolean() == true)
      cmd += ", artwork";
    if (parameterObject["options"]["overwrite"].isBoolean() &&
        parameterObject["options"]["overwrite"].asBoolean() == true)
      cmd += ", overwrite";
    cmd += ")";
  }
  CServiceBroker::GetAppMessenger()->SendMsg(TMSG_EXECUTE_BUILT_IN, -1, -1, nullptr, cmd);
  return ACK;
}

JSONRPC_STATUS CAudioLibrary::Clean(const CVariant &parameterObject, CVariant &result)
{
  std::string cmd = StringUtils::Format(
      "cleanlibrary(music, {})", parameterObject["showDialogs"].asBoolean() ? "true" : "false");
  CServiceBroker::GetAppMessenger()->SendMsg(TMSG_EXECUTE_BUILT_IN, -1, -1, nullptr, cmd);
  return ACK;
}

bool CAudioLibrary::FillFileItem(
    const std::string& strFilename,
    std::shared_ptr<CFileItem>& item,
    const CVariant& parameterObject /* = CVariant(CVariant::VariantTypeArray) */)
{
  CMusicDatabase musicdatabase;
  if (strFilename.empty())
    return false;

  bool filled = false;
  if (musicdatabase.Open())
  {
    if (CDirectory::Exists(strFilename))
    {
      CAlbum album;
      int albumid = musicdatabase.GetAlbumIdByPath(strFilename);
      if (musicdatabase.GetAlbum(albumid, album, false))
      {
        item->SetFromAlbum(album);
        FillItemArtistIDs(album.GetArtistIDArray(), item);

        CFileItemList items;
        items.Add(item);

        if (GetAdditionalAlbumDetails(parameterObject, items, musicdatabase) == OK)
          filled = true;
      }
    }
    else
    {
      CSong song;
      if (musicdatabase.GetSongByFileName(strFilename, song))
      {
        item->SetFromSong(song);
        FillItemArtistIDs(song.GetArtistIDArray(), item);

        CFileItemList items;
        items.Add(item);
        if (GetAdditionalSongDetails(parameterObject, items, musicdatabase) == OK)
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

bool CAudioLibrary::FillFileItemList(const CVariant &parameterObject, CFileItemList &list)
{
  CMusicDatabase musicdatabase;
  if (!musicdatabase.Open())
    return false;

  std::string file = parameterObject["file"].asString();
  int artistID = static_cast<int>(parameterObject["artistId"].asInteger(-1));
  int albumID = static_cast<int>(parameterObject["albumId"].asInteger(-1));
  int genreID = static_cast<int>(parameterObject["genreId"].asInteger(-1));

  // Gather into a list of our own. The sort below applies to what this call resolved, and
  // callers accumulate several items into one list - sorting theirs would reorder the items
  // they resolved earlier.
  CFileItemList resolved;

  bool success = false;
  CFileItemPtr fileItem(new CFileItem());
  if (FillFileItem(file, fileItem, parameterObject))
  {
    success = true;
    resolved.Add(fileItem);
  }

  if (artistID != -1 || albumID != -1 || genreID != -1)
    success |= musicdatabase.GetSongsNav(KODI::MUSIC::DB_PATH::SONGS, resolved, SortDescription(),
                                         genreID, artistID, albumID);

  int songID = static_cast<int>(parameterObject["songId"].asInteger(-1));
  if (songID != -1)
  {
    CSong song;
    if (musicdatabase.GetSong(songID, song))
    {
      resolved.Add(std::make_shared<CFileItem>(song));
      success = true;
    }
  }

  if (success)
  {
    // If we retrieved the list of songs by "artistId"
    // we sort by album (and implicitly by track number)
    if (artistID != -1)
      resolved.Sort(SortBy::ALBUM, SortOrder::ASCENDING,
                CServiceBroker::GetSettingsComponent()->GetSettings()->GetBool(
                    CSettings::SETTING_FILELISTS_IGNORETHEWHENSORTING)
                    ? SortAttributeIgnoreArticle
                    : SortAttributeNone);
    // If we retrieve the list of songs by "genreId"
    // we sort by artist (and implicitly by album and track number)
    else if (genreID != -1)
      resolved.Sort(SortBy::ARTIST, SortOrder::ASCENDING,
                    CServiceBroker::GetSettingsComponent()->GetSettings()->GetBool(
                        CSettings::SETTING_FILELISTS_IGNORETHEWHENSORTING)
                        ? SortAttributeIgnoreArticle
                        : SortAttributeNone);
    // otherwise we sort by track number
    else
      resolved.Sort(SortBy::TRACK_NUMBER, SortOrder::ASCENDING);

    list.Append(resolved);
  }

  return success;
}

void CAudioLibrary::FillItemArtistIDs(const std::vector<int>& artistids,
                                      std::shared_ptr<CFileItem>& item)
{
  // Add artistIds as separate property as not part of CMusicInfoTag
  CVariant artistidObj(CVariant::VariantTypeArray);
  for (const auto& artistid : artistids)
    artistidObj.push_back(artistid);

  item->SetProperty("artistid", artistidObj);
}

void CAudioLibrary::ApplyRoleFilter(const CVariant& parameterObject, CMusicDbUrl& url)
{
  const CVariant& filter = parameterObject["filter"];
  // any negative role id lifts the implicit roleid=1 (artist) that clients rely on
  if (parameterObject["allRoles"].isBoolean() && parameterObject["allRoles"].asBoolean())
    url.AddOption("roleid", -1000);
  else if (filter.isMember("roleId"))
    url.AddOption("roleid", static_cast<int>(filter["roleId"].asInteger()));
  else if (filter.isMember("role"))
    url.AddOption("role", filter["role"].asString());
}

void CAudioLibrary::FillAlbumItem(const CAlbum& album,
                                  const std::string& path,
                                  std::shared_ptr<CFileItem>& item)
{
  item = std::make_shared<CFileItem>(path, album);
  // Add album artistIds as separate property as not part of CMusicInfoTag
  std::vector<int> artistids = album.GetArtistIDArray();
  FillItemArtistIDs(artistids, item);
}

JSONRPC_STATUS CAudioLibrary::GetAdditionalDetails(const CVariant &parameterObject, CFileItemList &items)
{
  if (items.IsEmpty())
    return OK;

  CMusicDatabase musicdb;
  switch (MediaTypeFromName(items.GetContent()))
  {
    case KODI::MEDIA::TYPE::ARTIST:
      return GetAdditionalArtistDetails(parameterObject, items, musicdb);
    case KODI::MEDIA::TYPE::ALBUM:
      return GetAdditionalAlbumDetails(parameterObject, items, musicdb);
    case KODI::MEDIA::TYPE::SONG:
      return GetAdditionalSongDetails(parameterObject, items, musicdb);
    default:
      return OK;
  }
}

JSONRPC_STATUS CAudioLibrary::GetAdditionalArtistDetails(const CVariant& parameterObject,
                                                         const CFileItemList& items,
                                                         CMusicDatabase& musicdatabase)
{
  if (!musicdatabase.Open())
    return InternalError;

  std::set<std::string> checkProperties;
  checkProperties.insert("roles");
  checkProperties.insert("songGenres");
  checkProperties.insert("isAlbumArtist");
  checkProperties.insert("sourceId");
  std::set<std::string> additionalProperties;
  if (!CheckForAdditionalProperties(parameterObject["properties"], checkProperties, additionalProperties))
    return OK;

  if (additionalProperties.contains("roles"))
  {
    for (int i = 0; i < items.Size(); i++)
    {
      CFileItemPtr item = items[i];
      musicdatabase.GetRolesByArtist(item->GetMusicInfoTag()->GetDatabaseId(), item.get());
    }
  }
  if (additionalProperties.contains("songGenres"))
  {
    for (int i = 0; i < items.Size(); i++)
    {
      CFileItemPtr item = items[i];
      musicdatabase.GetGenresByArtist(item->GetMusicInfoTag()->GetDatabaseId(), item.get());
    }
  }
  if (additionalProperties.contains("isAlbumArtist"))
  {
    for (int i = 0; i < items.Size(); i++)
    {
      CFileItemPtr item = items[i];
      musicdatabase.GetIsAlbumArtist(item->GetMusicInfoTag()->GetDatabaseId(), item.get());
    }
  }
  if (additionalProperties.contains("sourceId"))
  {
    for (int i = 0; i < items.Size(); i++)
    {
      CFileItemPtr item = items[i];
      musicdatabase.GetSourcesByArtist(item->GetMusicInfoTag()->GetDatabaseId(), item.get());
    }
  }

  return OK;
}

JSONRPC_STATUS CAudioLibrary::GetAdditionalAlbumDetails(const CVariant& parameterObject,
                                                        const CFileItemList& items,
                                                        CMusicDatabase& musicdatabase)
{
  if (!musicdatabase.Open())
    return InternalError;

  std::set<std::string> checkProperties;
  checkProperties.insert("songGenres");
  checkProperties.insert("sourceId");
  std::set<std::string> additionalProperties;
  if (!CheckForAdditionalProperties(parameterObject["properties"], checkProperties, additionalProperties))
    return OK;

  if (additionalProperties.contains("songGenres"))
  {
    for (int i = 0; i < items.Size(); i++)
    {
      CFileItemPtr item = items[i];
      musicdatabase.GetGenresByAlbum(item->GetMusicInfoTag()->GetDatabaseId(), item.get());
    }
  }
  if (additionalProperties.contains("sourceId"))
  {
    for (int i = 0; i < items.Size(); i++)
    {
      CFileItemPtr item = items[i];
      musicdatabase.GetSourcesByAlbum(item->GetMusicInfoTag()->GetDatabaseId(), item.get());
    }
  }

  return OK;
}

JSONRPC_STATUS CAudioLibrary::GetAdditionalSongDetails(const CVariant& parameterObject,
                                                       const CFileItemList& items,
                                                       CMusicDatabase& musicdatabase)
{
  if (!musicdatabase.Open())
    return InternalError;

  std::set<std::string> checkProperties;
  checkProperties.insert("genreId");
  checkProperties.insert("sourceId");
  // Query (songview join songartistview) returns song.strAlbumArtists = CMusicInfoTag.m_strAlbumArtistDesc only
  // Actual album artist data, if required,  comes from album_artist and artist tables.
  // It may differ from just splitting album artist description string
  checkProperties.insert("albumArtist");
  checkProperties.insert("albumArtistId");
  checkProperties.insert("musicBrainzAlbumArtistId");
  std::set<std::string> additionalProperties;
  if (!CheckForAdditionalProperties(parameterObject["properties"], checkProperties, additionalProperties))
    return OK;

  for (int i = 0; i < items.Size(); i++)
  {
    CFileItemPtr item = items[i];
    if (additionalProperties.contains("genreId"))
    {
      std::vector<int> genreids;
      if (musicdatabase.GetGenresBySong(item->GetMusicInfoTag()->GetDatabaseId(), genreids))
      {
        CVariant genreidObj(CVariant::VariantTypeArray);
        for (const auto& genreid : genreids)
          genreidObj.push_back(genreid);

        item->SetProperty("genreid", genreidObj);
      }
    }
    if (additionalProperties.contains("sourceId"))
    {
      musicdatabase.GetSourcesBySong(item->GetMusicInfoTag()->GetDatabaseId(), item->GetPath(), item.get());
    }
    if (item->GetMusicInfoTag()->GetAlbumId() > 0)
    {
      if (additionalProperties.contains("albumArtist") ||
          additionalProperties.contains("albumArtistId") ||
          additionalProperties.contains("musicBrainzAlbumArtistId"))
      {
        musicdatabase.GetArtistsByAlbum(item->GetMusicInfoTag()->GetAlbumId(), item.get());
      }
    }
  }

  return OK;
}

JSONRPC_STATUS CAudioLibrary::RefreshArtist(const CVariant& parameterObject, CVariant& result)
{
  CMusicDatabase musicdatabase;
  if (!musicdatabase.Open())
    return InternalError;

  // Checking if artistID is a valid one
  const CVariant artistIdVariant{parameterObject["artistId"]};
  const auto artistID{static_cast<int>(artistIdVariant.asInteger())};
  if (const JSONRPC_STATUS status = StatusFor(musicdatabase.TryGetArtistExists(artistID), result,
                                              Target("artistId", parameterObject["artistId"]));
      status != OK)
    return status;

  // Start rescraping additional information for the given artist
  const std::string cmd = StringUtils::Format("musiclibrary.refreshartist({})",
                                              StringUtils::Paramify(artistIdVariant.asString()));
  CServiceBroker::GetAppMessenger()->SendMsg(TMSG_EXECUTE_BUILT_IN, -1, -1, nullptr, cmd);

  return ACK;
}

JSONRPC_STATUS CAudioLibrary::RefreshAlbum(const CVariant& parameterObject, CVariant& result)
{
  CMusicDatabase musicdatabase;
  if (!musicdatabase.Open())
    return InternalError;

  // Check if albumID is a valid one
  CAlbum album;
  const CVariant albumIdVariant{parameterObject["albumId"]};
  const int albumID = static_cast<int>(albumIdVariant.asInteger());
  if (const JSONRPC_STATUS status =
          StatusFor(musicdatabase.TryGetAlbum(albumID, album, false), result,
                    Target("albumId", parameterObject["albumId"]));
      status != OK)
    return status;

  // Start rescraping additional information for the given album
  const std::string cmd = StringUtils::Format("musiclibrary.refreshalbum({})",
                                              StringUtils::Paramify(albumIdVariant.asString()));
  CServiceBroker::GetAppMessenger()->SendMsg(TMSG_EXECUTE_BUILT_IN, -1, -1, nullptr, cmd);

  return ACK;
}

bool CAudioLibrary::ResolveInfoProviderView(const std::string& path,
                                            ADDON::ContentType& content,
                                            std::string& viewPath)
{
  CMusicDbUrl musicUrl;
  if (!musicUrl.FromString(path))
    return false;

  // Parsing folds an id the path spelled as a segment into the options, so both spellings of
  // the same listing are handled the same way from here on.
  std::string listing;
  std::string singleItem;
  if (StringUtils::EqualsNoCase(musicUrl.GetType(), KODI::MEDIA::CONTENT::ARTISTS))
  {
    content = ADDON::ContentType::ARTISTS;
    listing = KODI::MUSIC::DB_PATH::ARTISTS;
    singleItem = "artistid";
  }
  else if (StringUtils::EqualsNoCase(musicUrl.GetType(), KODI::MEDIA::CONTENT::ALBUMS))
  {
    content = ADDON::ContentType::ALBUMS;
    listing = KODI::MUSIC::DB_PATH::ALBUMS;
    singleItem = "albumid";
  }
  else
    return false;

  // The view is the listing, not the one item it may name, so the id naming a single item in
  // this listing goes. Every other option narrows the listing and stays: an albums view under
  // an artist is that artist's albums, and dropping its artistid would mean every album.
  CMusicDbUrl view;
  if (!view.FromString(listing))
    return false;

  view.AddOptions(musicUrl.GetOptionsString());
  view.RemoveOption(singleItem);
  viewPath = view.ToString();

  return true;
}

JSONRPC_STATUS CAudioLibrary::ResolveInfoProviderTarget(const CVariant& parameterObject,
                                                        CMusicDatabase& musicdatabase,
                                                        InfoProviderTarget& target,
                                                        CVariant& result)
{
  const std::string applyTo = parameterObject["applyTo"].asString();

  if (applyTo == "item")
  {
    const int artistId = static_cast<int>(parameterObject["artistId"].asInteger(-1));
    const int albumId = static_cast<int>(parameterObject["albumId"].asInteger(-1));
    if ((artistId > 0) == (albumId > 0))
      return InvalidParams;

    target.scope = InfoProviderTarget::Scope::Item;
    if (artistId > 0)
    {
      if (const JSONRPC_STATUS status =
              StatusFor(musicdatabase.TryGetArtistExists(artistId), result,
                        Target("artistId", parameterObject["artistId"]));
          status != OK)
        return status;

      target.content = ADDON::ContentType::ARTISTS;
      target.itemId = artistId;
    }
    else
    {
      CAlbum album;
      if (const JSONRPC_STATUS status =
              StatusFor(musicdatabase.TryGetAlbum(albumId, album, false), result,
                        Target("albumId", parameterObject["albumId"]));
          status != OK)
        return status;

      target.content = ADDON::ContentType::ALBUMS;
      target.itemId = albumId;
    }

    return OK;
  }

  if (applyTo == "view")
  {
    // The path's filter options are what SetScraperAll turns into a WHERE clause.
    if (!ResolveInfoProviderView(parameterObject["path"].asString(), target.content,
                                 target.viewPath))
      return InvalidParams;

    target.scope = InfoProviderTarget::Scope::View;
    return OK;
  }

  if (applyTo == "default")
  {
    // A default is what applies where nothing else does, so there has to be one to name.
    if (parameterObject["scraperId"].asString().empty())
      return InvalidParams;

    target.content = ADDON::TranslateContent(parameterObject["content"].asString());
    if (target.content != ADDON::ContentType::ARTISTS &&
        target.content != ADDON::ContentType::ALBUMS)
      return InvalidParams;

    target.scope = InfoProviderTarget::Scope::Default;
    target.viewPath = target.content == ADDON::ContentType::ARTISTS ? KODI::MUSIC::DB_PATH::ARTISTS
                                                                    : KODI::MUSIC::DB_PATH::ALBUMS;
    return OK;
  }

  return InvalidParams;
}

JSONRPC_STATUS CAudioLibrary::SetInfoProvider(const CVariant& parameterObject, CVariant& result)
{
  CMusicDatabase musicdatabase;
  if (!musicdatabase.Open())
    return InternalError;

  InfoProviderTarget target;
  if (const JSONRPC_STATUS status =
          ResolveInfoProviderTarget(parameterObject, musicdatabase, target, result);
      status != OK)
    return status;

  const std::string scraperId = parameterObject["scraperId"].asString();
  ADDON::ScraperPtr scraper;
  if (!scraperId.empty())
  {
    if (const JSONRPC_STATUS status =
            ResolveScraper(scraperId, target.content, parameterObject["scraperSettings"].asString(),
                           scraper, result);
        status != OK)
      return status;
  }

  bool written = false;
  switch (target.scope)
  {
    case InfoProviderTarget::Scope::Item:
      written = musicdatabase.SetScraper(target.itemId, target.content, scraper);
      break;

    case InfoProviderTarget::Scope::View:
      written = musicdatabase.SetScraperAll(target.viewPath, scraper);
      break;

    case InfoProviderTarget::Scope::Default:
    {
      // The dialog's default flow: the scraper's settings become its defaults, the setting
      // names it, and every override is cleared so that the default applies everywhere.
      scraper->SaveSettings();
      const std::shared_ptr<CSettings> settings =
          CServiceBroker::GetSettingsComponent()->GetSettings();
      settings->SetString(target.content == ADDON::ContentType::ARTISTS
                              ? CSettings::SETTING_MUSICLIBRARY_ARTISTSSCRAPER
                              : CSettings::SETTING_MUSICLIBRARY_ALBUMSSCRAPER,
                          scraper->ID());
      settings->Save();
      written = musicdatabase.SetScraperAll(target.viewPath, nullptr);
      break;
    }
  }
  if (!written)
    return InternalError;

  if (parameterObject["refresh"].asBoolean(false))
  {
    if (target.scope == InfoProviderTarget::Scope::Item)
    {
      const std::string cmd = StringUtils::Format(
          "musiclibrary.{}({})",
          target.content == ADDON::ContentType::ARTISTS ? "refreshartist" : "refreshalbum",
          target.itemId);
      CServiceBroker::GetAppMessenger()->SendMsg(TMSG_EXECUTE_BUILT_IN, -1, -1, nullptr, cmd);
    }
    else
      CMusicLibraryQueue::GetInstance().StartScan(target.content, target.viewPath, true);
  }

  return ACK;
}

bool CAudioLibrary::CheckForAdditionalProperties(const CVariant &properties, const std::set<std::string> &checkProperties, std::set<std::string> &foundProperties)
{
  if (!properties.isArray() || properties.empty())
    return false;

  std::set<std::string> checkingProperties = checkProperties;
  for (CVariant::const_iterator_array itr = properties.begin_array();
       itr != properties.end_array() && !checkingProperties.empty(); ++itr)
  {
    if (!itr->isString())
      continue;

    std::string property = itr->asString();
    if (checkingProperties.contains(property))
    {
      checkingProperties.erase(property);
      foundProperties.insert(property);
    }
  }

  return !foundProperties.empty();
}
