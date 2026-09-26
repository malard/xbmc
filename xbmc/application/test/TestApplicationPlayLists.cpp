/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "FileItem.h"
#include "FileItemList.h"
#include "GUIUserMessages.h"
#include "ServiceBroker.h"
#include "application/ApplicationPlayLists.h"
#include "guilib/GUIMessage.h"
#include "interfaces/AnnouncementManager.h"
#include "playlists/PlayList.h"

#include <memory>

#include <gtest/gtest.h>

using namespace KODI;
using namespace KODI::PLAYLIST;

namespace
{
// Two entries, and the Video playlist's cursor on the second.
void FillVideo(CApplicationPlayLists& playLists)
{
  CPlayList& playList = playLists.GetPlayList(PLAYLIST::Video);
  playList.Add(std::make_shared<CFileItem>("/video/first.mkv", false));
  const EntryId second = playList.Add(std::make_shared<CFileItem>("/video/second.mkv", false));
  playList.SetCurrent(second);
}
} // namespace

TEST(TestApplicationPlayLists, BothPlayListsAlwaysExist)
{
  CApplicationPlayLists playLists;
  EXPECT_TRUE(playLists.GetPlayList(PLAYLIST::Video).empty());
  EXPECT_TRUE(playLists.GetPlayList(PLAYLIST::Audio).empty());
  EXPECT_NE(&playLists.GetPlayList(PLAYLIST::Video), &playLists.GetPlayList(PLAYLIST::Audio));
}

TEST(TestApplicationPlayLists, TheRepeatStateIsReadFromTheMarkAndTheWrap)
{
  using enum CApplicationPlayLists::Repeat;
  CApplicationPlayLists playLists;
  FillVideo(playLists);
  CPlayList& playList = playLists.GetPlayList(PLAYLIST::Video);

  EXPECT_EQ(Off, playLists.GetRepeat(PLAYLIST::Video));

  playList.SetWrap(Wrap::ToStart());
  EXPECT_EQ(All, playLists.GetRepeat(PLAYLIST::Video));

  playList.SetRepeat(playList.GetCurrent());
  EXPECT_EQ(One, playLists.GetRepeat(PLAYLIST::Video)) << "a marked current entry reads as one";

  playList.SetCurrentPosition(0);
  EXPECT_EQ(All, playLists.GetRepeat(PLAYLIST::Video))
      << "leaving the marked entry drops to the wrap";
}

TEST(TestApplicationPlayLists, ThePlayerIdIsTheOneTheInterfacesPublish)
{
  CApplicationPlayLists playLists;
  EXPECT_EQ(-1, playLists.GetPlayerId());

  playLists.SetPlayingType(PLAYLIST::Audio);
  EXPECT_EQ(0, playLists.GetPlayerId());

  playLists.SetPlayingType(PLAYLIST::Video);
  EXPECT_EQ(1, playLists.GetPlayerId());
}

TEST(TestApplicationPlayLists, AFilmOnTheVideoPlayListTakesAudioWithIt)
{
  CApplicationPlayLists playLists;
  FillVideo(playLists);
  playLists.SetPlayingType(PLAYLIST::Video);

  CGUIMessage started(GUI_MSG_PLAYBACK_STARTED, 0, 0);
  playLists.OnMessage(started);

  EXPECT_TRUE(playLists.IsAudioFollowingVideo());
  EXPECT_EQ(CApplicationPlayLists::Phase::Playing, playLists.GetPhase(PLAYLIST::Video));
  EXPECT_EQ(CApplicationPlayLists::Phase::Playing, playLists.GetPhase(PLAYLIST::Audio));

  CGUIMessage paused(GUI_MSG_PLAYBACK_PAUSED, 0, 0);
  playLists.OnMessage(paused);
  EXPECT_EQ(CApplicationPlayLists::Phase::Paused, playLists.GetPhase(PLAYLIST::Audio));

  CGUIMessage stopped(GUI_MSG_PLAYBACK_STOPPED, 0, 0);
  playLists.OnMessage(stopped);
  EXPECT_FALSE(playLists.IsAudioFollowingVideo());
  EXPECT_EQ(CApplicationPlayLists::Phase::Idle, playLists.GetPhase(PLAYLIST::Audio));
}

// Clearing the playlist the playing item came from does not end playback. The item keeps playing,
// and the stop that eventually ends it still finds a playback to clean up.
TEST(TestApplicationPlayLists, ClearingThePlayingPlayListLeavesThePlaybackToItsStop)
{
  CApplicationPlayLists playLists;
  FillVideo(playLists);
  playLists.SetPlayingType(PLAYLIST::Video);

  CGUIMessage started(GUI_MSG_PLAYBACK_STARTED, 0, 0);
  playLists.OnMessage(started);

  playLists.GetPlayList(PLAYLIST::Video).Clear();
  EXPECT_EQ(NO_ENTRY, playLists.GetPlayList(PLAYLIST::Video).GetCurrent());
  EXPECT_TRUE(playLists.GetPlayingType() == PLAYLIST::Video);

  CGUIMessage stopped(GUI_MSG_PLAYBACK_STOPPED, 0, 0);
  EXPECT_TRUE(playLists.OnMessage(stopped));
  EXPECT_FALSE(playLists.GetPlayingType());
}

TEST(TestApplicationPlayLists, APlayListIsPlayingOnlyOnceItHasStarted)
{
  CApplicationPlayLists playLists;
  FillVideo(playLists);
  playLists.SetPlayingType(PLAYLIST::Video);
  EXPECT_FALSE(playLists.IsPlaying(PLAYLIST::Video)) << "chosen but not started";

  CGUIMessage started(GUI_MSG_PLAYBACK_STARTED, 0, 0);
  playLists.OnMessage(started);
  EXPECT_TRUE(playLists.IsPlaying(PLAYLIST::Video));
  EXPECT_FALSE(playLists.IsPlaying(PLAYLIST::Audio)) << "following is not playing its own list";
}

TEST(TestApplicationPlayLists, ThePlayingPositionIsOnlyThePlayingPlayList)
{
  CApplicationPlayLists playLists;
  FillVideo(playLists);
  EXPECT_EQ(-1, playLists.GetPlayingPosition(PLAYLIST::Video));

  playLists.SetPlayingType(PLAYLIST::Video);
  EXPECT_EQ(1, playLists.GetPlayingPosition(PLAYLIST::Video));
  EXPECT_EQ(-1, playLists.GetPlayingPosition(PLAYLIST::Audio));
}

TEST(TestApplicationPlayLists, QueueingReportsWhereTheItemsLanded)
{
  CApplicationPlayLists playLists;
  FillVideo(playLists);
  CFileItemList items;
  items.Add(std::make_shared<CFileItem>("/video/third.mkv", false));

  EXPECT_EQ(2, playLists.Queue(PLAYLIST::Video, items, true)) << "nothing plays, so at the end";

  playLists.SetPlayingType(PLAYLIST::Video);
  CGUIMessage started(GUI_MSG_PLAYBACK_STARTED, 0, 0);
  playLists.OnMessage(started);
  playLists.GetPlayList(PLAYLIST::Video).SetCurrentPosition(0);

  EXPECT_EQ(1, playLists.Queue(PLAYLIST::Video, items, true)) << "straight after the current";
  EXPECT_EQ(4, playLists.Queue(PLAYLIST::Video, items, false));
  EXPECT_EQ(-1, playLists.Queue(PLAYLIST::Video, CFileItemList{}, false));
}
TEST(TestApplicationPlayLists, ItemsNobodyPlacedChooseVideoIfAnyIsVideo)
{
  CFileItemList music;
  music.Add(std::make_shared<CFileItem>("/music/one.flac", false));
  music.Add(std::make_shared<CFileItem>("/music/two.mp3", false));
  EXPECT_EQ(PLAYLIST::Audio, CApplicationPlayLists::ChooseType(music));

  CFileItemList mixed;
  mixed.Add(std::make_shared<CFileItem>("/music/one.flac", false));
  mixed.Add(std::make_shared<CFileItem>("/video/one.mkv", false));
  EXPECT_EQ(PLAYLIST::Video, CApplicationPlayLists::ChooseType(mixed));

  PLAYLIST::CPlayList playList;
  playList.Add(mixed);
  EXPECT_EQ(PLAYLIST::Video, CApplicationPlayLists::ChooseType(playList));
}

// The slideshow publishes, so an announcement manager is registered for the test. An unstarted one
// only queues.
class TestApplicationPlayListsSlideShow : public ::testing::Test
{
protected:
  void SetUp() override
  {
    m_previous = CServiceBroker::GetAnnouncementManager();
    CServiceBroker::RegisterAnnouncementManager(
        std::make_shared<ANNOUNCEMENT::CAnnouncementManager>());
  }

  void TearDown() override
  {
    CServiceBroker::UnregisterAnnouncementManager();
    if (m_previous)
      CServiceBroker::RegisterAnnouncementManager(m_previous);
  }

private:
  std::shared_ptr<ANNOUNCEMENT::CAnnouncementManager> m_previous;
};

TEST_F(TestApplicationPlayListsSlideShow, ASlideShowHoldsVideoUnderMusic)
{
  using enum CApplicationPlayLists::Phase;
  CApplicationPlayLists playLists;
  const auto slide = std::make_shared<CFileItem>("/pictures/one.jpg", false);

  playLists.OnSlideShow(CApplicationPlayLists::PlayerEvent::Play, slide, true);
  EXPECT_EQ(Playing, playLists.GetPhase(PLAYLIST::Video));

  playLists.GetPlayList(PLAYLIST::Audio).Add(std::make_shared<CFileItem>("/music/one.flac", false));
  playLists.GetPlayList(PLAYLIST::Audio).SetCurrentPosition(0);
  playLists.SetPlayingType(PLAYLIST::Audio);
  CGUIMessage started(GUI_MSG_PLAYBACK_STARTED, 0, 0);
  playLists.OnMessage(started);

  EXPECT_EQ(Playing, playLists.GetPhase(PLAYLIST::Audio));
  EXPECT_EQ(Playing, playLists.GetPhase(PLAYLIST::Video))
      << "music does not take the slideshow's type";

  playLists.OnSlideShow(CApplicationPlayLists::PlayerEvent::Pause, slide, false);
  EXPECT_EQ(Paused, playLists.GetPhase(PLAYLIST::Video));
  EXPECT_EQ(Playing, playLists.GetPhase(PLAYLIST::Audio));

  playLists.OnSlideShow(CApplicationPlayLists::PlayerEvent::Stop, slide, false);
  EXPECT_EQ(Idle, playLists.GetPhase(PLAYLIST::Video));
}