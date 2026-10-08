/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "IJSONRPCAnnouncer.h"

#include "AudioLibrary.h"
#include "VideoLibrary.h"
#include "interfaces/AnnouncementEvents.h"
#include "interfaces/AnnouncementMessages.h"
#include "media/MediaType.h"
#include "utils/JSONVariantWriter.h"
#include "utils/Variant.h"

#include <cstdint>
#include <utility>

namespace JSONRPC
{

std::string IJSONRPCAnnouncer::AnnouncementToJSONRPC(const ANNOUNCEMENT::Announcement& announcement,
                                                     bool compactOutput)
{
  return AnnouncementToJSONRPC(ANNOUNCEMENT::FlagOf(announcement),
                               ANNOUNCEMENT::SenderOf(announcement),
                               ANNOUNCEMENT::MessageOf(announcement),
                               ANNOUNCEMENT::NotificationDataOf(announcement), compactOutput);
}

std::string IJSONRPCAnnouncer::AnnouncementToJSONRPC(ANNOUNCEMENT::AnnouncementFlag flag,
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

void IJSONRPCAnnouncer::AsPropertiesChanged(ANNOUNCEMENT::AnnouncementFlag flag,
                                            std::string& method,
                                            CVariant& data)
{
  if (flag != ANNOUNCEMENT::Player)
    return;

  const CVariant& player = data["player"];
  CVariant properties(CVariant::VariantTypeObject);
  if (method == ANNOUNCEMENT::MESSAGE::ON_PAUSE || method == ANNOUNCEMENT::MESSAGE::ON_RESUME ||
      method == ANNOUNCEMENT::MESSAGE::ON_SPEED_CHANGED)
    properties["speed"] = player["speed"];
  else if (method == ANNOUNCEMENT::MESSAGE::ON_SEEK)
    properties["time"] = player["time"];
  else
    return;

  CVariant changed(CVariant::VariantTypeObject);
  changed["properties"] = std::move(properties);
  changed["player"]["players"] = player["players"];
  method = ANNOUNCEMENT::MESSAGE::ON_PROPERTIES_CHANGED;
  data = std::move(changed);
}

bool IJSONRPCAnnouncer::AsItemNotification(ANNOUNCEMENT::AnnouncementFlag flag,
                                           std::string& method,
                                           CVariant& data)
{
  if ((flag != ANNOUNCEMENT::VideoLibrary && flag != ANNOUNCEMENT::AudioLibrary) ||
      (method != ANNOUNCEMENT::MESSAGE::ON_UPDATE && method != ANNOUNCEMENT::MESSAGE::ON_REMOVE))
    return true;

  const CVariant& item = data.isMember("item") ? data["item"] : data;
  const int64_t id = item["id"].asInteger(-1);
  const std::string kind = item["type"].asString();
  const KODI::MEDIA::MediaType type = KODI::MEDIA::MediaTypeOf(kind);
  const bool isKind = flag == ANNOUNCEMENT::VideoLibrary ? CVideoLibrary::IsItemKind(type)
                                                         : CAudioLibrary::IsItemKind(type);
  if (id <= 0 || !isKind)
    return false;

  const bool removed = method == ANNOUNCEMENT::MESSAGE::ON_REMOVE;
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

  method = removed ? ANNOUNCEMENT::MESSAGE::ON_ITEM_REMOVED
           : added ? ANNOUNCEMENT::MESSAGE::ON_ITEM_ADDED
                   : ANNOUNCEMENT::MESSAGE::ON_ITEM_PROPERTIES_CHANGED;
  data = std::move(changed);
  return true;
}

} // namespace JSONRPC
