/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "PlayListTypes.h"

#include <random>
#include <vector>

namespace KODI::PLAYLIST
{

/*!
 * \brief Decides the play order of a playlist without reordering it.
 *
 * It is told of every change to the list, by entry id, and answers what follows and what
 * precedes an entry. Whether a change needs acting on is the routine's business.
 */
class IPlayListShuffle
{
public:
  virtual ~IPlayListShuffle() = default;

  /*!
   * \brief Start over from the whole list.
   * \param listOrder Every entry, in list order.
   * \param current The current entry, or NO_ENTRY.
   */
  virtual void Reset(const std::vector<EntryId>& listOrder, EntryId current) = 0;

  virtual void OnAdded(EntryId entry, int position, EntryId current) = 0;
  virtual void OnRemoved(EntryId entry) = 0;
  virtual void OnMoved(EntryId entry, int position) = 0;

  /*!
   * \return The entry after the given one in play order, or NO_ENTRY after the last. Given
   * NO_ENTRY, the first entry.
   */
  virtual EntryId Following(EntryId entry) const = 0;

  /*!
   * \return The entry before the given one in play order, or NO_ENTRY before the first. Given
   * NO_ENTRY, the last entry.
   */
  virtual EntryId Preceding(EntryId entry) const = 0;
};

/*!
 * \brief Plays the list in the order it was built.
 */
class CPlayListNoShuffle : public IPlayListShuffle
{
public:
  void Reset(const std::vector<EntryId>& listOrder, EntryId current) override;
  void OnAdded(EntryId entry, int position, EntryId current) override;
  void OnRemoved(EntryId entry) override;
  void OnMoved(EntryId entry, int position) override;
  EntryId Following(EntryId entry) const override;
  EntryId Preceding(EntryId entry) const override;

private:
  std::vector<EntryId> m_order;
};

/*!
 * \brief Plays the list in a uniformly random order. The current entry leads the order it
 * shuffles, and entries added later are placed at random among those still to come.
 */
class CPlayListRandomShuffle : public IPlayListShuffle
{
public:
  CPlayListRandomShuffle();

  void Reset(const std::vector<EntryId>& listOrder, EntryId current) override;
  void OnAdded(EntryId entry, int position, EntryId current) override;
  void OnRemoved(EntryId entry) override;
  void OnMoved(EntryId entry, int position) override {}
  EntryId Following(EntryId entry) const override;
  EntryId Preceding(EntryId entry) const override;

private:
  std::vector<EntryId> m_order;
  std::mt19937 m_random;
};

} // namespace KODI::PLAYLIST
