/*
 *  Copyright (C) 2022 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "utils/ContentUtils.h"

#include <memory>
#include <string>

class CFileItem;
class CFileItemList;

namespace KODI::VIDEO::UTILS
{
/*! \brief Start playback of the given item. If the item is a folder, build a playlist with
  all items contained in the folder and start playback of the playlist. If item is a single video
  item, it replaces the video playlist and plays.
  \param item [in] the item to play
  \param player [in] the player to use, empty for default player
  \param mode [in] queue all successors and play them after item
  */
void PlayItem(const std::shared_ptr<CFileItem>& item,
              const std::string& player,
              ContentUtils::PlayMode mode = ContentUtils::PlayMode::CHECK_AUTO_PLAY_NEXT_ITEM);

enum class QueuePosition
{
  POSITION_BEGIN, // play next if the playlist is playing, otherwise at the end
  POSITION_END, // at the end of the playlist
};

/*! \brief Queue the given item on the video playlist.
  \param item [in] the item to queue
  \param pos [in] whether to play the item next or add it at the end
  */
void QueueItem(const std::shared_ptr<CFileItem>& item, QueuePosition pos);

/*! \brief For a given item, get the items to put in a playlist. If the item is a folder, all
  subitems will be added recursively to the returned item list. If the item is a playlist, the
  playlist will be loaded and contained items will be added to the returned item list. Shows a
  busy dialog if action takes certain amount of time to give the user visual feedback.
  \param item [in] the item to add to the playlist
  \param queuedItems [out] the items that can be put in a play list
  \param mode [in] queue all successors and play them after item
  \param startAt [in] the item to start at, if any; resuming starts at the last played item
  \param startPosition [out] where the start landed in queuedItems, or -1 if nowhere
  \return true on success, false otherwise
  */
bool GetItemsForPlayList(const std::shared_ptr<CFileItem>& item,
                         CFileItemList& queuedItems,
                         ContentUtils::PlayMode mode,
                         const std::shared_ptr<CFileItem>& startAt = nullptr,
                         int* startPosition = nullptr);

/*!
 \brief Check whether the given item can be played as one or more videos.
 \param item The item to check
 \return True if playable, false otherwise.
 */
bool IsItemPlayable(const CFileItem& item);

/*!
 \brief Check whether for the given item information is stored in the video database.
 \param item The item to check
 \return True if info is available, false otherwise.
 */
bool HasItemVideoDbInformation(const CFileItem& item);

/*!
 \brief Get a localized resume string for the given item, if it is resumable.
 \param item The item to retrieve the resume string for
 \return The resume string or empty string in case the item is not resumable.
 */
std::string GetResumeString(const CFileItem& item);

/*!
 \brief Get a localized resume string for the given start offset.
 \param startOffset The Start offset to get the resume string for
 \param partNumber The part number if available (1-based), 0 otherwise
 \return The resume string.
 */
std::string GetResumeString(int64_t startOffset, unsigned int partNumber);

/*!
 \brief Tell any open window and playlist that an item's path has changed, so that they update the
 item.
 \param item [in] The item, with its new path
 \param oldPath [in] The path the item had before
 \param oldFileId [in] The library file id the item had before, when the change gave it a new one
 */
void NotifyItemPathChanged(const CFileItem& item, const std::string& oldPath, int oldFileId = -1);

} // namespace KODI::VIDEO::UTILS
