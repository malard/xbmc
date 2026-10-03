/*
 *  Copyright (C) 2012-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "interfaces/IAnnouncer.h"

#include <string>

class CVariant;

namespace JSONRPC
{
class IJSONRPCAnnouncer : public ANNOUNCEMENT::IAnnouncer
{
public:
  ~IJSONRPCAnnouncer() override = default;

protected:
  //! \return the notification, or nothing when the announcement is not one clients receive
  static std::string AnnouncementToJSONRPC(ANNOUNCEMENT::AnnouncementFlag flag,
                                           const std::string& sender,
                                           const std::string& method,
                                           const CVariant& data,
                                           bool compactOutput);

private:
  /*!
   \brief Sends a playback event that is only a change of speed or time as the player's
   OnPropertiesChanged, carrying what changed.

   The announcement keeps its name inside Kodi, where components react to it.
   */
  static void AsPropertiesChanged(ANNOUNCEMENT::AnnouncementFlag flag,
                                  std::string& method,
                                  CVariant& data);

  /*!
   \brief Sends a library item's announcement as the item's own notification: an update as its
   OnItemAdded when it adds the item and otherwise as its OnItemPropertiesChanged, carrying the
   properties it names under the names GetItemProperties answers with, when it names any, and a
   removal as its OnItemRemoved.

   The announcement keeps its name inside Kodi, where components react to it.

   \return false for an announcement about no item of the library, which is not sent
   */
  static bool AsItemNotification(ANNOUNCEMENT::AnnouncementFlag flag,
                                 std::string& method,
                                 CVariant& data);
};
} // namespace JSONRPC
