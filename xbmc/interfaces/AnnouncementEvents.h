/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "interfaces/IAnnouncer.h"
#include "media/MediaType.h"
#include "utils/Variant.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>

class CFileItem;
enum class SettingLevel;

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

namespace EVENT::GUI
{
struct ScreensaverActivated
{
};

struct ScreensaverDeactivated
{
  //! A power down or suspend follows, so the deactivation may be ignored.
  bool shuttingDown{false};
};

struct DPMSActivated
{
};

struct DPMSDeactivated
{
};

struct SkinUnloading
{
};

struct SkinLoaded
{
};

struct SkinLoadFailed
{
};

struct WindowFocused
{
};

struct WindowUnfocused
{
};
} // namespace EVENT::GUI

struct GUIEvent : std::variant<EVENT::GUI::ScreensaverActivated,
                               EVENT::GUI::ScreensaverDeactivated,
                               EVENT::GUI::DPMSActivated,
                               EVENT::GUI::DPMSDeactivated,
                               EVENT::GUI::SkinUnloading,
                               EVENT::GUI::SkinLoaded,
                               EVENT::GUI::SkinLoadFailed,
                               EVENT::GUI::WindowFocused,
                               EVENT::GUI::WindowUnfocused>
{
  using variant::variant;
};

namespace EVENT::SYSTEM
{
struct Quit
{
  int exitCode{0};
};

struct Restart
{
};

struct Sleep
{
};

struct Wake
{
};

struct LowBattery
{
};
} // namespace EVENT::SYSTEM

struct SystemEvent : std::variant<EVENT::SYSTEM::Quit,
                                  EVENT::SYSTEM::Restart,
                                  EVENT::SYSTEM::Sleep,
                                  EVENT::SYSTEM::Wake,
                                  EVENT::SYSTEM::LowBattery>
{
  using variant::variant;
};

namespace EVENT::LIBRARY
{
struct ScanStarted
{
};

struct ScanFinished
{
};

struct CleanStarted
{
};

struct CleanFinished
{
};

struct Update
{
  //! When given, it names the item, and type and id are not used.
  std::shared_ptr<const CFileItem> item{};
  KODI::MEDIA::MediaType type{KODI::MEDIA::MediaType::NONE};
  int id{-1};
  bool transaction{false};
  bool added{false};
  std::optional<int> playCount{};
  //! The properties that changed, as JSON-RPC describes them.
  std::optional<CVariant> properties{};
};

struct Remove
{
  KODI::MEDIA::MediaType type{KODI::MEDIA::MediaType::NONE};
  int id{-1};
  bool transaction{false};
};

struct Export
{
  std::optional<std::string> root{};
  std::optional<std::string> file{};
  std::optional<int> failCount{};
};

struct Refresh
{
};
} // namespace EVENT::LIBRARY

struct LibraryEvent : std::variant<EVENT::LIBRARY::ScanStarted,
                                   EVENT::LIBRARY::ScanFinished,
                                   EVENT::LIBRARY::CleanStarted,
                                   EVENT::LIBRARY::CleanFinished,
                                   EVENT::LIBRARY::Update,
                                   EVENT::LIBRARY::Remove,
                                   EVENT::LIBRARY::Export,
                                   EVENT::LIBRARY::Refresh>
{
  using variant::variant;
};

struct VideoLibraryEvent : LibraryEvent
{
  using LibraryEvent::LibraryEvent;
};

struct AudioLibraryEvent : LibraryEvent
{
  using LibraryEvent::LibraryEvent;
};

//! Whether the event is part of a scan or clean, which announces its own end.
bool IsTransaction(const LibraryEvent& event);

namespace EVENT::INPUT
{
struct Requested
{
  enum class Kind
  {
    Keyboard,
    Password,
    Number,
    NumericPassword,
    Date,
    Time,
    Seconds,
    IPAddress,
  };

  Kind kind{Kind::Keyboard};
  std::optional<std::string> title;
  std::string value;
};

struct Finished
{
};
} // namespace EVENT::INPUT

struct InputEvent : std::variant<EVENT::INPUT::Requested, EVENT::INPUT::Finished>
{
  using variant::variant;
};

namespace EVENT::PVR
{
//! A radio traffic announcement started or ended.
struct RadioTrafficAnnouncement
{
  bool on{false};
};

//! The time a radio station's RDS clock gives.
struct RadioClock
{
  //! RFC 1123, or empty when the clock is invalid.
  std::string dateTime;
};

//! A radio traffic message channel (RDS-TMC) message.
struct RadioTrafficMessage
{
  std::string channel;
  uint16_t ident{0};
  unsigned int flags{0};
  uint8_t x{0};
  unsigned int y{0};
  unsigned int z{0};
};
} // namespace EVENT::PVR

struct PVREvent : std::variant<EVENT::PVR::RadioTrafficAnnouncement,
                               EVENT::PVR::RadioClock,
                               EVENT::PVR::RadioTrafficMessage>
{
  using variant::variant;
};

namespace EVENT::INFO
{
//! The item the info labels describe changed.
struct Changed
{
};
} // namespace EVENT::INFO

struct InfoEvent : std::variant<EVENT::INFO::Changed>
{
  using variant::variant;
};

namespace EVENT::SOURCES
{
struct Added
{
  std::string path;
};

struct Removed
{
  std::string path;
};

struct Updated
{
  std::string path;
};
} // namespace EVENT::SOURCES

struct SourcesEvent
  : std::variant<EVENT::SOURCES::Added, EVENT::SOURCES::Removed, EVENT::SOURCES::Updated>
{
  using variant::variant;
};

namespace EVENT::SETTINGS
{
struct LevelChanged
{
  SettingLevel level{};
};
} // namespace EVENT::SETTINGS

struct SettingsEvent : std::variant<EVENT::SETTINGS::LevelChanged>
{
  using variant::variant;
};

struct Announcement : std::variant<PlayerEvent,
                                   PlaylistEvent,
                                   GUIEvent,
                                   SystemEvent,
                                   VideoLibraryEvent,
                                   AudioLibraryEvent,
                                   InputEvent,
                                   PVREvent,
                                   InfoEvent,
                                   SourcesEvent,
                                   SettingsEvent>
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
