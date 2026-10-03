/*
 *  Copyright (C) 2016-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "FileItemHandler.h"
#include "JSONRPC.h"
#include "XBDateTime.h"
#include "utils/Artwork.h"
#include "utils/DatabaseUtils.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

class CFileItem;
class CFileItemList;
class CVideoDatabase;
class CVariant;

namespace JSONRPC
{
//! The kinds of item the video library holds
enum class VideoKind
{
  Movie,
  Set,
  TVShow,
  Season,
  Episode,
  MusicVideo,
};

class CVideoLibrary : public CFileItemHandler
{
public:
  //! Whether the library holds items of \p type
  static bool IsItemKind(KODI::MEDIA::MediaType type);

  //! The query over one kind's items
  static JSONRPC_STATUS GetItems(const CVariant& parameterObject, CVariant& result);

  //! A list method: the query over \p Kind with the listing \p From
  template<VideoKind Kind, Listing From = Listing::All>
  static JSONRPC_STATUS List(const CVariant& parameterObject, CVariant& result);

  static JSONRPC_STATUS GetItemProperties(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS SetItemProperties(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS AddItem(const CVariant& parameterObject, CVariant& result);

  static JSONRPC_STATUS GetGenres(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS GetTags(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS GetAvailableArtTypes(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS GetAvailableArt(const CVariant& parameterObject, CVariant& result);

  static JSONRPC_STATUS Refresh(const CVariant& parameterObject, CVariant& result);

  // Deprecated in favour of Refresh, which also reaches movie sets and seasons
  static JSONRPC_STATUS RefreshMovie(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS RefreshTVShow(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS RefreshEpisode(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS RefreshMusicVideo(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS RefreshContentGeometry(const CVariant& parameterObject, CVariant& result);

  static JSONRPC_STATUS RemoveMovie(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS RemoveTVShow(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS RemoveEpisode(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS RemoveMusicVideo(const CVariant& parameterObject, CVariant& result);

  static JSONRPC_STATUS Scan(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS SetSourceContent(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS Export(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS Clean(const CVariant& parameterObject, CVariant& result);

  static bool FillFileItem(const std::string& strFilename,
                           std::shared_ptr<CFileItem>& item,
                           const CVariant& parameterObject = CVariant(CVariant::VariantTypeArray));
  static bool FillFileItemList(const CVariant& parameterObject, CFileItemList& list);

protected:
  //! Adds the files table's playback state to an item that already says what it is.
  static void ApplyPlaybackState(const CVideoInfoTag& fileDetails, CVideoInfoTag& details);

  struct PlaybackUpdate
  {
    int playCount;
    CDateTime lastPlayed;
  };

  /*! \brief The playback state a show-level update leaves one of its episodes with.
     \return what to store, or nothing when the episode is left as it is
    */
  static std::optional<PlaybackUpdate> EpisodePlaybackUpdate(const CVideoInfoTag& show,
                                                             bool updatePlaycount,
                                                             bool updateLastplayed,
                                                             const CVideoInfoTag& episode);

  //! What a Set*Details call changes about an item's artwork, and which details it names
  struct DetailsEdit
  {
    KODI::ART::Artwork artwork;
    std::set<std::string, std::less<>> removedArtwork;
    std::set<std::string, std::less<>> updatedDetails;
  };

  //! Applies the caller's edit to \p details and to the item's stored artwork.
  static DetailsEdit EditDetails(const CVariant& parameterObject,
                                 CVideoInfoTag& details,
                                 CVideoDatabase& videodatabase);

  //! Stores the playcount, last played date and resume point an edit changed from \p before.
  static void StorePlaybackEdit(const CVariant& parameterObject,
                                const PlaybackUpdate& before,
                                CVideoInfoTag& details,
                                CVideoDatabase& videodatabase);

public:
  static void UpdateResumePoint(const CVariant& parameterObject,
                                CVideoInfoTag& details,
                                CVideoDatabase& videodatabase);

  /*! \brief Provided the JSON-RPC parameter object compute the VideoDbDetails mask
    * \param parameterObject the JSON parameter mask
    * \return the mask value for the requested properties
    */
  static int GetDetailsFromJsonParameters(const CVariant& parameterObject);

private:
  /*!
     \brief Lists the items of \p kind that \p listing selects
     \param parameterObject The caller's properties, limits, sort and filter, and the show and
     season an episode or season list is narrowed to
     */
  static JSONRPC_STATUS Query(VideoKind kind,
                              Listing listing,
                              const CVariant& parameterObject,
                              CVariant& result);

  //! Answers the \p fields of the item of \p kind with \p id
  static JSONRPC_STATUS ReadItem(VideoKind kind,
                                 int id,
                                 const CVariant& fields,
                                 CVideoDatabase& videodatabase,
                                 CVariant& result);

  /*! \brief Stores \p infos, the item of their kind with \p id after \p edit
     \param before the item's playback state before the edit
     */
  static JSONRPC_STATUS SetMovieDetails(int id,
                                        const CVariant& properties,
                                        const PlaybackUpdate& before,
                                        const DetailsEdit& edit,
                                        CVideoInfoTag& infos,
                                        CVideoDatabase& videodatabase,
                                        CVariant& result);
  static JSONRPC_STATUS SetMovieSetDetails(int id,
                                           const CVariant& properties,
                                           const PlaybackUpdate& before,
                                           const DetailsEdit& edit,
                                           CVideoInfoTag& infos,
                                           CVideoDatabase& videodatabase,
                                           CVariant& result);
  static JSONRPC_STATUS SetTVShowDetails(int id,
                                         const CVariant& properties,
                                         const PlaybackUpdate& before,
                                         const DetailsEdit& edit,
                                         CVideoInfoTag& infos,
                                         CVideoDatabase& videodatabase,
                                         CVariant& result);
  static JSONRPC_STATUS SetSeasonDetails(int id,
                                         const CVariant& properties,
                                         const PlaybackUpdate& before,
                                         const DetailsEdit& edit,
                                         CVideoInfoTag& infos,
                                         CVideoDatabase& videodatabase,
                                         CVariant& result);
  static JSONRPC_STATUS SetEpisodeDetails(int id,
                                          const CVariant& properties,
                                          const PlaybackUpdate& before,
                                          const DetailsEdit& edit,
                                          CVideoInfoTag& infos,
                                          CVideoDatabase& videodatabase,
                                          CVariant& result);
  static JSONRPC_STATUS SetMusicVideoDetails(int id,
                                             const CVariant& properties,
                                             const PlaybackUpdate& before,
                                             const DetailsEdit& edit,
                                             CVideoInfoTag& infos,
                                             CVideoDatabase& videodatabase,
                                             CVariant& result);

  static int RequiresAdditionalDetails(KODI::MEDIA::MediaType mediaType,
                                       const CVariant& parameterObject);
  static JSONRPC_STATUS HandleItems(const char* idProperty,
                                    CFileItemList& items,
                                    const CVariant& parameterObject,
                                    CVariant& result,
                                    bool limit = true);
  static JSONRPC_STATUS RemoveVideo(const CVariant& parameterObject);

  //! A deprecated refresh method: refreshes the item its kind's id member names
  static JSONRPC_STATUS RefreshById(const CVariant& parameterObject, CVariant& result);

  /*! \brief Queues a refresh of the item of \p kind with \p id
     \param target The item as the caller named it, for a failure to name
     */
  static JSONRPC_STATUS RefreshVideo(VideoKind kind,
                                     int id,
                                     const CVariant& target,
                                     const CVariant& parameterObject,
                                     CVariant& result);

  static JSONRPC_STATUS ResolveRefreshItem(VideoKind kind,
                                           int id,
                                           const CVariant& target,
                                           CVideoDatabase& videodatabase,
                                           CFileItem& item,
                                           CVariant& result);
  static void UpdateVideoTag(const CVariant& parameterObject,
                             CVideoInfoTag& details,
                             KODI::ART::Artwork& artwork,
                             std::set<std::string, std::less<>>& removedArtwork,
                             std::set<std::string, std::less<>>& updatedDetails);
  static void UpdateVideoTagField(const CVariant& parameterObject,
                                  const std::string& fieldName,
                                  std::vector<std::string>& fieldValue,
                                  std::set<std::string, std::less<>>& updatedDetails);
};

template<VideoKind Kind, Listing From>
JSONRPC_STATUS CVideoLibrary::List(const CVariant& parameterObject, CVariant& result)
{
  return Query(Kind, From, parameterObject, result);
}
} // namespace JSONRPC
