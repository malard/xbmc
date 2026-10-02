/*
 *  Copyright (C) 2013-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <string>
#include <string_view>

namespace KODI::MEDIA
{

//! \brief What a library item is.
enum class MediaType
{
  NONE,
  MUSIC,
  ARTIST,
  ALBUM,
  SONG,
  VIDEO,
  VIDEO_COLLECTION,
  MUSIC_VIDEO,
  MOVIE,
  TV_SHOW,
  SEASON,
  EPISODE,
  VIDEO_VERSION,
};

//! \brief The name a type is stored and exposed under, e.g. "movie". Empty for NONE.
const std::string& NameOf(MediaType type);

//! \brief Formats as its name, for fmt.
inline const std::string& format_as(MediaType type)
{
  return NameOf(type);
}

//! \brief The plural name, e.g. "movies", and "sets" for a video collection. Empty for NONE.
const std::string& PluralNameOf(MediaType type);

//! \brief The type \p name gives, singular or plural, in any case. NONE for any other text.
MediaType MediaTypeFromName(std::string_view name);

//! \brief The type whose stored name is exactly \p name. NONE for any other text.
MediaType MediaTypeOf(std::string_view name);

//! \brief Whether an item of this type holds other items, as an album holds songs.
bool IsContainer(MediaType type);

//! \brief The localized name, e.g. "movie". Empty for NONE.
std::string GetLocalization(MediaType type);

//! \brief The localized plural name, e.g. "movies". Empty for NONE.
std::string GetPluralLocalization(MediaType type);

//! \brief The localized name as a heading, e.g. "Movie". Empty for NONE.
std::string GetCapitalLocalization(MediaType type);

//! \brief The localized plural name as a heading, e.g. "Movies". Empty for NONE.
std::string GetCapitalPluralLocalization(MediaType type);

} // namespace KODI::MEDIA
