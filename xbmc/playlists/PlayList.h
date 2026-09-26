/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "PlayListTypes.h"
#include "threads/CriticalSection.h"

#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class CFileItem;
class CFileItemList;

namespace KODI::PLAYLIST
{

class IPlayListShuffle;

/*!
 * \brief One position in a playlist. Two copies of one item are two entries.
 */
struct PlayListEntry
{
  EntryId id{NO_ENTRY};
  std::shared_ptr<CFileItem> item;
  bool repeat{false};
  bool playable{true};
};

/*!
 * \brief What an edit did, reported to the observer once the playlist has released its lock.
 */
struct PlayListChange
{
  enum class Type
  {
    Added,
    Removed,
    Moved,
    Cleared
  };

  Type type;
  EntryId entry{NO_ENTRY};
  int position{-1};
  std::shared_ptr<CFileItem> item;
};

/*!
 * \brief An ordered list of entries, and what plays next.
 *
 * The list is never reordered by shuffling: play order is the business of the installed
 * IPlayListShuffle. The playlist keeps its own cursor, and decides what follows it, in this
 * order: the first queued play-next request, the current entry's repeat mark, what the shuffle
 * says follows, the wrap, and otherwise nothing.
 *
 * Every public method is safe to call from any thread.
 */
class CPlayList
{
public:
  using Observer = std::function<void(const std::vector<PlayListChange>&)>;

  CPlayList();
  virtual ~CPlayList();
  CPlayList(const CPlayList&) = delete;
  CPlayList& operator=(const CPlayList&) = delete;

  virtual bool Load(const std::string& strFileName);
  virtual bool LoadData(std::istream &stream);
  virtual bool LoadData(const std::string& strData);
  virtual void Save(const std::string& strFileName) const {};

  EntryId Add(const std::shared_ptr<CFileItem>& item);
  void Add(const CPlayList& playlist);
  void Add(const CFileItemList& items);

  void Insert(const CPlayList& playlist, int iPosition = -1);
  void Insert(const CFileItemList& items, int iPosition = -1);
  EntryId Insert(const std::shared_ptr<CFileItem>& item, int iPosition = -1);

  std::string GetName() const;

  /*!
   * \brief The playlist file or smart playlist the entries were read from, if any. Cleared with
   * the entries.
   */
  void SetSourcePath(const std::string& path);
  std::string GetSourcePath() const;

  void Remove(const std::string& strFileName);
  void Remove(int position);
  bool Swap(int position1, int position2);
  bool Expand(int position); // expands any playlist at position into this playlist
  void Clear();
  int size() const;
  bool IsEmpty() const { return size() == 0; }
  int RemoveDVDItems();

  std::shared_ptr<CFileItem> operator[](int iItem) const;

  EntryId GetEntryId(int position) const;
  int GetPosition(EntryId entry) const;
  std::shared_ptr<CFileItem> GetItem(EntryId entry) const;

  /*!
   * \return The entries in play order, for presenting. Positions in it are not positions in the
   * list.
   */
  std::vector<PlayListEntry> GetPlayOrder() const;

  EntryId GetCurrent() const;
  int GetCurrentPosition() const;
  std::shared_ptr<CFileItem> GetCurrentItem() const;

  /*!
   * \brief Move the cursor. Consumes the first play-next request if it names this entry.
   * \return false if the entry is not in the playlist, and the cursor is left where it was.
   */
  bool SetCurrent(EntryId entry);
  bool SetCurrentPosition(int position);
  void ClearCurrent();

  /*!
   * \brief What would play after the current entry, without moving to it.
   * \param steps How many entries to look ahead.
   */
  EntryId PeekNext(Advance advance, int steps = 1) const;
  EntryId PeekPrevious(int steps = 1) const;

  /*!
   * \brief The entry the given number of entries ahead of the current one in play order, or
   * behind it for a negative offset, as the list would play on by itself.
   */
  EntryId PeekOffset(int offset) const;

  /*!
   * \brief Move to what plays after the current entry.
   * \return The new current entry, or NO_ENTRY if nothing follows, in which case the cursor is
   * left where it was.
   */
  EntryId Next(Advance advance);
  EntryId Previous();

  void SetRepeat(EntryId entry);
  void ClearRepeat(EntryId entry);
  void ClearRepeats();
  bool IsRepeat(EntryId entry) const;

  void SetWrap(const Wrap& wrap);
  Wrap GetWrap() const;

  /*!
   * \brief Play an entry already in the list next, and carry on from there. The list is not
   * touched. Requests are served first in, first out, and one whose entry is gone is dropped.
   */
  void PlayNext(EntryId entry);

  /*!
   * \brief Insert a new item directly after the current entry, or after the last queued insert,
   * and play it next.
   */
  EntryId PlayNext(const std::shared_ptr<CFileItem>& item);
  /*!
   * \return The first of the entries inserted, or NO_ENTRY if there were none.
   */
  EntryId PlayNext(const CFileItemList& items);

  void SetShuffle(std::unique_ptr<IPlayListShuffle> shuffle);
  void SetShuffled(bool shuffled);
  bool IsShuffled() const;

  /*!
   * \brief Deal the play order again from the whole list, the current entry leading.
   */
  void ResetShuffle();

  void SetUnPlayable(EntryId entry);
  bool IsPlayable(EntryId entry) const;
  int GetPlayable() const;

  void UpdateItem(const CFileItem *item);

  static std::string ResolveURL(const std::shared_ptr<CFileItem>& item);

  /*!
   * \brief Be told of every edit. Called without the playlist's lock held.
   */
  void SetObserver(Observer observer);

protected:
  std::string m_strPlayListName;
  std::string m_strBasePath;
  std::vector<PlayListEntry> m_entries;

private:
  struct Request
  {
    EntryId entry;
    bool inserted;
  };

  using Changes = std::vector<PlayListChange>;

  EntryId InsertLocked(const std::shared_ptr<CFileItem>& item, int position, Changes& changes);
  void RemoveLocked(int position, Changes& changes);
  EntryId PlayNextLocked(const std::shared_ptr<CFileItem>& item, Changes& changes);
  int FindLocked(EntryId entry) const;
  EntryId StepLocked(EntryId from, std::deque<Request>& requests, Advance advance) const;
  EntryId StepBackLocked(EntryId from) const;
  void DropStaleRequestsLocked(std::deque<Request>& requests) const;
  void Notify(const Changes& changes) const;

  mutable CCriticalSection m_critSection;
  EntryId m_lastId{NO_ENTRY};
  EntryId m_current{NO_ENTRY};
  Wrap m_wrap{Wrap::None()};
  std::string m_sourcePath;
  std::deque<Request> m_requests;
  std::unique_ptr<IPlayListShuffle> m_shuffle;
  bool m_shuffled{false};
  Observer m_observer;
};

} // namespace KODI::PLAYLIST
