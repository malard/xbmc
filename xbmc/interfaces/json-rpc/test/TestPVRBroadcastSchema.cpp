/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "ServiceDescription.h"
#include "utils/JSONVariantParser.h"
#include "utils/Variant.h"

#include <set>
#include <string>

#include <gtest/gtest.h>

namespace
{

//! \brief A type's definition, read out of the shipped service description
CVariant Definition(const std::string& type)
{
  for (const char* const entry : JSONRPC::JSONRPC_SERVICE_TYPES)
  {
    // Each entry is one definition without its enclosing braces
    CVariant parsed;
    if (!CJSONVariantParser::Parse("{" + std::string(entry) + "}", parsed))
    {
      continue;
    }

    if (parsed.isMember(type))
    {
      return parsed[type];
    }
  }

  ADD_FAILURE() << type << " is not declared in the service description";
  return {};
}

std::set<std::string> RequestableFields()
{
  std::set<std::string> fields;

  const CVariant& values{Definition("PVR.Fields.Broadcast")["items"]["enum"]};
  for (auto value = values.begin_array(); value != values.end_array(); ++value)
  {
    fields.insert(value->asString());
  }

  return fields;
}

std::set<std::string> DeclaredProperties()
{
  std::set<std::string> properties;

  const CVariant& values{Definition("PVR.Details.Broadcast")["properties"]};
  for (auto value = values.begin_map(); value != values.end_map(); ++value)
  {
    properties.insert(value->first);
  }

  return properties;
}

} // unnamed namespace

/*!
 A field a caller may ask for that the details type does not declare arrives
 with no documented type or meaning. Half a field addition landing on its own
 says nothing at runtime, in either direction.
 */
TEST(TestPVRBroadcastSchema, EveryRequestableFieldIsDeclared)
{
  const std::set<std::string> properties{DeclaredProperties()};

  for (const std::string& field : RequestableFields())
  {
    EXPECT_TRUE(properties.contains(field)) << "PVR.Fields.Broadcast offers \"" << field
                                            << "\", which PVR.Details.Broadcast does not declare";
  }
}

TEST(TestPVRBroadcastSchema, EveryDeclaredPropertyIsRequestable)
{
  const std::set<std::string> fields{RequestableFields()};

  for (const std::string& property : DeclaredProperties())
  {
    // CFileItemHandler::HandleFileItem answers the identifier itself, so it is
    // not one of the requestable fields
    if (property == "broadcastid")
    {
      continue;
    }

    EXPECT_TRUE(fields.contains(property))
        << "PVR.Details.Broadcast declares \"" << property << "\", which no caller can request";
  }
}

/*!
 A recording is reachable from its broadcast as a library item, so PVR.GetRecordingDetails
 can follow it.
 */
TEST(TestPVRBroadcastSchema, TheRecordingIsAddressableByItsIdentifier)
{
  EXPECT_TRUE(RequestableFields().contains("recordingid"));
  EXPECT_TRUE(DeclaredProperties().contains("recordingid"));
}
