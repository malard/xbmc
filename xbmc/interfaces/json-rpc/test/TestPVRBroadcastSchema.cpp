/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "JSONRPCTestUtils.h"
#include "ServiceDescription.h"
#include "utils/Variant.h"

#include <set>
#include <string>

#include <gtest/gtest.h>

using namespace JSONRPC;

namespace
{

std::set<std::string> RequestableFields()
{
  return EnumValues(ShippedType("PVR.Fields.Broadcast")["items"]);
}

std::set<std::string> DeclaredProperties()
{
  return Keys(ShippedType("PVR.Details.Broadcast")["properties"]);
}

} // unnamed namespace

TEST(TestPVRBroadcastSchema, EveryDeclaredPropertyIsRequestable)
{
  const std::set<std::string> fields{RequestableFields()};

  for (const std::string& property : DeclaredProperties())
  {
    // CFileItemHandler::HandleFileItem answers the identifier itself, so it is
    // not one of the requestable fields
    if (property == "broadcastId")
    {
      continue;
    }

    EXPECT_TRUE(fields.contains(property))
        << "PVR.Details.Broadcast declares \"" << property << "\", which no caller can request";
  }
}
