/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "JSONRPC.h"
#include "JSONUtils.h"
#include "imagefiles/ImageFileURL.h"
#include "interfaces/IAnnouncer.h"
#include "media/MediaType.h"
#include "utils/StringUtils.h"
#include "utils/Variant.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>

class CDbUrl;
class CFileItem;
class CFileItemList;
class CThumbLoader;
class CVariant;
class ISerializable;

namespace PVR
{
class CPVRRecording;
class CPVRTimerInfoTag;
} // namespace PVR

namespace JSONRPC
{
//! Which of a kind's items a library list answers with
enum class Listing
{
  All, //!< those the caller's filter, sort and limits select
  RecentlyAdded,
  RecentlyPlayed,
  InProgress,
};

//! What a client names a library's kind by, and the types of its items
template<typename K>
struct LibraryKind
{
  using Kind = K;

  Kind kind;
  KODI::MEDIA::MediaType type; //!< the media type, whose name the kind goes by on the wire
  const char* id;
  const char* fields;
  const char* settable;
};

//! A library's kinds, each described by a \p Traits, and the database that holds their items
template<typename Traits, typename Database>
class LibraryKinds
{
public:
  using Kind = typename Traits::Kind;

  constexpr explicit LibraryKinds(std::span<const Traits> kinds) : m_kinds(kinds) {}

  const Traits& Of(Kind kind) const { return *std::ranges::find(m_kinds, kind, &Traits::kind); }

  //! The kind a client names \p name, nullptr for none
  const Traits* Named(std::string_view name) const
  {
    const auto traits = std::ranges::find(m_kinds, KODI::MEDIA::MediaTypeOf(name), &Traits::type);
    return traits == m_kinds.end() ? nullptr : &*traits;
  }

  //! Whether the library holds items of \p type
  bool Holds(KODI::MEDIA::MediaType type) const
  {
    return std::ranges::find(m_kinds, type, &Traits::type) != m_kinds.end();
  }

  //! The error target for an item, in the addressing the caller used
  CVariant ItemTarget(Kind kind, int id) const
  {
    CVariant item(CVariant::VariantTypeObject);
    item["kind"] = KODI::MEDIA::NameOf(Of(kind).type);
    item["id"] = id;
    return Target("item", item);
  }

private:
  std::span<const Traits> m_kinds;
};

class CFileItemHandler : public CJSONUtils
{
public:
  //! A list filter naming one field's value, and the database URL option it sets
  struct FilterField
  {
    static constexpr FilterField Text(const char* name, const char* option = nullptr)
    {
      return {name, option ? option : name, false};
    }
    static constexpr FilterField Number(const char* name, const char* option = nullptr)
    {
      return {name, option ? option : name, true};
    }

    const char* name;
    const char* option;
    bool number;
  };

  /*!
     \brief Validates \p value against a type of the service description

     A parameter declared as any of several kinds' types is checked again against the one type
     of the kind the caller named.

     \param parameter The parameter \p value was given as, named in the error data
     \param checked The value with every omitted member filled in
     \param errorData Why the value does not validate, in the shape the validator reports
     */
  static JSONRPC_STATUS CheckAgainstType(const char* type,
                                         const char* parameter,
                                         const CVariant& value,
                                         CVariant& checked,
                                         CVariant& errorData);

  //! Refuses \p parameter, which means nothing for the kind the caller named
  static JSONRPC_STATUS RefuseForKind(const char* parameter,
                                      KODI::MEDIA::MediaType kind,
                                      CVariant& errorData);

protected:
  static void FillDetails(const ISerializable* info,
                          const std::shared_ptr<CFileItem>& item,
                          std::set<std::string>& fields,
                          CVariant& result,
                          CThumbLoader* thumbLoader = nullptr);
  static void HandleFileItemList(const char* ID,
                                 bool allowFile,
                                 const char* resultname,
                                 CFileItemList& items,
                                 const CVariant& parameterObject,
                                 CVariant& result,
                                 bool sortLimit = true);
  static void HandleFileItemList(const char* ID,
                                 bool allowFile,
                                 const char* resultname,
                                 CFileItemList& items,
                                 const CVariant& parameterObject,
                                 CVariant& result,
                                 int size,
                                 bool sortLimit = true);
  static void HandleFileItem(const char* ID,
                             bool allowFile,
                             const char* resultname,
                             const std::shared_ptr<CFileItem>& item,
                             const CVariant& parameterObject,
                             const CVariant& validFields,
                             CVariant& result,
                             bool append = true,
                             CThumbLoader* thumbLoader = nullptr);
  static void HandleFileItem(const char* ID,
                             bool allowFile,
                             const char* resultname,
                             const std::shared_ptr<CFileItem>& item,
                             const CVariant& parameterObject,
                             const std::set<std::string>& validFields,
                             CVariant& result,
                             bool append = true,
                             CThumbLoader* thumbLoader = nullptr);

  static bool FillFileItemList(const CVariant& parameterObject, CFileItemList& list);

  /*!
     \brief Narrows \p url by the caller's filter: the first of \p fields it names, else its rules
     \param rulesType The smart playlist type the rules are written for
     \return false if the rules do not parse
     */
  static bool ApplyFilter(const CVariant& filter,
                          std::span<const FilterField> fields,
                          const std::string& rulesType,
                          CDbUrl& url);

  /*!
     \brief Diagnoses an item FillFileItemList dropped without saying why

     Bypasses the caches so a cached hit cannot mask storage that has gone away.

     \return NotFound, or InvalidParams as not-a-file or, when nothing better applies,
     not-playable
     */
  static JSONRPC_STATUS DiagnoseUnresolvedItem(const CVariant& item, CVariant& result);

  //! Moves the list a query answered under \p from to \p to, as an empty list when there is none
  static void RenameList(CVariant& result, const char* from, const char* to);

  /*!
     \brief The members of \p object given a value

     An object declared as any of several kinds' types arrives with the members of whichever
     type it matched first filled in as null; without them it can be checked against its own.
     */
  static CVariant GivenMembers(const CVariant& object);

  //! The members of \p values given a value that a client can read back through \p fieldsType
  static CVariant ReadableNames(const CVariant& values, const char* fieldsType);

  //! Announces the \p names of the item of \p kind with \p id changed, to the values in \p item
  static void AnnounceChange(ANNOUNCEMENT::AnnouncementFlag library,
                             KODI::MEDIA::MediaType kind,
                             int id,
                             const CVariant& names,
                             const CVariant& item);

  /*!
     \brief A library's GetItems: \p query over the kind the caller named
     \param check Checks the parameters against the kind, filling in what was omitted
     */
  template<typename Traits, typename Database>
  static JSONRPC_STATUS GetItemsIn(
      const LibraryKinds<Traits, Database>& kinds,
      JSONRPC_STATUS (*check)(const Traits&, const CVariant&, CVariant&, CVariant&),
      JSONRPC_STATUS (*query)(typename Traits::Kind, Listing, const CVariant&, CVariant&),
      const CVariant& parameterObject,
      CVariant& result)
  {
    const Traits* traits = kinds.Named(parameterObject["kind"].asString());
    if (!traits)
      return InvalidParams;

    CVariant checked(parameterObject);
    if (const JSONRPC_STATUS status = check(*traits, parameterObject, checked, result);
        status != OK)
      return status;

    return query(traits->kind, Listing::All, checked, result);
  }

  //! A library's GetItemProperties: \p readItem answers the item the caller named
  template<typename Traits, typename Database>
  static JSONRPC_STATUS GetItemPropertiesIn(
      const LibraryKinds<Traits, Database>& kinds,
      JSONRPC_STATUS (*readItem)(typename Traits::Kind, int, const CVariant&, Database&, CVariant&),
      const CVariant& parameterObject,
      CVariant& result)
  {
    const Traits* traits = kinds.Named(parameterObject["item"]["kind"].asString());
    if (!traits)
      return InvalidParams;

    CVariant fields;
    if (const JSONRPC_STATUS status = CheckAgainstType(
            traits->fields, "properties", parameterObject["properties"], fields, result);
        status != OK)
      return status;

    Database database;
    if (!database.Open())
      return InternalError;

    return readItem(traits->kind, static_cast<int>(parameterObject["item"]["id"].asInteger()),
                    fields, database, result);
  }

  //! A library's GetAvailableArtTypes
  template<typename Traits, typename Database>
  static JSONRPC_STATUS GetAvailableArtTypesIn(const LibraryKinds<Traits, Database>& kinds,
                                               const CVariant& parameterObject,
                                               CVariant& result)
  {
    const Traits* traits = kinds.Named(parameterObject["item"]["kind"].asString());
    if (!traits)
      return InvalidParams;

    Database database;
    if (!database.Open())
      return InternalError;

    CVariant availablearttypes = CVariant(CVariant::VariantTypeArray);
    for (const auto& artType : database.GetAvailableArtTypesForItem(
             static_cast<int>(parameterObject["item"]["id"].asInteger()), traits->type))
      availablearttypes.append(artType);

    result = CVariant(CVariant::VariantTypeObject);
    result["availableArtTypes"] = availablearttypes;
    return OK;
  }

  //! A library's GetAvailableArt
  template<typename Traits, typename Database>
  static JSONRPC_STATUS GetAvailableArtIn(const LibraryKinds<Traits, Database>& kinds,
                                          const CVariant& parameterObject,
                                          CVariant& result)
  {
    const Traits* traits = kinds.Named(parameterObject["item"]["kind"].asString());
    if (!traits)
      return InvalidParams;

    std::string artType = parameterObject["artType"].asString();
    StringUtils::ToLower(artType);

    Database database;
    if (!database.Open())
      return InternalError;

    CVariant availableart = CVariant(CVariant::VariantTypeArray);
    for (const auto& artentry : database.GetAvailableArtForItem(
             static_cast<int>(parameterObject["item"]["id"].asInteger()), traits->type, artType))
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

private:
  static void Sort(CFileItemList& items, const CVariant& parameterObject);
  /*!
     \param epgRecording The item's EPG recording, looked up once; engaged and null when none
     \param epgTimer The item's EPG timer, looked up once; engaged and null when none
     */
  static bool GetField(const std::string& field,
                       const CVariant& info,
                       const std::shared_ptr<CFileItem>& item,
                       CVariant& result,
                       bool& fetchedArt,
                       std::optional<std::shared_ptr<PVR::CPVRRecording>>& epgRecording,
                       std::optional<std::shared_ptr<PVR::CPVRTimerInfoTag>>& epgTimer,
                       CThumbLoader* thumbLoader = nullptr);
};
} // namespace JSONRPC
