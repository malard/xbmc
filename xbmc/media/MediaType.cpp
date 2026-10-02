/*
 *  Copyright (C) 2013-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "MediaType.h"

#include "ServiceBroker.h"
#include "resources/LocalizeStrings.h"
#include "resources/ResourcesComponent.h"
#include "utils/StringUtils.h"

#include <algorithm>
#include <array>

namespace KODI::MEDIA
{

namespace
{
struct MediaTypeInfo
{
  MediaType type;
  std::string_view name;
  std::string_view plural;
  bool container;
  int localizationSingular;
  int localizationPlural;
  int localizationSingularCapital;
  int localizationPluralCapital;
};

// clang-format off
constexpr std::array<MediaTypeInfo, 12> MEDIA_TYPES{{
    {MediaType::MUSIC,            "music",        "music",         true,  36914, 36915,   249,   249},
    {MediaType::ARTIST,           "artist",       "artists",       true,  36916, 36917,   557,   133},
    {MediaType::ALBUM,            "album",        "albums",        true,  36918, 36919,   558,   132},
    {MediaType::SONG,             "song",         "songs",         false, 36920, 36921,   179,   134},
    {MediaType::VIDEO,            "video",        "videos",        true,  36912, 36913,   291,     3},
    {MediaType::VIDEO_COLLECTION, "set",          "sets",          true,  36910, 36911, 20141, 20434},
    {MediaType::MUSIC_VIDEO,      "musicvideo",   "musicvideos",   false, 36908, 36909, 20391, 20389},
    {MediaType::MOVIE,            "movie",        "movies",        false, 36900, 36901, 20338, 20342},
    {MediaType::TV_SHOW,          "tvshow",       "tvshows",       true,  36902, 36903, 36902, 36903},
    {MediaType::SEASON,           "season",       "seasons",       true,  36904, 36905, 20373, 33054},
    {MediaType::EPISODE,          "episode",      "episodes",      false, 36906, 36907, 20359, 20360},
    {MediaType::VIDEO_VERSION,    "videoversion", "videoversions", false, 40010, 40011, 40012, 40013},
}};
// clang-format on

const MediaTypeInfo* Find(MediaType type)
{
  const auto it = std::ranges::find(MEDIA_TYPES, type, &MediaTypeInfo::type);
  return it != MEDIA_TYPES.end() ? &*it : nullptr;
}

//! Every type's Field as a string, indexed by type, so a name can be handed out by reference.
template<std::string_view MediaTypeInfo::* Field>
const std::string& Text(MediaType type)
{
  static const std::array<std::string, MEDIA_TYPES.size() + 1> texts = []
  {
    std::array<std::string, MEDIA_TYPES.size() + 1> result;
    for (const MediaTypeInfo& info : MEDIA_TYPES)
      result[static_cast<size_t>(info.type)] = std::string{info.*Field};
    return result;
  }();
  return texts[static_cast<size_t>(type)];
}

std::string Localize(MediaType type, int MediaTypeInfo::* id)
{
  const MediaTypeInfo* info = Find(type);
  if (!info || info->*id <= 0)
    return {};
  return CServiceBroker::GetResourcesComponent().GetLocalizeStrings().Get(info->*id);
}
} // namespace

const std::string& NameOf(MediaType type)
{
  return Text<&MediaTypeInfo::name>(type);
}

const std::string& PluralNameOf(MediaType type)
{
  return Text<&MediaTypeInfo::plural>(type);
}

MediaType MediaTypeFromName(std::string_view name)
{
  const auto it = std::ranges::find_if(MEDIA_TYPES,
                                       [name](const MediaTypeInfo& info)
                                       {
                                         return StringUtils::EqualsNoCase(name, info.name) ||
                                                StringUtils::EqualsNoCase(name, info.plural);
                                       });
  return it != MEDIA_TYPES.end() ? it->type : MediaType::NONE;
}

MediaType MediaTypeOf(std::string_view name)
{
  const auto it = std::ranges::find(MEDIA_TYPES, name, &MediaTypeInfo::name);
  return it != MEDIA_TYPES.end() ? it->type : MediaType::NONE;
}

bool IsContainer(MediaType type)
{
  const MediaTypeInfo* info = Find(type);
  return info && info->container;
}

std::string GetLocalization(MediaType type)
{
  return Localize(type, &MediaTypeInfo::localizationSingular);
}

std::string GetPluralLocalization(MediaType type)
{
  return Localize(type, &MediaTypeInfo::localizationPlural);
}

std::string GetCapitalLocalization(MediaType type)
{
  return Localize(type, &MediaTypeInfo::localizationSingularCapital);
}

std::string GetCapitalPluralLocalization(MediaType type)
{
  return Localize(type, &MediaTypeInfo::localizationPluralCapital);
}

} // namespace KODI::MEDIA
