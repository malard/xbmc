/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "interfaces/json-rpc/AddonsOperations.h"
#include "utils/Variant.h"

#include <gtest/gtest.h>

using namespace JSONRPC;

namespace
{
constexpr const char* NO_SUCH_ADDON = "plugin.video.jsonrpc-no-such-addon";

CVariant Addon()
{
  CVariant params(CVariant::VariantTypeObject);
  params["addonId"] = NO_SUCH_ADDON;
  return params;
}

void ExpectNoSuchAddon(JSONRPC_STATUS status, const CVariant& result)
{
  EXPECT_EQ(NotFound, status);
  EXPECT_EQ("no-such-addon", result["reason"].asString());
  EXPECT_EQ(NO_SUCH_ADDON, result["target"]["addonId"].asString());
}
} // namespace

//! \brief An add-on id nothing has is not found, not a malformed request
TEST(TestAddonsOperations, AnUnknownAddonIsNotFound)
{
  CVariant result;
  ExpectNoSuchAddon(CAddonsOperations::GetAddonDetails(Addon(), result), result);

  CVariant params = Addon();
  params["enabled"] = true;
  result = CVariant();
  ExpectNoSuchAddon(CAddonsOperations::SetAddonEnabled(params, result), result);

  result = CVariant();
  ExpectNoSuchAddon(CAddonsOperations::ExecuteAddon(Addon(), result), result);
}

//! \brief Kodi refusing to disable a required system add-on is a declined change, not a bad request
TEST(TestAddonsOperations, DisablingARequiredAddonIsDeclined)
{
  CVariant params(CVariant::VariantTypeObject);
  params["addonId"] = "game.controller.default";
  params["enabled"] = false;

  CVariant result;
  EXPECT_EQ(Unavailable, CAddonsOperations::SetAddonEnabled(params, result));
  EXPECT_EQ("change-declined", result["reason"].asString());
  EXPECT_EQ("game.controller.default", result["target"]["addonId"].asString());
}
