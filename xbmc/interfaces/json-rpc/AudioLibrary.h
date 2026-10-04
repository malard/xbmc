/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "FileItemHandler.h"
#include "JSONRPC.h"
#include "addons/Scraper.h"
#include "media/MediaType.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

class CAlbum;
class CFileitem;
class CFileitemList;
class CMusicDatabase;
class CMusicDbUrl;
class CVariant;

namespace JSONRPC
{
//! The kinds of item the music library holds
enum class AudioKind
{
  Artist,
  Album,
  Song,
};

class CAudioLibrary : public CFileItemHandler
  {
  public:
    static JSONRPC_STATUS GetProperties(const CVariant& parameterObject, CVariant& result);

  //! Whether the library holds items of \p type
  static bool IsItemKind(KODI::MEDIA::MediaType type);

  //! The query over one kind's items
  static JSONRPC_STATUS GetItems(const CVariant& parameterObject, CVariant& result);

  //! A list method: the query over \p Kind with the listing \p From
  template<AudioKind Kind, Listing From = Listing::All>
  static JSONRPC_STATUS List(const CVariant &parameterObject, CVariant &result);
    static JSONRPC_STATUS GetItemProperties(const CVariant &parameterObject, CVariant &result);
    static JSONRPC_STATUS SetItemProperties(const CVariant &parameterObject, CVariant &result);
    static JSONRPC_STATUS GetGenres(const CVariant &parameterObject, CVariant &result);
    static JSONRPC_STATUS GetRoles(const CVariant &parameterObject, CVariant &result);
    static JSONRPC_STATUS GetSources(const CVariant &parameterObject, CVariant &result);
    static JSONRPC_STATUS GetAvailableArtTypes(const CVariant& parameterObject, CVariant& result);
    static JSONRPC_STATUS GetAvailableArt(const CVariant &parameterObject, CVariant &result);

    static JSONRPC_STATUS Scan(const CVariant &parameterObject, CVariant &result);
    static JSONRPC_STATUS Export(const CVariant &parameterObject, CVariant &result);
    static JSONRPC_STATUS Clean(const CVariant &parameterObject, CVariant &result);

    static bool FillFileItem(
        const std::string& strFilename,
        std::shared_ptr<CFileItem>& item,
        const CVariant& parameterObject = CVariant(CVariant::VariantTypeArray));
    static bool FillFileItemList(const CVariant &parameterObject, CFileItemList &list);

    static JSONRPC_STATUS GetAdditionalDetails(const CVariant &parameterObject, CFileItemList &items);
    static JSONRPC_STATUS GetAdditionalArtistDetails(const CVariant& parameterObject,
                                                     const CFileItemList& items,
                                                     CMusicDatabase& musicdatabase);
    static JSONRPC_STATUS GetAdditionalAlbumDetails(const CVariant& parameterObject,
                                                    const CFileItemList& items,
                                                    CMusicDatabase& musicdatabase);
    static JSONRPC_STATUS GetAdditionalSongDetails(const CVariant& parameterObject,
                                                   const CFileItemList& items,
                                                   CMusicDatabase& musicdatabase);
    static JSONRPC_STATUS RefreshArtist(const CVariant& parameterObject,
                                        CVariant& result);
    static JSONRPC_STATUS RefreshAlbum(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS SetInfoProvider(const CVariant& parameterObject, CVariant& result);

protected:
  /*!
     \brief Resolves the listing an information provider is being applied to.

     Drops the id that names a single item within the listing, and keeps every other filter
     the path carries, however the path spelled it. False for anything but a musicdb://
     artists or albums listing.
     */
  static bool ResolveInfoProviderView(const std::string& path,
                                      ADDON::ContentType& content,
                                      std::string& viewPath);

  //! What SetInfoProvider's "applyTo" names, and the scope's own parameters.
  struct InfoProviderTarget
  {
    enum class Scope
    {
      Item, //!< one artist or album, named by itemId
      View, //!< the listing at viewPath
      Default, //!< the content type's default provider, and viewPath is every row of it
    };

    Scope scope{Scope::Item};
    ADDON::ContentType content{ADDON::ContentType::NONE};
    int itemId{-1};
    std::string viewPath;
  };

  /*!
     \brief Reads SetInfoProvider's scope parameters into the target it names.

     Answers NotFound for an item that does not exist, so the caller does not reach the
     database again to find that out.
     */
  static JSONRPC_STATUS ResolveInfoProviderTarget(const CVariant& parameterObject,
                                                  CMusicDatabase& musicdatabase,
                                                  InfoProviderTarget& target,
                                                  CVariant& result);

private:
  /*!
     \brief Lists the items of \p kind that \p listing selects
     \param parameterObject The caller's properties, limits, sort and filter, and the options
     that narrow the kind's list
     */
  static JSONRPC_STATUS Query(AudioKind kind,
                              Listing listing,
                                       const CVariant& parameterObject,
                                       CVariant& result);

  //! Answers the \p fields of the item of \p kind with \p id
  static JSONRPC_STATUS ReadItem(AudioKind kind,
                                 int id,
                                 const CVariant& fields,
                                 CMusicDatabase& musicdatabase,
                                 CVariant& result);

  //! Store each of \p properties given a value on the item of their kind with \p id
  static JSONRPC_STATUS SetArtistDetails(int id,
                                         const CVariant& properties,
                                         CMusicDatabase& musicdatabase,
                                         CVariant& result);
  static JSONRPC_STATUS SetAlbumDetails(int id,
                                        const CVariant& properties,
                                        CMusicDatabase& musicdatabase,
                                        CVariant& result);
  static JSONRPC_STATUS SetSongDetails(int id,
                                       const CVariant& properties,
                                       CMusicDatabase& musicdatabase,
                                       CVariant& result);

  //! Adds the art and fanart the JSON listing leaves out to each item of \p list
  static void FillListArt(CVariant& list,
                          const std::set<std::string, std::less<>>& fields,
                          const char* idName,
                          KODI::MEDIA::MediaType mediaType);

  //! Narrows \p url to the artists in the role the caller's filter names, or to every role
  static void ApplyRoleFilter(const CVariant& parameterObject, CMusicDbUrl& url);

  static void FillAlbumItem(const CAlbum& album,
                              const std::string& path,
                              std::shared_ptr<CFileItem>& item);
    static void FillItemArtistIDs(const std::vector<int>& artistids,
                                  std::shared_ptr<CFileItem>& item);

    static bool CheckForAdditionalProperties(const CVariant &properties, const std::set<std::string> &checkProperties, std::set<std::string> &foundProperties);
  };

template<AudioKind Kind, Listing From>
JSONRPC_STATUS CAudioLibrary::List(const CVariant& parameterObject, CVariant& result)
{
  return Query(Kind, From, parameterObject, result);
}
} // namespace JSONRPC
