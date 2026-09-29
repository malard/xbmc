/*
 *  Copyright (C) 2012-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "interfaces/IAnnouncer.h"
#include "utils/JSONVariantWriter.h"
#include "utils/Variant.h"

#include <cstdint>
#include <string>
#include <utility>

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
                                           bool compactOutput)
  {
    CVariant root;
    root["jsonrpc"] = "2.0";

    std::string name = method;
    CVariant payload = data;
    AsPropertiesChanged(flag, name, payload);
    if (!AsItemPropertiesChanged(flag, name, payload))
      return {};

    std::string namespaceMethod = ANNOUNCEMENT::AnnouncementFlagToString(flag);
    namespaceMethod += ".";
    namespaceMethod += name;
    root["method"] = namespaceMethod;

    root["params"]["data"] = payload;
    root["params"]["sender"] = sender;

    std::string str;
    CJSONVariantWriter::Write(root, str, compactOutput);

    return str;
  }

private:
  /*!
   \brief Sends a playback event that is only a change of speed or time as the player's
   OnPropertiesChanged, carrying what changed.

   The announcement keeps its name inside Kodi, where components react to it.
   */
  static void AsPropertiesChanged(ANNOUNCEMENT::AnnouncementFlag flag,
                                  std::string& method,
                                  CVariant& data)
  {
    if (flag != ANNOUNCEMENT::Player)
      return;

    const CVariant& player = data["player"];
    CVariant properties(CVariant::VariantTypeObject);
    if (method == "OnPause" || method == "OnResume" || method == "OnSpeedChanged")
      properties["speed"] = player["speed"];
    else if (method == "OnSeek")
      properties["time"] = player["time"];
    else
      return;

    CVariant changed(CVariant::VariantTypeObject);
    changed["properties"] = std::move(properties);
    changed["player"]["players"] = player["players"];
    method = "OnPropertiesChanged";
    data = std::move(changed);
  }

  /*!
   \brief Sends a library update as the item's OnItemPropertiesChanged, carrying the properties
   it names under the names GetItemProperties answers with, when it names any.

   The announcement keeps its name inside Kodi, where components react to it.

   \return false for an update to no library item, which is not sent
   */
  static bool AsItemPropertiesChanged(ANNOUNCEMENT::AnnouncementFlag flag,
                                      std::string& method,
                                      CVariant& data)
  {
    if ((flag != ANNOUNCEMENT::VideoLibrary && flag != ANNOUNCEMENT::AudioLibrary) ||
        method != "OnUpdate")
      return true;

    const CVariant& item = data.isMember("item") ? data["item"] : data;
    const int64_t id = item["id"].asInteger(-1);
    if (id <= 0)
      return false;

    CVariant changed(CVariant::VariantTypeObject);
    changed["item"]["kind"] = item["type"];
    changed["item"]["id"] = id;
    if (data.isMember("properties"))
      changed["properties"] = data["properties"];
    else if (data.isMember("playcount"))
      changed["properties"]["playCount"] = data["playcount"];
    for (const char* marker : {"transaction", "added"})
    {
      if (data.isMember(marker))
        changed[marker] = data[marker];
    }

    method = "OnItemPropertiesChanged";
    data = std::move(changed);
    return true;
  }
};
} // namespace JSONRPC
