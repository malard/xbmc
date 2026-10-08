/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "interfaces/IAnnouncer.h"
#include "utils/Variant.h"

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <variant>

class CFileItem;

/*!
 * \brief What an announcement says, as the sender knew it when it was raised.
 *
 * A listener acts on the event and never on live state: delivery happens later, on the
 * announcement manager's own thread. An item an event carries is a copy taken when it was
 * announced, and nothing changes it.
 */
namespace ANNOUNCEMENT
{

namespace EVENT::PLAYER
{
//! What a playback holds.
struct Players
{
  bool video{false};
  bool audio{false};

  bool operator==(const Players&) const = default;
};

struct Play
{
  std::shared_ptr<const CFileItem> item;
  int speed{1};
  Players players;
};

struct AVStart
{
  std::shared_ptr<const CFileItem> item;
  Players players;
};

struct AVChange
{
  std::shared_ptr<const CFileItem> item;
  Players players;
};

struct Pause
{
  std::shared_ptr<const CFileItem> item;
  Players players;
};

struct Resume
{
  std::shared_ptr<const CFileItem> item;
  Players players;
};

struct SpeedChanged
{
  std::shared_ptr<const CFileItem> item;
  int speed{1};
  Players players;
};

struct Seek
{
  std::shared_ptr<const CFileItem> item;
  int speed{1};
  std::chrono::milliseconds time{0};
  std::chrono::milliseconds seekOffset{0};
  Players players;
};

struct Stop
{
  std::shared_ptr<const CFileItem> item;
  //! The playback reached its end, rather than being stopped.
  bool end{false};
  //! Given for the slideshow only.
  std::optional<Players> players;
};

struct PlaybackFailed
{
  enum class Reason
  {
    None,
    Unplayable,
    Unresolved,
    Locked,
    Error,
  };

  std::shared_ptr<const CFileItem> item;
  Reason reason{Reason::None};
};

//! The properties of the player that changed; only those given changed.
struct PropertiesChanged
{
  std::optional<Players> players;
  std::optional<int> volume;
  std::optional<bool> muted;
  std::optional<bool> partyMode;
  std::optional<bool> shuffled;
  std::optional<std::string> repeat;
  std::optional<bool> subtitleEnabled;
  //! The streams as JSON-RPC describes them.
  std::optional<CVariant> currentSubtitle;
  std::optional<CVariant> currentAudioStream;
  std::optional<CVariant> currentVideoStream;
};

struct ContentGeometryChange
{
  //! The geometry as JSON-RPC describes it.
  CVariant geometry;
  Players players;
};

struct Commercial
{
  //! How long the commercial lasts, or where it ends, as MM:SS.
  std::string time;
};

struct ToggleSkipCommercials
{
  bool skip{false};
};

struct ProcessInfo
{
};

struct Menu
{
};

struct BlurayMenuError
{
};

struct BlurayEncryptedError
{
};

//! The source delivers data more slowly than playback needs.
struct SourceSlow
{
};
//! Whether the item is announced as a picture rather than as a video, song or channel.
bool IsPicture(const CFileItem* item);
} // namespace EVENT::PLAYER

struct PlayerEvent : std::variant<EVENT::PLAYER::Play,
                                  EVENT::PLAYER::AVStart,
                                  EVENT::PLAYER::AVChange,
                                  EVENT::PLAYER::Pause,
                                  EVENT::PLAYER::Resume,
                                  EVENT::PLAYER::SpeedChanged,
                                  EVENT::PLAYER::Seek,
                                  EVENT::PLAYER::Stop,
                                  EVENT::PLAYER::PlaybackFailed,
                                  EVENT::PLAYER::PropertiesChanged,
                                  EVENT::PLAYER::ContentGeometryChange,
                                  EVENT::PLAYER::Commercial,
                                  EVENT::PLAYER::ToggleSkipCommercials,
                                  EVENT::PLAYER::ProcessInfo,
                                  EVENT::PLAYER::Menu,
                                  EVENT::PLAYER::BlurayMenuError,
                                  EVENT::PLAYER::BlurayEncryptedError,
                                  EVENT::PLAYER::SourceSlow>
{
  using variant::variant;
};

namespace EVENT::PLAYLIST
{
struct Add
{
  std::string playList;
  int position{-1};
  std::shared_ptr<const CFileItem> item;
};

struct Remove
{
  std::string playList;
  int position{-1};
};

struct Clear
{
  std::string playList;
};

//! The properties of a playlist that changed; only those given changed.
struct PropertiesChanged
{
  std::string playList;
  std::optional<bool> shuffled;
  std::optional<std::string> repeat;
};
} // namespace EVENT::PLAYLIST

struct PlaylistEvent : std::variant<EVENT::PLAYLIST::Add,
                                    EVENT::PLAYLIST::Remove,
                                    EVENT::PLAYLIST::Clear,
                                    EVENT::PLAYLIST::PropertiesChanged>
{
  using variant::variant;
};

struct Announcement : std::variant<PlayerEvent, PlaylistEvent>
{
  using variant::variant;
};

AnnouncementFlag FlagOf(const Announcement& announcement);
const char* MessageOf(const Announcement& announcement);
std::shared_ptr<const CFileItem> ItemOf(const Announcement& announcement);

/*!
 * \brief The data JSON-RPC and Python have been given for this announcement, before the item is
 * added to it. Python add-ons read it as it is, so nothing in it may move, be renamed, be
 * removed or change type.
 */
CVariant LegacyDataOf(const Announcement& announcement);

/*!
 * \brief The announcement with its item replaced by \p item.
 */
Announcement WithItem(Announcement announcement, std::shared_ptr<const CFileItem> item);

} // namespace ANNOUNCEMENT
