/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "interfaces/json-rpc/PlaylistOperations.h"
#include "utils/Variant.h"

#include <gtest/gtest.h>

using namespace JSONRPC;

namespace
{
CVariant PicturePlayList()
{
  CVariant params(CVariant::VariantTypeObject);
  params["playlist"] = "picture";
  return params;
}

void ExpectNotApplicableToPictures(const CVariant& result)
{
  EXPECT_EQ("not-applicable", result["reason"].asString());
  EXPECT_EQ("picture", result["target"]["playlist"].asString());
}
} // namespace

//! \brief The slideshow's list has no repeat, order or queue of its own to edit
TEST(TestPlaylistOperations, EditsThePicturePlayListCannotTakeAreNotApplicable)
{
  CVariant params = PicturePlayList();
  params["repeat"] = "all";
  CVariant result;
  EXPECT_EQ(FailedToExecute, CPlaylistOperations::SetRepeat(params, result));
  ExpectNotApplicableToPictures(result);

  params = PicturePlayList();
  params["position"] = 0;
  result = CVariant();
  EXPECT_EQ(FailedToExecute, CPlaylistOperations::Remove(params, result));
  ExpectNotApplicableToPictures(result);

  params = PicturePlayList();
  params["position1"] = 0;
  params["position2"] = 1;
  result = CVariant();
  EXPECT_EQ(FailedToExecute, CPlaylistOperations::Swap(params, result));
  ExpectNotApplicableToPictures(result);
}
