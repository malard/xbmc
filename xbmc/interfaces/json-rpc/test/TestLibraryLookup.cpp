/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "interfaces/json-rpc/JSONUtils.h"
#include "utils/Variant.h"

#include <gtest/gtest.h>

using namespace JSONRPC;

namespace
{
class CTestLookup : public CJSONUtils
{
public:
  using CJSONUtils::StatusFor;
};
} // namespace

//! \brief An id that resolves to nothing fails as no-such-item, naming the id as the caller gave it
TEST(TestLibraryLookup, AMissingItemIsNoSuchItem)
{
  CVariant result;
  EXPECT_EQ(NotFound,
            CTestLookup::StatusFor(CDatabase::GetResult::NotFound, result, Target("movieId", 3)));
  EXPECT_EQ("no-such-item", result["reason"].asString());
  EXPECT_EQ(3, result["target"]["movieId"].asInteger());
}

TEST(TestLibraryLookup, AFoundItemLeavesTheResultAlone)
{
  CVariant result;
  EXPECT_EQ(OK, CTestLookup::StatusFor(CDatabase::GetResult::Ok, result, Target("movieId", 3)));
  EXPECT_TRUE(result.isNull());
}

TEST(TestLibraryLookup, ADatabaseErrorCarriesNoReason)
{
  CVariant result;
  EXPECT_EQ(InternalError,
            CTestLookup::StatusFor(CDatabase::GetResult::Error, result, Target("movieId", 3)));
  EXPECT_TRUE(result.isNull());
}
