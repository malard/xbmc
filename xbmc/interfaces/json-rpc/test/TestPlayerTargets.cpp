/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "interfaces/json-rpc/PlayerOperations.h"
#include "utils/Variant.h"

#include <gtest/gtest.h>

using namespace JSONRPC;

namespace
{
CVariant Named(const char* playlist)
{
  CVariant params(CVariant::VariantTypeObject);
  params["playlist"] = playlist;
  return params;
}
} // namespace

TEST(TestPlayerTargets, AVerbOnThePlayerFailsWhenTheNamedPlayListIsIdle)
{
  CVariant result;
  EXPECT_EQ(FailedToExecute, CPlayerOperations::Stop(Named("audio"), result));
  EXPECT_EQ(FailedToExecute, CPlayerOperations::PlayPause(Named("video"), result));
}
