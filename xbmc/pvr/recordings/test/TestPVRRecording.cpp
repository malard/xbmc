/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "addons/kodi-dev-kit/include/kodi/c-api/addon-instance/pvr/pvr_epg.h"
#include "pvr/recordings/PVRRecording.h"
#include "utils/Variant.h"

#include <string>

#include <gtest/gtest.h>

using namespace PVR;

namespace
{

constexpr unsigned int CLIENT_ID{1};

/*!
 \brief A recording as a client reports one.

 The channel type is stated so that the recording does not have to consult the
 PVR manager for it, which no test has running.
 */
PVR_RECORDING ClientRecording(const char* title, const char* episodeName)
{
  PVR_RECORDING recording{};
  recording.strRecordingId = "0815";
  recording.strTitle = title;
  recording.strEpisodeName = episodeName;
  recording.strChannelName = "Example Channel";
  recording.channelType = PVR_RECORDING_CHANNEL_TYPE_TV;
  recording.iGenreType = EPG_GENRE_USE_STRING;
  recording.strGenreDescription = "Drama";
  return recording;
}

} // unnamed namespace

/*!
 A client reports a programme title plus an episode name, which is the opposite
 way round to the CVideoInfoTag members a recording inherits. Playing the same
 episode from the library and from the client used to answer every generic
 caller - JSON-RPC and the info labels alike - with the two swapped.
 */
TEST(TestPVRRecording, AnEpisodeDescribesItselfAsAScannedOneDoes)
{
  const CPVRRecording recording{ClientRecording("Heroes", "Genesis"), CLIENT_ID};

  EXPECT_EQ("Genesis", recording.m_strTitle);
  EXPECT_EQ("Heroes", recording.m_strShowTitle);

  CVariant serialized;
  recording.Serialize(serialized);

  EXPECT_EQ("Genesis", serialized["title"].asString());
  EXPECT_EQ("Heroes", serialized["showtitle"].asString());
}

/*!
 The PVR side of Kodi still has to reach what the client actually sent, and
 renaming a recording renames the programme rather than the episode.
 */
TEST(TestPVRRecording, AnEpisodeStillReportsWhatTheClientSent)
{
  CPVRRecording recording{ClientRecording("Heroes", "Genesis"), CLIENT_ID};

  EXPECT_EQ("Heroes", recording.ProgrammeTitle());
  EXPECT_EQ("Genesis", recording.EpisodeName());

  recording.SetProgrammeTitle("Heroes Reborn");

  EXPECT_EQ("Heroes Reborn", recording.ProgrammeTitle());
  EXPECT_EQ("Genesis", recording.EpisodeName());
}

/*!
 A recording of anything that is not an episode has its own title and belongs
 to no show, exactly as a scanned film does.
 */
TEST(TestPVRRecording, ARecordingThatIsNotAnEpisodeBelongsToNoShow)
{
  CPVRRecording recording{ClientRecording("Casablanca", ""), CLIENT_ID};

  EXPECT_EQ("Casablanca", recording.m_strTitle);
  EXPECT_EQ("", recording.m_strShowTitle);
  EXPECT_EQ("Casablanca", recording.ProgrammeTitle());
  EXPECT_EQ("", recording.EpisodeName());

  recording.SetProgrammeTitle("Casablanca (1942)");

  EXPECT_EQ("Casablanca (1942)", recording.ProgrammeTitle());
  EXPECT_EQ("Casablanca (1942)", recording.m_strTitle);
}

/*!
 The path is this recording's key into the video database, where its play
 count, resume point and stream details are stored. It is built from the
 programme title and then the episode name, which is what the client sent and
 what it was built from before those two moved between members - so every
 stored playback state on an existing installation still keys on it.
 */
TEST(TestPVRRecording, ThePathKeysOnTheProgrammeTitleAndThenTheEpisodeName)
{
  PVR_RECORDING clientEpisode{ClientRecording("Heroes", "Genesis")};
  clientEpisode.iSeriesNumber = 1;
  clientEpisode.iEpisodeNumber = 1;
  const CPVRRecording episode{clientEpisode, CLIENT_ID};

  EXPECT_EQ("pvr://recordings/tv/active/Heroes s01e01%20Genesis, TV%20(Example%20Channel), "
            "19700101_000000, 0815.pvr",
            episode.m_strFileNameAndPath);

  const CPVRRecording film{ClientRecording("Casablanca", ""), CLIENT_ID};

  EXPECT_EQ("pvr://recordings/tv/active/Casablanca, TV%20(Example%20Channel), "
            "19700101_000000, 0815.pvr",
            film.m_strFileNameAndPath);
}
