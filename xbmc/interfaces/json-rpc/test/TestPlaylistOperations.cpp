/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "filesystem/SpecialProtocol.h"
#include "interfaces/json-rpc/PlaylistOperations.h"
#include "utils/Variant.h"

#include <string>

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

CVariant AddFiles(std::initializer_list<std::string> files)
{
  CVariant params(CVariant::VariantTypeObject);
  params["playlist"] = "video";
  params["item"] = CVariant(CVariant::VariantTypeArray);
  for (const std::string& file : files)
  {
    CVariant item(CVariant::VariantTypeObject);
    item["file"] = file;
    params["item"].push_back(item);
  }
  return params;
}

// a local path, since a URL is left to the player to resolve
std::string Missing(const std::string& name)
{
  return CSpecialProtocol::TranslatePath("special://temp/jsonrpc-playlist-missing/" + name);
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

//! \brief A call that adds nothing fails for the reason its first missing item gives, as its entry does
TEST(TestPlaylistOperations, NothingAddedFailsForTheFirstMissingItemsReason)
{
  const std::string first{Missing("a.mkv")};
  const std::string second{Missing("b.mkv")};

  CVariant result;
  EXPECT_EQ(NotFound, CPlaylistOperations::Add(AddFiles({first, second}), result));
  EXPECT_EQ("no-such-path", result["reason"].asString());
  EXPECT_EQ(first, result["target"]["file"].asString());
}

//! \brief An item that names nothing Kodi can resolve is not playable
TEST(TestPlaylistOperations, AnUnresolvableItemIsNotPlayable)
{
  CVariant params(CVariant::VariantTypeObject);
  params["playlist"] = "video";
  params["item"]["file"] = "";

  CVariant result;
  EXPECT_EQ(InvalidParams, CPlaylistOperations::Add(params, result));
  EXPECT_EQ("not-playable", result["reason"].asString());
}
