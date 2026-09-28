/*
 *  Copyright (C) 2022 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <algorithm>
#include <cctype>
#include <optional>
#include <string_view>

#include <fmt/format.h>

namespace KODI::PLAYLIST
{

/*!
 * \brief The two playlists: Video and Audio. An entry's playlist is what it claims to hold.
 */
enum class Type
{
  Video,
  Audio
};
using enum Type;

/*!
 * \return The playlist an int carrying a Type names, if it names one.
 */
inline std::optional<Type> TypeFromInt(int value)
{
  if (value != static_cast<int>(Video) && value != static_cast<int>(Audio))
    return std::nullopt;
  return static_cast<Type>(value);
}

//! A playlist's name in the interfaces: "video" or "audio".
constexpr std::string_view NameOf(Type type)
{
  return type == Video ? "video" : "audio";
}

/*!
 * \return The playlist a name names, ignoring case: "video", or "audio" or "music".
 */
inline std::optional<Type> TypeFromName(std::string_view name)
{
  const auto is = [name](std::string_view word)
  {
    return std::ranges::equal(name, word, [](char a, char b)
                              { return std::tolower(static_cast<unsigned char>(a)) == b; });
  };
  if (is("video"))
    return Video;
  if (is("audio") || is("music"))
    return Audio;
  return std::nullopt;
}

/*!
 * \brief Identifies one entry of one playlist. Never reused within that playlist, so a stale id
 * resolves to nothing rather than to a different entry.
 */
using EntryId = unsigned int;
constexpr EntryId NO_ENTRY = 0;

/*!
 * \brief Why the playlist is moving on. A user skip does not repeat the current entry, so a
 * repeating entry can always be left; repeat stays on for whatever becomes current.
 */
enum class Advance
{
  Automatic,
  User
};

/*!
 * \brief What the playlist does once nothing follows the last entry in play order.
 */
enum class Wrap
{
  None,
  ToStart
};

/*!
 * \brief What an entry holds, recorded once when it is placed.
 */
enum class Holds
{
  Audio,
  Video,
  VideoAndAudio
};

} // namespace KODI::PLAYLIST

template<>
struct fmt::formatter<KODI::PLAYLIST::Type> : fmt::formatter<std::string_view>
{
  template<typename FormatContext>
  constexpr auto format(KODI::PLAYLIST::Type type, FormatContext& ctx) const
  {
    return fmt::formatter<std::string_view>::format(KODI::PLAYLIST::NameOf(type), ctx);
  }
};
