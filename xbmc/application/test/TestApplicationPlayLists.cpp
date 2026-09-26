/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "FileItem.h"
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
// Two entries, and the Video side's cursor on the second.
void FillVideo(CApplicationPlayLists& playLists)
{
  CPlayList& playList = playLists.GetPlayList(Side::Video);
  playList.Add(std::make_shared<CFileItem>("/video/first.mkv", false));
  const EntryId second = playList.Add(std::make_shared<CFileItem>("/video/second.mkv", false));
  playList.SetCurrent(second);
}
} // namespace

TEST(TestApplicationPlayLists, BothSidesAlwaysHaveAPlayList)
{
  CApplicationPlayLists playLists;
  EXPECT_TRUE(playLists.GetPlayList(Side::Video).empty());
  EXPECT_TRUE(playLists.GetPlayList(Side::Audio).empty());
  EXPECT_NE(&playLists.GetPlayList(Side::Video), &playLists.GetPlayList(Side::Audio));
}

TEST(TestApplicationPlayLists, TheRepeatStateIsReadFromTheMarkAndTheWrap)
{
  using enum CApplicationPlayLists::Repeat;
  CApplicationPlayLists playLists;
  FillVideo(playLists);
  CPlayList& playList = playLists.GetPlayList(Side::Video);

  EXPECT_EQ(Off, playLists.GetRepeat(Side::Video));

  playList.SetWrap(Wrap::ToStart());
  EXPECT_EQ(All, playLists.GetRepeat(Side::Video));

  playList.SetRepeat(playList.GetCurrent());
  EXPECT_EQ(One, playLists.GetRepeat(Side::Video)) << "a marked current entry reads as one";

  playList.SetCurrentPosition(0);
  EXPECT_EQ(All, playLists.GetRepeat(Side::Video)) << "leaving the marked entry drops to the wrap";
}

TEST(TestApplicationPlayLists, ThePlayerIdIsTheOneTheInterfacesPublish)
{
  CApplicationPlayLists playLists;
  EXPECT_EQ(-1, playLists.GetPlayerId());

  playLists.SetPlayingSide(Side::Audio);
  EXPECT_EQ(0, playLists.GetPlayerId());

  playLists.SetPlayingSide(Side::Video);
  EXPECT_EQ(1, playLists.GetPlayerId());
}

TEST(TestApplicationPlayLists, AFilmOnTheVideoSideTakesTheAudioSideWithIt)
{
  CApplicationPlayLists playLists;
  FillVideo(playLists);
  playLists.SetPlayingSide(Side::Video);

  CGUIMessage started(GUI_MSG_PLAYBACK_STARTED, 0, 0);
  playLists.OnMessage(started);

  EXPECT_TRUE(playLists.IsAudioFollowingVideo());
  EXPECT_EQ(CApplicationPlayLists::Phase::Playing, playLists.GetPhase(Side::Video));
  EXPECT_EQ(CApplicationPlayLists::Phase::Playing, playLists.GetPhase(Side::Audio));

  CGUIMessage paused(GUI_MSG_PLAYBACK_PAUSED, 0, 0);
  playLists.OnMessage(paused);
  EXPECT_EQ(CApplicationPlayLists::Phase::Paused, playLists.GetPhase(Side::Audio));

  CGUIMessage stopped(GUI_MSG_PLAYBACK_STOPPED, 0, 0);
  playLists.OnMessage(stopped);
  EXPECT_FALSE(playLists.IsAudioFollowingVideo());
  EXPECT_EQ(CApplicationPlayLists::Phase::Idle, playLists.GetPhase(Side::Audio));
}

// Clearing the playlist the playing item came from does not end playback. The item keeps playing,
// and the stop that eventually ends it still finds a playback to clean up.
TEST(TestApplicationPlayLists, ClearingThePlayingPlayListLeavesThePlaybackToItsStop)
{
  CApplicationPlayLists playLists;
  FillVideo(playLists);
  playLists.SetPlayingSide(Side::Video);

  CGUIMessage started(GUI_MSG_PLAYBACK_STARTED, 0, 0);
  playLists.OnMessage(started);

  playLists.GetPlayList(Side::Video).Clear();
  EXPECT_EQ(NO_ENTRY, playLists.GetPlayList(Side::Video).GetCurrent());
  EXPECT_TRUE(playLists.GetPlayingSide() == Side::Video);

  CGUIMessage stopped(GUI_MSG_PLAYBACK_STOPPED, 0, 0);
  EXPECT_TRUE(playLists.OnMessage(stopped));
  EXPECT_FALSE(playLists.GetPlayingSide());
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

TEST_F(TestApplicationPlayListsSlideShow, ASlideShowHoldsTheVideoSideUnderMusic)
{
  using enum CApplicationPlayLists::Phase;
  CApplicationPlayLists playLists;
  const auto slide = std::make_shared<CFileItem>("/pictures/one.jpg", false);

  playLists.OnSlideShow(CApplicationPlayLists::PlayerEvent::Play, slide, true);
  EXPECT_EQ(Playing, playLists.GetPhase(Side::Video));

  playLists.GetPlayList(Side::Audio).Add(std::make_shared<CFileItem>("/music/one.flac", false));
  playLists.GetPlayList(Side::Audio).SetCurrentPosition(0);
  playLists.SetPlayingSide(Side::Audio);
  CGUIMessage started(GUI_MSG_PLAYBACK_STARTED, 0, 0);
  playLists.OnMessage(started);

  EXPECT_EQ(Playing, playLists.GetPhase(Side::Audio));
  EXPECT_EQ(Playing, playLists.GetPhase(Side::Video)) << "music does not take the slideshow's side";

  playLists.OnSlideShow(CApplicationPlayLists::PlayerEvent::Pause, slide, false);
  EXPECT_EQ(Paused, playLists.GetPhase(Side::Video));
  EXPECT_EQ(Playing, playLists.GetPhase(Side::Audio));

  playLists.OnSlideShow(CApplicationPlayLists::PlayerEvent::Stop, slide, false);
  EXPECT_EQ(Idle, playLists.GetPhase(Side::Video));
}