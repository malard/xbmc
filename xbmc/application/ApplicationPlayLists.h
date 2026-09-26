/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "application/IApplicationComponent.h"
#include "guilib/IMsgTargetCallback.h"
#include "messaging/IMessageTarget.h"
#include "playlists/PlayListTypes.h"
#include "threads/CriticalSection.h"

#include <array>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

class CAction;
class CFileItem;
class CFileItemList;
class CVariant;

namespace KODI::PLAYLIST
{
class CPlayList;
struct PlayListChange;
} // namespace KODI::PLAYLIST

/*!
 * \brief The Video and Audio playlists, and what each is doing.
 *
 * Both playlists always exist, possibly empty. The player works through one of them at a time;
 * while it plays something from the Video playlist that also holds audio, Audio follows Video,
 * and the Audio playlist waits.
 */
class CApplicationPlayLists : public IApplicationComponent,
                              public IMsgTargetCallback,
                              public KODI::MESSAGING::IMessageTarget
{
public:
  enum class Phase
  {
    Idle,
    Playing,
    Paused
  };

  /*!
   * \brief The three states of the repeat button, composed from an entry's repeat mark and the
   * playlist's wrap.
   */
  enum class Repeat
  {
    Off,
    One,
    All
  };

  /*!
   * \brief A property of how the playlists play that changed.
   */
  enum class PlayerProperty
  {
    PartyMode,
    Shuffled,
    Repeat
  };

  /*!
   * \brief A transition of what is playing.
   */
  enum class PlayerEvent
  {
    Play,
    AVStart,
    AVChange,
    Pause,
    Resume,
    Seek,
    SpeedChanged,
    Stop
  };

  CApplicationPlayLists();
  ~CApplicationPlayLists() override;

  bool OnMessage(CGUIMessage& message) override;
  int GetMessageMask() override;
  void OnApplicationMessage(KODI::MESSAGING::ThreadMessage* pMsg) override;
  bool OnAction(const CAction& action);

  KODI::PLAYLIST::CPlayList& GetPlayList(KODI::PLAYLIST::Type type);
  const KODI::PLAYLIST::CPlayList& GetPlayList(KODI::PLAYLIST::Type type) const;

  /*!
   * \return The playlist the player is working through, if any.
   */
  std::optional<KODI::PLAYLIST::Type> GetPlayingType() const;

  /*!
   * \brief Choose the playlist the player works through. Choosing another one ends party mode.
   */
  void SetPlayingType(std::optional<KODI::PLAYLIST::Type> type);

  /*!
   * \return Whether the player is working through this playlist and it is not idle.
   */
  bool IsPlaying(KODI::PLAYLIST::Type type) const;

  /*!
   * \return While the player is working through this playlist, the position in list order of the
   * entry the given number of entries ahead of the current one in play order, or behind it for a
   * negative offset; otherwise, or if there is no such entry, -1.
   */
  int GetPlayingPosition(KODI::PLAYLIST::Type type, int offset = 0) const;

  /*!
   * \brief The playlist for items nobody named one for: Video if any of them is video, else Audio.
   */
  static KODI::PLAYLIST::Type ChooseType(const CFileItemList& items);
  static KODI::PLAYLIST::Type ChooseType(const KODI::PLAYLIST::CPlayList& items);

  /*!
   * \brief The playlist for an item nobody named one for: a channel's is whether it is radio or
   * TV, an item holding only audio goes on Audio, and anything else on Video.
   */
  static KODI::PLAYLIST::Type ChooseType(const CFileItem& item);

  /*!
   * \return Where items queued without naming a playlist go: the playlist being played, else the
   * one matching what the player has open, else the fallback.
   */
  KODI::PLAYLIST::Type GetQueueType(KODI::PLAYLIST::Type fallback) const;

  Phase GetPhase(KODI::PLAYLIST::Type type) const;
  bool IsAudioFollowingVideo() const;

  /*!
   * \return The playerid the JSON-RPC interface publishes for what is playing.
   */
  int GetPlayerId() const;

  /*!
   * \return The playerid the JSON-RPC interface publishes for the given item: a channel's is
   * whether it is radio or TV, whoever plays it.
   */
  int GetPlayerId(const CFileItem* item) const;

  /*!
   * \brief Start playing a playlist.
   * \param position A position in list order, whose entry then leads the play order; with none,
   * the playlist plays from the start of its play order.
   * \param replace whether this item should replace the currently playing item. See
   * CApplication::PlayFile.
   * \param playPreviousOnFail whether to go back to the previous entry if playback fails.
   */
  bool Play(KODI::PLAYLIST::Type type,
            std::optional<int> position = std::nullopt,
            const std::string& player = "",
            bool replace = false,
            bool playPreviousOnFail = false);

  /*!
   * \brief Replace the playlist's contents with the items, and play it.
   * \param position As for Play(type, position, ...).
   */
  bool Play(KODI::PLAYLIST::Type type,
            const CFileItemList& items,
            std::optional<int> position = std::nullopt,
            const std::string& player = "");

  /*!
   * \brief Replace the playlist's contents with one item, and play it.
   * \param replace As for Play(type, position, ...).
   */
  bool Play(KODI::PLAYLIST::Type type,
            const std::shared_ptr<CFileItem>& item,
            const std::string& player,
            bool replace = false);

  /*!
   * \brief Replace the playlist's contents with what a playlist file or smart playlist holds, and
   * play it.
   * \param path Where the items were read from; see GetPlayingSourcePath().
   * \param position As for Play(type, position, ...).
   */
  bool PlaySource(KODI::PLAYLIST::Type type,
                  const std::string& path,
                  const KODI::PLAYLIST::CPlayList& items,
                  std::optional<int> position = std::nullopt,
                  const std::string& player = "");
  bool PlaySource(KODI::PLAYLIST::Type type,
                  const std::string& path,
                  const CFileItemList& items,
                  std::optional<int> position = std::nullopt,
                  const std::string& player = "");

  /*!
   * \return The playlist file or smart playlist that the playing playlist was read from, or empty.
   */
  std::string GetPlayingSourcePath() const;

  /*!
   * \brief Add items to a playlist without starting it.
   * \param playNext Play them next if the playlist is playing, rather than after the rest.
   * \return The position of the first of them, or -1 if there were none.
   */
  int Queue(KODI::PLAYLIST::Type type, const CFileItemList& items, bool playNext);

  /*!
   * \brief Play an item nobody has put on a playlist, on the playlist ChooseType() gives it.
   */
  bool Play(const std::shared_ptr<CFileItem>& item, const std::string& player);

  /*!
   * \brief Move the playing playlist on and play what follows.
   * \param advance Automatic when the current entry ended by itself.
   */
  bool PlayNext(KODI::PLAYLIST::Advance advance = KODI::PLAYLIST::Advance::User);
  bool PlayPrevious();

  /*!
   * \brief Make this the playlist the player works through, and move it on or back.
   */
  bool PlayNext(KODI::PLAYLIST::Type type);
  bool PlayPrevious(KODI::PLAYLIST::Type type);

  /*!
   * \brief Play the entry the given number of entries ahead of the current one in play order,
   * or behind it for a negative offset.
   */
  bool PlayOffset(int offset);

  KODI::PLAYLIST::EntryId PeekNextEntry() const;

  /*!
   * \brief The player accepted an entry to start by itself once the current one ends.
   */
  void OnNextQueued(KODI::PLAYLIST::EntryId entry);

  /*!
   * \brief The player could not take the next entry, so move on to it without playing it.
   */
  void SkipQueued(KODI::PLAYLIST::EntryId entry);

  void ClearQueued();

  /*!
   * \return What the player last started, as published; nullptr if it was an entry handed on by
   * OnNextQueued() that has since left the playlist.
   */
  std::shared_ptr<CFileItem> GetStartedItem() const;

  bool HasPlayedFirstFile() const;
  bool IsSingleItemNonRepeatPlaylist() const;

  void SetShuffle(KODI::PLAYLIST::Type type, bool shuffle, bool notify = false);
  bool IsShuffled(KODI::PLAYLIST::Type type) const;

  void SetRepeat(KODI::PLAYLIST::Type type, Repeat repeat, bool notify = false);
  Repeat GetRepeat(KODI::PLAYLIST::Type type) const;

  void ClearPlayLists();

  /*!
   * \brief The picture slideshow reports what it does. It keeps its own list and cursor, and holds
   * Video while it runs.
   * \param running Whether the slides advance by themselves, for Play.
   */
  void OnSlideShow(PlayerEvent event, const std::shared_ptr<const CFileItem>& slide, bool running);
  void OnSlideShow(PlayerProperty property, const CVariant& value) const;
  void OnSlideShowListChanged(const KODI::PLAYLIST::PlayListChange& change) const;

  /*!
   * \brief Publish a change to a property of what is playing. Nothing is published while nothing
   * plays.
   */
  void Announce(PlayerProperty property, const CVariant& value) const;

  /*!
   * \brief Publish properties the player reports changed, an object keyed by their published
   * names. Nothing is published while nothing plays.
   */
  void OnPlayerPropertiesChanged(const CVariant& properties) const;

  /*!
   * \brief Publish a transition of the given item.
   * \param data What the event carries besides the player's identity: the player's speed, time
   * and so on under "player", or whether playback ended for Stop.
   */
  void Announce(PlayerEvent event,
                const std::shared_ptr<const CFileItem>& item,
                CVariant data) const;

private:
  void OnPlayListChanged(KODI::PLAYLIST::Type type,
                         const std::vector<KODI::PLAYLIST::PlayListChange>& changes);
  bool PlayEntry(KODI::PLAYLIST::Type type,
                 KODI::PLAYLIST::EntryId entry,
                 const std::string& player,
                 bool replace,
                 bool playPreviousOnFail);
  void EndPlayback(bool clearPlayList);
  void Announce(KODI::PLAYLIST::Type type, PlayerProperty property, const CVariant& value) const;
  void OnMediaPlay(KODI::MESSAGING::ThreadMessage* pMsg);

  /*!
   * \brief Publish what the player reports about playback, as Player notifications. This target
   * is registered before the application's, so each notification precedes what the application
   * does in answer, including starting the next entry.
   */
  void PublishPlayback(const CGUIMessage& message);

  /*!
   * \brief What the player started: the entry OnNextQueued() handed it, now current, or else the
   * item it reports.
   */
  std::shared_ptr<CFileItem> OnStarted(const CGUIMessage& message);
  bool IsPlayingChannel() const;

  std::array<std::unique_ptr<KODI::PLAYLIST::CPlayList>, 2> m_playLists;

  mutable CCriticalSection m_critSection;
  std::optional<KODI::PLAYLIST::Type> m_playingType;
  std::array<Phase, 2> m_phase{Phase::Idle, Phase::Idle};
  Phase m_slideShowPhase{Phase::Idle};
  bool m_audioFollowsVideo{false};
  bool m_playbackStarted{false};
  bool m_playedFirstFile{false};
  KODI::PLAYLIST::EntryId m_queued{KODI::PLAYLIST::NO_ENTRY};
  std::shared_ptr<CFileItem> m_startedItem;
  int m_failedSongs{0};
  std::chrono::steady_clock::time_point m_failedSongsStart;
};
