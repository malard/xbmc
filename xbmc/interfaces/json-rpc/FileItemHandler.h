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
#include "interfaces/IAnnouncer.h"

#include <memory>
#include <optional>
#include <set>
#include <span>

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
                                      const std::string& kind,
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

     \return NotFound, or InvalidParams when nothing better applies
     */
  static JSONRPC_STATUS DiagnoseUnresolvedItem(const CVariant& item);

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
                             const std::string& kind,
                             int id,
                             const CVariant& names,
                             const CVariant& item);

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
