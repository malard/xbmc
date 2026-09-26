/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "PlayListShuffle.h"

#include <algorithm>

namespace KODI::PLAYLIST
{

namespace
{

EntryId FollowingIn(const std::vector<EntryId>& order, EntryId entry)
{
  if (order.empty())
    return NO_ENTRY;
  if (entry == NO_ENTRY)
    return order.front();

  const auto it = std::ranges::find(order, entry);
  if (it == order.end() || std::next(it) == order.end())
    return NO_ENTRY;
  return *std::next(it);
}

EntryId PrecedingIn(const std::vector<EntryId>& order, EntryId entry)
{
  if (order.empty())
    return NO_ENTRY;
  if (entry == NO_ENTRY)
    return order.back();

  const auto it = std::ranges::find(order, entry);
  if (it == order.end() || it == order.begin())
    return NO_ENTRY;
  return *std::prev(it);
}

void RemoveFrom(std::vector<EntryId>& order, EntryId entry)
{
  std::erase(order, entry);
}

} // namespace

void CPlayListNoShuffle::Reset(const std::vector<EntryId>& listOrder, EntryId current)
{
  m_order = listOrder;
}

void CPlayListNoShuffle::OnAdded(EntryId entry, int position, EntryId current)
{
  if (position < 0 || position > static_cast<int>(m_order.size()))
    position = static_cast<int>(m_order.size());
  m_order.insert(m_order.begin() + position, entry);
}

void CPlayListNoShuffle::OnRemoved(EntryId entry)
{
  RemoveFrom(m_order, entry);
}

void CPlayListNoShuffle::OnMoved(EntryId entry, int position)
{
  RemoveFrom(m_order, entry);
  OnAdded(entry, position, NO_ENTRY);
}

EntryId CPlayListNoShuffle::Following(EntryId entry) const
{
  return FollowingIn(m_order, entry);
}

EntryId CPlayListNoShuffle::Preceding(EntryId entry) const
{
  return PrecedingIn(m_order, entry);
}

CPlayListRandomShuffle::CPlayListRandomShuffle() : m_random(std::random_device{}())
{
}

void CPlayListRandomShuffle::Reset(const std::vector<EntryId>& listOrder, EntryId current)
{
  m_order = listOrder;
  auto toShuffle = m_order.begin();
  if (const auto it = std::ranges::find(m_order, current); it != m_order.end())
  {
    std::iter_swap(m_order.begin(), it);
    ++toShuffle;
  }
  std::shuffle(toShuffle, m_order.end(), m_random);
}

void CPlayListRandomShuffle::OnAdded(EntryId entry, int position, EntryId current)
{
  // Anywhere after the current entry, including straight after it and at the very end.
  const auto it = std::ranges::find(m_order, current);
  const size_t first = it == m_order.end() ? 0 : std::distance(m_order.begin(), it) + 1;
  std::uniform_int_distribution<size_t> pick(first, m_order.size());
  m_order.insert(m_order.begin() + pick(m_random), entry);
}

void CPlayListRandomShuffle::OnRemoved(EntryId entry)
{
  RemoveFrom(m_order, entry);
}

EntryId CPlayListRandomShuffle::Following(EntryId entry) const
{
  return FollowingIn(m_order, entry);
}

EntryId CPlayListRandomShuffle::Preceding(EntryId entry) const
{
  return PrecedingIn(m_order, entry);
}

} // namespace KODI::PLAYLIST
