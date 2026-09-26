/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "PlayList.h"

#include "FileItem.h"
#include "FileItemList.h"
#include "PlayListFactory.h"
#include "PlayListShuffle.h"
#include "filesystem/File.h"
#include "music/MusicFileItemClassify.h"
#include "music/tags/MusicInfoTag.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/log.h"

#include <algorithm>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <typeinfo>
#include <utility>
#include <vector>

using namespace MUSIC_INFO;
using namespace XFILE;

namespace KODI::PLAYLIST
{

CPlayList::CPlayList() : m_shuffle(std::make_unique<CPlayListNoShuffle>())
{
}

CPlayList::~CPlayList() = default;

void CPlayList::SetObserver(Observer observer)
{
  std::unique_lock lock(m_critSection);
  m_observer = std::move(observer);
}

void CPlayList::Notify(const Changes& changes) const
{
  if (changes.empty())
    return;

  Observer observer;
  {
    std::unique_lock lock(m_critSection);
    observer = m_observer;
  }
  if (observer)
    observer(changes);
}

int CPlayList::FindLocked(EntryId entry) const
{
  if (entry == NO_ENTRY)
    return -1;

  const auto it = std::ranges::find(m_entries, entry, &PlayListEntry::id);
  return it == m_entries.end() ? -1 : static_cast<int>(std::distance(m_entries.begin(), it));
}

EntryId CPlayList::InsertLocked(const std::shared_ptr<CFileItem>& item,
                                int position,
                                Changes& changes)
{
  const int size = static_cast<int>(m_entries.size());
  if (position < 0 || position > size)
    position = size;

  // set 'IsPlayable' property - needed for properly handling plugin:// URLs
  item->SetProperty("IsPlayable", true);

  // set 'BasePath' property - needed for properly handling browse for subtitles
  if (!item->HasProperty("BasePath"))
    item->SetProperty("BasePath", m_strBasePath);

  PlayListEntry entry;
  entry.id = ++m_lastId;
  entry.item = item;
  m_entries.insert(m_entries.begin() + position, entry);
  m_shuffle->OnAdded(entry.id, position, m_current);

  changes.push_back({PlayListChange::Type::Added, entry.id, position, item});
  return entry.id;
}

void CPlayList::RemoveLocked(int position, Changes& changes)
{
  const EntryId entry = m_entries[position].id;

  // Leave the cursor on the entry before, so that what followed the removed entry still follows.
  if (entry == m_current)
    m_current = m_shuffle->Preceding(entry);

  m_entries.erase(m_entries.begin() + position);
  m_shuffle->OnRemoved(entry);

  changes.push_back({PlayListChange::Type::Removed, entry, position, nullptr});
}

EntryId CPlayList::Add(const std::shared_ptr<CFileItem>& item)
{
  return Insert(item, -1);
}

void CPlayList::Add(const CPlayList& playlist)
{
  Insert(playlist, -1);
}

void CPlayList::Add(const CFileItemList& items)
{
  Insert(items, -1);
}

void CPlayList::Insert(const CPlayList& playlist, int iPosition /* = -1 */)
{
  std::vector<std::shared_ptr<CFileItem>> items;
  {
    std::unique_lock lock(playlist.m_critSection);
    for (const auto& entry : playlist.m_entries)
      items.emplace_back(entry.item);
  }

  Changes changes;
  {
    std::unique_lock lock(m_critSection);
    if (iPosition < 0 || iPosition > static_cast<int>(m_entries.size()))
      iPosition = static_cast<int>(m_entries.size());
    for (const auto& item : items)
      InsertLocked(item, iPosition++, changes);
  }
  Notify(changes);
}

void CPlayList::Insert(const CFileItemList& items, int iPosition /* = -1 */)
{
  Changes changes;
  {
    std::unique_lock lock(m_critSection);
    if (iPosition < 0 || iPosition > static_cast<int>(m_entries.size()))
      iPosition = static_cast<int>(m_entries.size());
    for (int i = 0; i < items.Size(); i++)
      InsertLocked(items[i], iPosition++, changes);
  }
  Notify(changes);
}

EntryId CPlayList::Insert(const std::shared_ptr<CFileItem>& item, int iPosition /* = -1 */)
{
  Changes changes;
  EntryId entry;
  {
    std::unique_lock lock(m_critSection);
    entry = InsertLocked(item, iPosition, changes);
  }
  Notify(changes);
  return entry;
}

void CPlayList::Clear()
{
  Changes changes;
  {
    std::unique_lock lock(m_critSection);
    if (!m_entries.empty())
      changes.push_back({PlayListChange::Type::Cleared, NO_ENTRY, -1, nullptr});

    m_entries.clear();
    m_current = NO_ENTRY;
    m_requests.clear();
    m_shuffle->Reset({}, NO_ENTRY);
    m_strPlayListName.clear();
    m_sourcePath.clear();
  }
  Notify(changes);
}

int CPlayList::size() const
{
  std::unique_lock lock(m_critSection);
  return static_cast<int>(m_entries.size());
}

std::shared_ptr<CFileItem> CPlayList::operator[](int iItem) const
{
  std::unique_lock lock(m_critSection);
  if (iItem < 0 || iItem >= static_cast<int>(m_entries.size()))
  {
    CLog::Log(LOGERROR, "Error trying to retrieve an item that's out of range");
    return {};
  }
  return m_entries[iItem].item;
}

EntryId CPlayList::GetEntryId(int position) const
{
  std::unique_lock lock(m_critSection);
  if (position < 0 || position >= static_cast<int>(m_entries.size()))
    return NO_ENTRY;
  return m_entries[position].id;
}

int CPlayList::GetPosition(EntryId entry) const
{
  std::unique_lock lock(m_critSection);
  return FindLocked(entry);
}

std::shared_ptr<CFileItem> CPlayList::GetItem(EntryId entry) const
{
  std::unique_lock lock(m_critSection);
  const int position = FindLocked(entry);
  return position < 0 ? nullptr : m_entries[position].item;
}

std::vector<PlayListEntry> CPlayList::GetPlayOrder() const
{
  std::unique_lock lock(m_critSection);
  std::vector<PlayListEntry> order;
  order.reserve(m_entries.size());
  for (EntryId entry = m_shuffle->Following(NO_ENTRY);
       entry != NO_ENTRY && order.size() < m_entries.size(); entry = m_shuffle->Following(entry))
  {
    if (const int position = FindLocked(entry); position >= 0)
      order.emplace_back(m_entries[position]);
  }
  return order;
}

EntryId CPlayList::GetCurrent() const
{
  std::unique_lock lock(m_critSection);
  return m_current;
}

int CPlayList::GetCurrentPosition() const
{
  std::unique_lock lock(m_critSection);
  return FindLocked(m_current);
}

std::shared_ptr<CFileItem> CPlayList::GetCurrentItem() const
{
  std::unique_lock lock(m_critSection);
  const int position = FindLocked(m_current);
  return position < 0 ? nullptr : m_entries[position].item;
}

bool CPlayList::SetCurrent(EntryId entry)
{
  std::unique_lock lock(m_critSection);
  if (FindLocked(entry) < 0)
    return false;

  DropStaleRequestsLocked(m_requests);
  if (!m_requests.empty() && m_requests.front().entry == entry)
    m_requests.pop_front();

  m_current = entry;
  return true;
}

bool CPlayList::SetCurrentPosition(int position)
{
  return SetCurrent(GetEntryId(position));
}

void CPlayList::ClearCurrent()
{
  std::unique_lock lock(m_critSection);
  m_current = NO_ENTRY;
}

void CPlayList::DropStaleRequestsLocked(std::deque<Request>& requests) const
{
  while (!requests.empty() && FindLocked(requests.front().entry) < 0)
    requests.pop_front();
}

EntryId CPlayList::StepLocked(EntryId from, std::deque<Request>& requests, Advance advance) const
{
  DropStaleRequestsLocked(requests);
  if (!requests.empty())
  {
    const EntryId requested = requests.front().entry;
    requests.pop_front();
    return requested;
  }

  if (advance == Advance::Automatic)
  {
    if (const int position = FindLocked(from); position >= 0 && m_entries[position].repeat)
    {
      if (!m_entries[position].playable)
      {
        CLog::Log(LOGERROR, "Playlist: repeating entry is unplayable: {}, path [{}]", from,
                  m_entries[position].item->GetPath());
        return NO_ENTRY;
      }
      return from;
    }
  }

  if (const EntryId following = m_shuffle->Following(from); following != NO_ENTRY)
    return following;

  switch (m_wrap.GetKind())
  {
    case Wrap::Kind::ToStart:
      return m_shuffle->Following(NO_ENTRY);
    case Wrap::Kind::ToEntry:
      return FindLocked(m_wrap.GetTarget()) < 0 ? NO_ENTRY : m_wrap.GetTarget();
    case Wrap::Kind::None:
    default:
      return NO_ENTRY;
  }
}

EntryId CPlayList::PeekNext(Advance advance, int steps /* = 1 */) const
{
  std::unique_lock lock(m_critSection);
  std::deque<Request> requests = m_requests;
  EntryId entry = m_current;
  for (int i = 0; i < steps && (i == 0 || entry != NO_ENTRY); i++)
  {
    entry = StepLocked(entry, requests, advance);
    // Only the first step is the user's; after it the list plays on by itself.
    advance = Advance::Automatic;
  }
  return entry;
}

EntryId CPlayList::PeekOffset(int offset) const
{
  return offset >= 0 ? PeekNext(Advance::Automatic, offset) : PeekPrevious(-offset);
}

EntryId CPlayList::Next(Advance advance)
{
  std::unique_lock lock(m_critSection);
  const EntryId next = StepLocked(m_current, m_requests, advance);
  if (next != NO_ENTRY)
    m_current = next;
  return next;
}

EntryId CPlayList::StepBackLocked(EntryId from) const
{
  if (from == NO_ENTRY)
    return NO_ENTRY;

  if (const EntryId preceding = m_shuffle->Preceding(from); preceding != NO_ENTRY)
    return preceding;

  if (m_wrap.GetKind() == Wrap::Kind::ToStart)
    return m_shuffle->Preceding(NO_ENTRY);

  return NO_ENTRY;
}

EntryId CPlayList::PeekPrevious(int steps /* = 1 */) const
{
  std::unique_lock lock(m_critSection);
  EntryId entry = m_current;
  for (int i = 0; i < steps && entry != NO_ENTRY; i++)
    entry = StepBackLocked(entry);
  return entry;
}

EntryId CPlayList::Previous()
{
  std::unique_lock lock(m_critSection);
  const EntryId previous = PeekPrevious();
  if (previous != NO_ENTRY)
    m_current = previous;
  return previous;
}

void CPlayList::SetRepeat(EntryId entry)
{
  std::unique_lock lock(m_critSection);
  if (const int position = FindLocked(entry); position >= 0)
    m_entries[position].repeat = true;
}

void CPlayList::ClearRepeat(EntryId entry)
{
  std::unique_lock lock(m_critSection);
  if (const int position = FindLocked(entry); position >= 0)
    m_entries[position].repeat = false;
}

void CPlayList::ClearRepeats()
{
  std::unique_lock lock(m_critSection);
  for (auto& entry : m_entries)
    entry.repeat = false;
}

bool CPlayList::IsRepeat(EntryId entry) const
{
  std::unique_lock lock(m_critSection);
  const int position = FindLocked(entry);
  return position >= 0 && m_entries[position].repeat;
}

void CPlayList::SetWrap(const Wrap& wrap)
{
  std::unique_lock lock(m_critSection);
  m_wrap = wrap;
}

Wrap CPlayList::GetWrap() const
{
  std::unique_lock lock(m_critSection);
  return m_wrap;
}

void CPlayList::PlayNext(EntryId entry)
{
  std::unique_lock lock(m_critSection);
  if (FindLocked(entry) >= 0)
    m_requests.push_back({entry, false});
}

EntryId CPlayList::PlayNextLocked(const std::shared_ptr<CFileItem>& item, Changes& changes)
{
  int position = 0;
  const auto lastInsert = std::ranges::find_if(m_requests.rbegin(), m_requests.rend(),
                                               [this](const Request& request)
                                               { return request.inserted && FindLocked(request.entry) >= 0; });
  if (lastInsert != m_requests.rend())
    position = FindLocked(lastInsert->entry) + 1;
  else if (const int current = FindLocked(m_current); current >= 0)
    position = current + 1;

  const EntryId entry = InsertLocked(item, position, changes);
  m_requests.push_back({entry, true});
  return entry;
}

EntryId CPlayList::PlayNext(const std::shared_ptr<CFileItem>& item)
{
  Changes changes;
  EntryId entry;
  {
    std::unique_lock lock(m_critSection);
    entry = PlayNextLocked(item, changes);
  }
  Notify(changes);
  return entry;
}

EntryId CPlayList::PlayNext(const CFileItemList& items)
{
  Changes changes;
  EntryId first = NO_ENTRY;
  {
    std::unique_lock lock(m_critSection);
    for (int i = 0; i < items.Size(); i++)
    {
      const EntryId entry = PlayNextLocked(items[i], changes);
      if (first == NO_ENTRY)
        first = entry;
    }
  }
  Notify(changes);
  return first;
}

void CPlayList::SetShuffle(std::unique_ptr<IPlayListShuffle> shuffle)
{
  std::unique_lock lock(m_critSection);
  const IPlayListShuffle& routine = *shuffle;
  m_shuffled = typeid(routine) != typeid(CPlayListNoShuffle);
  m_shuffle = std::move(shuffle);
  ResetShuffle();
}

void CPlayList::ResetShuffle()
{
  std::unique_lock lock(m_critSection);
  std::vector<EntryId> listOrder;
  listOrder.reserve(m_entries.size());
  for (const auto& entry : m_entries)
    listOrder.emplace_back(entry.id);

  m_shuffle->Reset(listOrder, m_current);
}

void CPlayList::SetShuffled(bool shuffled)
{
  if (shuffled == IsShuffled())
    return;

  if (shuffled)
    SetShuffle(std::make_unique<CPlayListRandomShuffle>());
  else
    SetShuffle(std::make_unique<CPlayListNoShuffle>());
}

bool CPlayList::IsShuffled() const
{
  std::unique_lock lock(m_critSection);
  return m_shuffled;
}

std::string CPlayList::GetName() const
{
  std::unique_lock lock(m_critSection);
  return m_strPlayListName;
}

void CPlayList::SetSourcePath(const std::string& path)
{
  std::unique_lock lock(m_critSection);
  m_sourcePath = path;
}

std::string CPlayList::GetSourcePath() const
{
  std::unique_lock lock(m_critSection);
  return m_sourcePath;
}

void CPlayList::Remove(const std::string& strFileName)
{
  Changes changes;
  {
    std::unique_lock lock(m_critSection);
    for (int position = 0; position < static_cast<int>(m_entries.size());)
    {
      if (m_entries[position].item->GetPath() == strFileName)
        RemoveLocked(position, changes);
      else
        ++position;
    }
  }
  Notify(changes);
}

void CPlayList::Remove(int position)
{
  Changes changes;
  {
    std::unique_lock lock(m_critSection);
    if (position >= 0 && position < static_cast<int>(m_entries.size()))
      RemoveLocked(position, changes);
  }
  Notify(changes);
}

int CPlayList::RemoveDVDItems()
{
  Changes changes;
  {
    std::unique_lock lock(m_critSection);
    for (int position = 0; position < static_cast<int>(m_entries.size());)
    {
      const CFileItem& item = *m_entries[position].item;
      if (MUSIC::IsCDDA(item) || item.IsOnDVD())
        RemoveLocked(position, changes);
      else
        ++position;
    }
  }
  Notify(changes);
  return static_cast<int>(changes.size());
}

bool CPlayList::Swap(int position1, int position2)
{
  Changes changes;
  {
    std::unique_lock lock(m_critSection);
    const int size = static_cast<int>(m_entries.size());
    if (position1 < 0 || position2 < 0 || position1 >= size || position2 >= size)
      return false;

    std::swap(m_entries[position1], m_entries[position2]);
    m_shuffle->OnMoved(m_entries[position1].id, position1);
    m_shuffle->OnMoved(m_entries[position2].id, position2);
    changes.push_back({PlayListChange::Type::Moved, m_entries[position1].id, position1, nullptr});
    changes.push_back({PlayListChange::Type::Moved, m_entries[position2].id, position2, nullptr});
  }
  Notify(changes);
  return true;
}

void CPlayList::SetUnPlayable(EntryId entry)
{
  std::unique_lock lock(m_critSection);
  const int position = FindLocked(entry);
  if (position < 0)
  {
    CLog::Log(LOGWARNING, "Attempt to set unplayable entry {}", entry);
    return;
  }
  m_entries[position].playable = false;
}

bool CPlayList::IsPlayable(EntryId entry) const
{
  std::unique_lock lock(m_critSection);
  const int position = FindLocked(entry);
  return position >= 0 && m_entries[position].playable;
}

int CPlayList::GetPlayable() const
{
  std::unique_lock lock(m_critSection);
  return static_cast<int>(std::ranges::count(m_entries, true, &PlayListEntry::playable));
}

bool CPlayList::Load(const std::string& strFileName)
{
  Clear();
  m_strBasePath = URIUtils::GetDirectory(strFileName);

  CFileStream file;
  if (!file.Open(strFileName))
    return false;

  if (file.GetLength() > 1024*1024)
  {
    CLog::Log(LOGWARNING, "{} - File is larger than 1 MB, most likely not a playlist",
              __FUNCTION__);
    return false;
  }

  return LoadData(file);
}

bool CPlayList::LoadData(std::istream &stream)
{
  // try to read as a string
  std::ostringstream ostr;
  ostr << stream.rdbuf();
  return LoadData(ostr.str());
}

bool CPlayList::LoadData(const std::string& strData)
{
  return false;
}

bool CPlayList::Expand(int position)
{
  const std::shared_ptr<CFileItem> item = (*this)[position];
  if (!item)
    return false;

  std::unique_ptr<CPlayList> playlist (CPlayListFactory::Create(*item.get()));
  if (playlist == nullptr)
    return false;

  std::string path = item->GetDynPath();

  if (!playlist->Load(path))
    return false;

  // remove any item that points back to itself
  for (int i = 0;i<playlist->size();i++)
  {
    if (StringUtils::EqualsNoCase((*playlist)[i]->GetPath(), path))
    {
      playlist->Remove(i);
      i--;
    }
  }

  // @todo
  // never change original path (id) of a file item
  for (int i = 0;i<playlist->size();i++)
  {
    (*playlist)[i]->SetDynPath((*playlist)[i]->GetPath());
    (*playlist)[i]->SetPath(item->GetDynPath());
    // Only propagate parent's start offset if the loaded item doesn't already
    // have its own (e.g. a CUE sheet offset loaded from the playlist file).
    if (!(*playlist)[i]->HasProperty("item_start"))
      (*playlist)[i]->SetStartOffset(item->GetStartOffset());
    if (!(*playlist)[i]->HasProperty("BasePath"))
      (*playlist)[i]->SetProperty("BasePath", playlist->m_strBasePath);
  }

  if (playlist->IsEmpty())
    return false;

  Changes changes;
  {
    std::unique_lock lock(m_critSection);
    if (position >= static_cast<int>(m_entries.size()) || m_entries[position].item != item)
      return false;

    const bool wasCurrent = m_entries[position].id == m_current;
    RemoveLocked(position, changes);

    EntryId first = NO_ENTRY;
    int insertAt = position;
    for (const auto& entry : playlist->m_entries)
    {
      const EntryId added = InsertLocked(entry.item, insertAt++, changes);
      if (first == NO_ENTRY)
        first = added;
    }
    if (wasCurrent)
      m_current = first;
  }
  Notify(changes);
  return true;
}

void CPlayList::UpdateItem(const CFileItem *item)
{
  if (!item) return;

  std::unique_lock lock(m_critSection);
  for (const auto& entry : m_entries)
  {
    const std::shared_ptr<CFileItem>& playlistItem = entry.item;
    if (playlistItem->IsSamePath(item))
    {
      std::string temp = playlistItem->GetPath(); // save path, it may have been altered
      *playlistItem = *item;
      playlistItem->SetPath(temp);
      break;
    }
  }
}

std::string CPlayList::ResolveURL(const std::shared_ptr<CFileItem>& item)
{
  if (MUSIC::IsMusicDb(*item) && item->HasMusicInfoTag())
    return item->GetMusicInfoTag()->GetURL();
  else
    return item->GetDynPath();
}

} // namespace KODI::PLAYLIST
