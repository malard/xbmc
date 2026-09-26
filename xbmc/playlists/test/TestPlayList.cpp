/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "FileItem.h"
#include "FileItemList.h"
#include "playlists/PlayList.h"
#include "playlists/PlayListShuffle.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace KODI;
using namespace KODI::PLAYLIST;

namespace
{
std::shared_ptr<CFileItem> Item(const std::string& name)
{
  return std::make_shared<CFileItem>("/media/" + name + ".mkv", false);
}

// Plays the list backwards, so that play order and list order are told apart.
class CReverseShuffle : public CPlayListNoShuffle
{
public:
  EntryId Following(EntryId entry) const override { return CPlayListNoShuffle::Preceding(entry); }
  EntryId Preceding(EntryId entry) const override { return CPlayListNoShuffle::Following(entry); }
};

class TestPlayList : public ::testing::Test
{
protected:
  // Four entries, a to d, with ids kept in list order.
  void SetUp() override
  {
    for (const char* name : {"a", "b", "c", "d"})
      m_ids.emplace_back(m_playList.Add(Item(name)));
  }

  std::string NameOf(EntryId entry) const
  {
    const auto item = m_playList.GetItem(entry);
    return item ? item->GetPath().substr(7, 1) : "-";
  }

  std::string PlayOrder() const
  {
    std::string order;
    for (const PlayListEntry& entry : m_playList.GetPlayOrder())
      order += NameOf(entry.id);
    return order;
  }

  CPlayList m_playList;
  std::vector<EntryId> m_ids;
};
} // namespace

TEST_F(TestPlayList, EntryIdsAreNeverReused)
{
  const EntryId removed = m_ids[3];
  m_playList.Remove(3);
  const EntryId added = m_playList.Add(Item("e"));

  EXPECT_NE(removed, added);
  EXPECT_EQ(-1, m_playList.GetPosition(removed));

  m_playList.Clear();
  EXPECT_NE(added, m_playList.Add(Item("f")));
}

TEST_F(TestPlayList, TwoCopiesOfOneItemAreTwoEntries)
{
  const auto item = Item("e");
  const EntryId first = m_playList.Add(item);
  const EntryId second = m_playList.Add(item);

  EXPECT_NE(first, second);
  m_playList.SetRepeat(second);
  EXPECT_FALSE(m_playList.IsRepeat(first));
  EXPECT_TRUE(m_playList.IsRepeat(second));
}

TEST_F(TestPlayList, NextFromNoCurrentEntryGivesTheFirst)
{
  EXPECT_EQ(m_ids[0], m_playList.Next(Advance::Automatic));
  EXPECT_EQ(m_ids[0], m_playList.GetCurrent());
}

TEST_F(TestPlayList, NextFollowsListOrderAndStopsAtTheEnd)
{
  m_playList.SetCurrent(m_ids[2]);
  EXPECT_EQ(m_ids[3], m_playList.Next(Advance::Automatic));
  EXPECT_EQ(NO_ENTRY, m_playList.Next(Advance::Automatic));
  EXPECT_EQ(m_ids[3], m_playList.GetCurrent()) << "a failed Next() must leave the cursor alone";
}

TEST_F(TestPlayList, PeekingDoesNotMove)
{
  m_playList.SetCurrent(m_ids[1]);
  EXPECT_EQ(m_ids[2], m_playList.PeekNext(Advance::Automatic));
  EXPECT_EQ(m_ids[3], m_playList.PeekNext(Advance::Automatic, 2));
  EXPECT_EQ(m_ids[1], m_playList.GetCurrent());
}

TEST_F(TestPlayList, ARepeatingEntryRepeatsUntilTheUserSkips)
{
  m_playList.SetCurrent(m_ids[1]);
  m_playList.SetRepeat(m_ids[1]);

  EXPECT_EQ(m_ids[1], m_playList.Next(Advance::Automatic));
  EXPECT_EQ(m_ids[2], m_playList.Next(Advance::User));
  EXPECT_TRUE(m_playList.IsRepeat(m_ids[1])) << "a skip leaves the mark in place";
}

TEST_F(TestPlayList, ARepeatingEntryThatWillNotPlayStops)
{
  m_playList.SetCurrent(m_ids[1]);
  m_playList.SetRepeat(m_ids[1]);
  m_playList.SetUnPlayable(m_ids[1]);

  EXPECT_EQ(NO_ENTRY, m_playList.PeekNext(Advance::Automatic));
  EXPECT_EQ(m_ids[2], m_playList.PeekNext(Advance::User));
}

TEST_F(TestPlayList, ClearRepeatsClearsEveryMark)
{
  m_playList.SetRepeat(m_ids[0]);
  m_playList.SetRepeat(m_ids[2]);
  m_playList.ClearRepeats();

  EXPECT_FALSE(m_playList.IsRepeat(m_ids[0]));
  EXPECT_FALSE(m_playList.IsRepeat(m_ids[2]));
}

TEST_F(TestPlayList, WrapToStartGoesBackToTheFirstEntry)
{
  m_playList.SetWrap(Wrap::ToStart());
  m_playList.SetCurrent(m_ids[3]);
  EXPECT_EQ(m_ids[0], m_playList.Next(Advance::Automatic));
}

TEST_F(TestPlayList, WrapToAnEntryGoesThere)
{
  m_playList.SetWrap(Wrap::To(m_ids[2]));
  m_playList.SetCurrent(m_ids[3]);
  EXPECT_EQ(m_ids[2], m_playList.Next(Advance::Automatic));
}

TEST_F(TestPlayList, WrapToARemovedEntryStops)
{
  m_playList.SetWrap(Wrap::To(m_ids[2]));
  m_playList.Remove(2);
  m_playList.SetCurrent(m_ids[3]);
  EXPECT_EQ(NO_ENTRY, m_playList.Next(Advance::Automatic));
}

TEST_F(TestPlayList, PreviousWrapsOnlyWhenTheListWraps)
{
  m_playList.SetCurrent(m_ids[0]);
  EXPECT_EQ(NO_ENTRY, m_playList.PeekPrevious());

  m_playList.SetWrap(Wrap::ToStart());
  EXPECT_EQ(m_ids[3], m_playList.PeekPrevious());
}

TEST_F(TestPlayList, APlayNextRequestJumpsAndCarriesOnFromThere)
{
  m_playList.SetCurrent(m_ids[0]);
  m_playList.PlayNext(m_ids[2]);

  EXPECT_EQ(m_ids[2], m_playList.Next(Advance::Automatic));
  EXPECT_EQ(m_ids[3], m_playList.Next(Advance::Automatic));
}

TEST_F(TestPlayList, PlayNextRequestsAreFirstInFirstOut)
{
  m_playList.SetCurrent(m_ids[0]);
  m_playList.PlayNext(m_ids[3]);
  m_playList.PlayNext(m_ids[1]);

  EXPECT_EQ(m_ids[3], m_playList.Next(Advance::Automatic));
  EXPECT_EQ(m_ids[1], m_playList.Next(Advance::Automatic));
}

TEST_F(TestPlayList, APlayNextRequestComesBeforeTheRepeatMark)
{
  m_playList.SetCurrent(m_ids[0]);
  m_playList.SetRepeat(m_ids[0]);
  m_playList.PlayNext(m_ids[2]);

  EXPECT_EQ(m_ids[2], m_playList.Next(Advance::Automatic));
}

TEST_F(TestPlayList, ARequestForARemovedEntryIsDropped)
{
  m_playList.SetCurrent(m_ids[0]);
  m_playList.PlayNext(m_ids[2]);
  m_playList.Remove(2);

  EXPECT_EQ(m_ids[1], m_playList.Next(Advance::Automatic));
}

TEST_F(TestPlayList, StartingTheRequestedEntryConsumesTheRequest)
{
  m_playList.SetCurrent(m_ids[0]);
  m_playList.PlayNext(m_ids[2]);

  // What a player queueing its gapless successor does: look, then move once it has started.
  const EntryId next = m_playList.PeekNext(Advance::Automatic);
  ASSERT_EQ(m_ids[2], next);
  m_playList.SetCurrent(next);

  EXPECT_EQ(m_ids[3], m_playList.PeekNext(Advance::Automatic));
}

TEST_F(TestPlayList, NewItemsToPlayNextGoAfterTheCurrentEntryInOrder)
{
  m_playList.SetCurrent(m_ids[1]);
  const EntryId e = m_playList.PlayNext(Item("e"));
  const EntryId f = m_playList.PlayNext(Item("f"));

  EXPECT_EQ(2, m_playList.GetPosition(e));
  EXPECT_EQ(3, m_playList.GetPosition(f)) << "a later insert sits after the earlier one";
  EXPECT_EQ(e, m_playList.Next(Advance::Automatic));
  EXPECT_EQ(f, m_playList.Next(Advance::Automatic));
  EXPECT_EQ(m_ids[2], m_playList.Next(Advance::Automatic));
}

TEST_F(TestPlayList, NewItemsToPlayNextComeFirstUnderShuffle)
{
  m_playList.SetCurrent(m_ids[1]);
  m_playList.SetShuffle(std::make_unique<CReverseShuffle>());
  CFileItemList items;
  items.Add(Item("e"));
  m_playList.PlayNext(items);

  EXPECT_EQ("e", NameOf(m_playList.Next(Advance::Automatic)));
}

TEST_F(TestPlayList, ShufflingNeverReordersTheList)
{
  m_playList.SetShuffle(std::make_unique<CReverseShuffle>());

  for (int i = 0; i < 4; i++)
    EXPECT_EQ(m_ids[i], m_playList.GetEntryId(i));
  EXPECT_TRUE(m_playList.IsShuffled());
  EXPECT_EQ("dcba", PlayOrder());
}

TEST_F(TestPlayList, NextFollowsTheShuffle)
{
  m_playList.SetShuffle(std::make_unique<CReverseShuffle>());
  m_playList.SetCurrent(m_ids[2]);
  EXPECT_EQ(m_ids[1], m_playList.Next(Advance::Automatic));
}

TEST_F(TestPlayList, TurningShuffleOffLeavesTheCursorWhereItWas)
{
  m_playList.SetShuffled(true);
  m_playList.SetCurrent(m_ids[2]);
  m_playList.SetShuffled(false);

  EXPECT_FALSE(m_playList.IsShuffled());
  EXPECT_EQ(m_ids[2], m_playList.GetCurrent());
  EXPECT_EQ("abcd", PlayOrder());
}

TEST_F(TestPlayList, RemovingTheCurrentEntryKeepsWhatFollowedIt)
{
  m_playList.SetCurrent(m_ids[1]);
  m_playList.Remove(1);
  EXPECT_EQ(m_ids[2], m_playList.Next(Advance::Automatic));
}

TEST_F(TestPlayList, ClearingLeavesNoCurrentEntry)
{
  m_playList.SetCurrent(m_ids[1]);
  m_playList.PlayNext(m_ids[3]);
  m_playList.Clear();

  EXPECT_EQ(NO_ENTRY, m_playList.GetCurrent());
  const EntryId added = m_playList.Add(Item("e"));
  EXPECT_EQ(added, m_playList.Next(Advance::Automatic)) << "the old request went with the list";
}

TEST_F(TestPlayList, SwapMovesEntriesNotTheCursor)
{
  m_playList.SetCurrent(m_ids[0]);
  ASSERT_TRUE(m_playList.Swap(0, 3));

  EXPECT_EQ(m_ids[0], m_playList.GetCurrent());
  EXPECT_EQ(3, m_playList.GetCurrentPosition());
  EXPECT_EQ("dbca", PlayOrder());
}

TEST_F(TestPlayList, PeekOffsetLooksBothWays)
{
  m_playList.SetCurrent(m_ids[2]);
  EXPECT_EQ(m_ids[2], m_playList.PeekOffset(0));
  EXPECT_EQ(m_ids[3], m_playList.PeekOffset(1));
  EXPECT_EQ(m_ids[0], m_playList.PeekOffset(-2));
  EXPECT_EQ(NO_ENTRY, m_playList.PeekOffset(2));
}

TEST_F(TestPlayList, GetPlayableCountsEntries)
{
  EXPECT_EQ(4, m_playList.GetPlayable());
  m_playList.SetUnPlayable(m_ids[0]);
  EXPECT_EQ(3, m_playList.GetPlayable());
  EXPECT_FALSE(m_playList.IsPlayable(m_ids[0]));
}

TEST_F(TestPlayList, TheObserverHearsEachEditAfterTheLockIsReleased)
{
  std::vector<PlayListChange> seen;
  m_playList.SetObserver(
      [this, &seen](const std::vector<PlayListChange>& changes)
      {
        // Reading the playlist from the observer must not deadlock or see a half-done edit.
        EXPECT_GE(m_playList.size(), 0);
        seen.insert(seen.end(), changes.begin(), changes.end());
      });

  const EntryId added = m_playList.Add(Item("e"));
  m_playList.Remove(0);
  m_playList.Clear();

  ASSERT_EQ(3u, seen.size());
  EXPECT_EQ(PlayListChange::Type::Added, seen[0].type);
  EXPECT_EQ(added, seen[0].entry);
  EXPECT_EQ(4, seen[0].position);
  EXPECT_EQ(PlayListChange::Type::Removed, seen[1].type);
  EXPECT_EQ(m_ids[0], seen[1].entry);
  EXPECT_EQ(PlayListChange::Type::Cleared, seen[2].type);
}

TEST(TestPlayListRandomShuffle, TheCurrentEntryLeadsAndEveryEntryIsPlayedOnce)
{
  CPlayListRandomShuffle shuffle;
  const std::vector<EntryId> listOrder{1, 2, 3, 4, 5, 6, 7, 8};
  shuffle.Reset(listOrder, 5);

  std::vector<EntryId> order;
  for (EntryId entry = shuffle.Following(NO_ENTRY); entry != NO_ENTRY;
       entry = shuffle.Following(entry))
    order.emplace_back(entry);

  ASSERT_EQ(listOrder.size(), order.size());
  EXPECT_EQ(5u, order.front());
  std::ranges::sort(order);
  EXPECT_EQ(listOrder, order);
}

TEST(TestPlayListRandomShuffle, AnAddedEntryComesAfterTheCurrentOne)
{
  for (int attempt = 0; attempt < 20; attempt++)
  {
    CPlayListRandomShuffle shuffle;
    shuffle.Reset({1, 2, 3, 4}, 3);
    shuffle.OnAdded(9, 4, 3);

    bool seenCurrent = false;
    for (EntryId entry = shuffle.Following(NO_ENTRY); entry != NO_ENTRY;
         entry = shuffle.Following(entry))
    {
      if (entry == 3)
        seenCurrent = true;
      if (entry == 9)
        EXPECT_TRUE(seenCurrent) << "the new entry was placed among those already played";
    }
  }
}
