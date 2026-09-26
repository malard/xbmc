/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "playlists/SmartPlayList.h"

#include <string>
#include <utility>

#include <gtest/gtest.h>

using namespace KODI;

using Param = std::pair<std::string, PLAYLIST::Type>;

class TestSmartPlayListType : public ::testing::TestWithParam<Param>
{
};

TEST_P(TestSmartPlayListType, PlaysOnThePlayListOfItsType)
{
  PLAYLIST::CSmartPlaylist smartPlayList;
  smartPlayList.SetType(GetParam().first);
  EXPECT_EQ(GetParam().second, smartPlayList.GetPlayListType());
}

INSTANTIATE_TEST_SUITE_P(Types,
                         TestSmartPlayListType,
                         ::testing::Values(Param{"songs", PLAYLIST::Audio},
                                           Param{"albums", PLAYLIST::Audio},
                                           Param{"artists", PLAYLIST::Audio},
                                           Param{"movies", PLAYLIST::Video},
                                           Param{"tvshows", PLAYLIST::Video},
                                           Param{"episodes", PLAYLIST::Video},
                                           Param{"musicvideos", PLAYLIST::Video},
                                           Param{"mixed", PLAYLIST::Video}));
