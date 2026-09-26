/*
 *  Copyright (C) 2022 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <optional>

namespace KODI::PLAYLIST
{

/*!
 * \brief The playlist ids of the JSON-RPC and Python interfaces. Inside Kodi a playlist is named
 * by the side it plays on; see Side, SideFromId() and IdFromSide().
 */
enum class Id : int
{
  TYPE_NONE = -1,
  TYPE_MUSIC = 0,
  TYPE_VIDEO = 1,
  TYPE_PICTURE = 2,
  TYPE_GAME = 3
};

/*!
 * \brief The two things Kodi renders. Whatever is playing holds one or both, and the side an
 * entry is put on is what it claims.
 */
enum class Side
{
  Video,
  Audio
};

inline std::optional<Side> SideFromId(Id id)
{
  switch (id)
  {
    case Id::TYPE_MUSIC:
      return Side::Audio;
    case Id::TYPE_VIDEO:
    case Id::TYPE_PICTURE:
    case Id::TYPE_GAME:
      return Side::Video;
    default:
      return std::nullopt;
  }
}

inline Id IdFromSide(std::optional<Side> side)
{
  if (!side)
    return Id::TYPE_NONE;
  return *side == Side::Audio ? Id::TYPE_MUSIC : Id::TYPE_VIDEO;
}

/*!
 * \brief Identifies one entry of one playlist. Never reused within that playlist, so a stale id
 * resolves to nothing rather than to a different entry.
 */
using EntryId = unsigned int;
constexpr EntryId NO_ENTRY = 0;

/*!
 * \brief Why the playlist is moving on. A user skip leaves out the current entry's repeat mark,
 * so a repeating entry can always be left.
 */
enum class Advance
{
  Automatic,
  User
};

/*!
 * \brief What the playlist does once nothing follows the last entry in play order.
 */
class Wrap
{
public:
  enum class Kind
  {
    None,
    ToStart,
    ToEntry
  };

  static Wrap None() { return Wrap(Kind::None, NO_ENTRY); }
  static Wrap ToStart() { return Wrap(Kind::ToStart, NO_ENTRY); }
  static Wrap To(EntryId entry) { return Wrap(Kind::ToEntry, entry); }

  Kind GetKind() const { return m_kind; }
  EntryId GetTarget() const { return m_target; }

  bool operator==(const Wrap& other) const = default;

private:
  Wrap(Kind kind, EntryId target) : m_kind(kind), m_target(target) {}

  Kind m_kind;
  EntryId m_target;
};

enum class ExcludeUsedPlaylists : bool
{
  DONT_EXCLUDE_USED_PLAYLISTS,
  EXCLUDE_USED_PLAYLISTS
};

} // namespace KODI::PLAYLIST
