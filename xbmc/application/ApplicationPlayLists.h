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
 * \brief The Video and Audio playlists, and what each side is doing.
 *
 * Both playlists always exist, possibly empty. The player works through one of them at a time;
 * while it plays something on the Video side that also holds audio, the Audio side follows the
 * Video side, and its own playlist waits.
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

  CApplicationPlayLists();
  ~CApplicationPlayLists() override;

  bool OnMessage(CGUIMessage& message) override;
  int GetMessageMask() override;
  void OnApplicationMessage(KODI::MESSAGING::ThreadMessage* pMsg) override;
  bool OnAction(const CAction& action);

  KODI::PLAYLIST::CPlayList& GetPlayList(KODI::PLAYLIST::Side side);
  const KODI::PLAYLIST::CPlayList& GetPlayList(KODI::PLAYLIST::Side side) const;

  /*!
   * \return The side whose playlist the player is working through, if any.
   */
  std::optional<KODI::PLAYLIST::Side> GetPlayingSide() const;

  /*!
   * \brief Choose the playlist the player works through. Choosing another one ends party mode.
   */
  void SetPlayingSide(std::optional<KODI::PLAYLIST::Side> side);

  Phase GetPhase(KODI::PLAYLIST::Side side) const;
  bool IsAudioFollowingVideo() const;

  /*!
   * \return The playerid the JSON-RPC interface publishes for what is playing.
   */
  int GetPlayerId() const;

  /*!
   * \brief Start playing a side's playlist.
   * \param position A position in list order, whose entry then leads the play order; with none,
   * the playlist plays from the start of its play order.
   * \param replace whether this item should replace the currently playing item. See
   * CApplication::PlayFile.
   * \param playPreviousOnFail whether to go back to the previous entry if playback fails.
   */
  bool Play(KODI::PLAYLIST::Side side,
            std::optional<int> position = std::nullopt,
            const std::string& player = "",
            bool replace = false,
            bool playPreviousOnFail = false);

  /*!
   * \brief Replace the side's playlist with a playlist of one item, and play it.
   */
  bool Play(KODI::PLAYLIST::Side side,
            const std::shared_ptr<CFileItem>& item,
            const std::string& player);

  /*!
   * \brief Play an item nobody has put on a side, choosing the side from what the item is.
   */
  bool Play(const std::shared_ptr<CFileItem>& item, const std::string& player);

  /*!
   * \brief Move the playing side on and play what follows.
   * \param advance Automatic when the current entry ended by itself.
   */
  bool PlayNext(KODI::PLAYLIST::Advance advance = KODI::PLAYLIST::Advance::User);
  bool PlayPrevious();

  /*!
   * \brief Play the entry the given number of entries ahead of the current one in play order,
   * or behind it for a negative offset.
   */
  bool PlayOffset(int offset);

  /*!
   * \brief What would play after the current entry of the playing side, without moving to it.
   */
  std::shared_ptr<CFileItem> PeekNextItem(int steps = 1) const;

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
   * \brief The player started what OnNextQueued() handed it, so that is now the current entry.
   * \return Nothing if nothing was queued; otherwise the entry's item, or nullptr if the entry
   * has gone from the playlist since.
   */
  std::optional<std::shared_ptr<CFileItem>> OnQueuedStarted();

  bool HasPlayedFirstFile() const;
  bool IsSingleItemNonRepeatPlaylist() const;

  void SetShuffle(KODI::PLAYLIST::Side side, bool shuffle, bool notify = false);
  bool IsShuffled(KODI::PLAYLIST::Side side) const;

  void SetRepeat(KODI::PLAYLIST::Side side, Repeat repeat, bool notify = false);
  Repeat GetRepeat(KODI::PLAYLIST::Side side) const;

  void ClearPlayLists();

private:
  void OnPlayListChanged(KODI::PLAYLIST::Side side,
                         const std::vector<KODI::PLAYLIST::PlayListChange>& changes);
  bool PlayEntry(KODI::PLAYLIST::Side side,
                 KODI::PLAYLIST::EntryId entry,
                 const std::string& player,
                 bool replace,
                 bool playPreviousOnFail);
  void EndPlayback(bool clearPlayList);
  void AnnouncePropertyChanged(KODI::PLAYLIST::Side side,
                               const std::string& property,
                               const CVariant& value) const;
  void OnMediaPlay(KODI::MESSAGING::ThreadMessage* pMsg);

  std::array<std::unique_ptr<KODI::PLAYLIST::CPlayList>, 2> m_playLists;

  mutable CCriticalSection m_critSection;
  std::optional<KODI::PLAYLIST::Side> m_playingSide;
  std::array<Phase, 2> m_phase{Phase::Idle, Phase::Idle};
  bool m_audioFollowsVideo{false};
  bool m_playbackStarted{false};
  bool m_playedFirstFile{false};
  KODI::PLAYLIST::EntryId m_queued{KODI::PLAYLIST::NO_ENTRY};
  int m_failedSongs{0};
  std::chrono::steady_clock::time_point m_failedSongsStart;
};
