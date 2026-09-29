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

#include <string_view>

class CFileItemList;
class CVariant;

namespace JSONRPC
{
class CPlaylistOperations : public CFileItemHandler
{
public:
  static JSONRPC_STATUS GetPlaylists(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS GetProperties(const CVariant& parameterObject, CVariant& result);

  static JSONRPC_STATUS GetItems(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS Add(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS Remove(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS Insert(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS Clear(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS Swap(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS SetShuffle(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS SetRepeat(const CVariant& parameterObject, CVariant& result);

private:
  /*!
     * \brief Read the items a request names, as the media the playlist takes. What resolved
     * arrives in items, in request order; everything else arrives in unresolved, each with a
     * "reason" of notfound, unavailable or invalid.
     */
  static void ReadItems(std::string_view media,
                        const CVariant& itemParam,
                        CFileItemList& items,
                        CVariant& unresolved);
  /*!
     * \brief The failure of a call that added nothing. A reference that no longer resolves is
     * NotFound, for the reason the first such item gives; only when every item was malformed is
     * the request itself at fault.
     */
  static JSONRPC_STATUS NothingAdded(const CVariant& unresolved, CVariant& result);
};
} // namespace JSONRPC
