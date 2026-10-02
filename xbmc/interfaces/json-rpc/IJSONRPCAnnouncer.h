/*
 *  Copyright (C) 2012-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "interfaces/IAnnouncer.h"
#include "media/MediaType.h"
#include "utils/JSONVariantWriter.h"
#include "utils/Variant.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
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
    if (!AsItemNotification(flag, name, payload))
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
   \brief Sends a library item's announcement as the item's own notification: an update as its
   OnItemAdded when it adds the item and otherwise as its OnItemPropertiesChanged, carrying the
   properties it names under the names GetItemProperties answers with, when it names any, and a
   removal as its OnItemRemoved.

   The announcement keeps its name inside Kodi, where components react to it.

   \return false for an announcement about no item of the library, which is not sent
   */
  static bool AsItemNotification(ANNOUNCEMENT::AnnouncementFlag flag,
                                 std::string& method,
                                 CVariant& data)
  {
    using KODI::MEDIA::MediaType;
    static constexpr std::array VIDEO_KINDS{MediaType::MOVIE,   MediaType::VIDEO_COLLECTION,
                                            MediaType::TV_SHOW, MediaType::SEASON,
                                            MediaType::EPISODE, MediaType::MUSIC_VIDEO};
    static constexpr std::array AUDIO_KINDS{MediaType::ARTIST, MediaType::ALBUM, MediaType::SONG};

    if ((flag != ANNOUNCEMENT::VideoLibrary && flag != ANNOUNCEMENT::AudioLibrary) ||
        (method != "OnUpdate" && method != "OnRemove"))
      return true;

    const CVariant& item = data.isMember("item") ? data["item"] : data;
    const int64_t id = item["id"].asInteger(-1);
    const std::string kind = item["type"].asString();
    const auto isNamed = [&kind](MediaType type) { return KODI::MEDIA::NameOf(type) == kind; };
    const bool isKind = flag == ANNOUNCEMENT::VideoLibrary
                            ? std::ranges::any_of(VIDEO_KINDS, isNamed)
                            : std::ranges::any_of(AUDIO_KINDS, isNamed);
    if (id <= 0 || !isKind)
      return false;

    const bool removed = method == "OnRemove";
    const bool added = !removed && data["added"].asBoolean(false);
    CVariant changed(CVariant::VariantTypeObject);
    changed["item"]["kind"] = kind;
    changed["item"]["id"] = id;
    if (!removed && !added)
    {
      if (data.isMember("properties"))
        changed["properties"] = data["properties"];
      else if (data.isMember("playcount"))
        changed["properties"]["playCount"] = data["playcount"];
    }
    if (data.isMember("transaction"))
      changed["transaction"] = data["transaction"];

    method = removed ? "OnItemRemoved" : added ? "OnItemAdded" : "OnItemPropertiesChanged";
    data = std::move(changed);
    return true;
  }
};
} // namespace JSONRPC
