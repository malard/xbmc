/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "ServiceDescription.h"
#include "addons/kodi-dev-kit/include/kodi/c-api/addon-instance/pvr/pvr_epg.h"
#include "pvr/recordings/PVRRecording.h"
#include "utils/JSONVariantParser.h"
#include "utils/Variant.h"

#include <set>
#include <string>

#include <gtest/gtest.h>

using namespace PVR;

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
      continue;

    if (parsed.isMember(type))
      return parsed[type];
  }

  ADD_FAILURE() << type << " is not declared in the service description";
  return {};
}

std::set<std::string> RequestableFields()
{
  std::set<std::string> fields;

  const CVariant& values{Definition("PVR.Fields.Recording")["items"]["enum"]};
  for (auto value = values.begin_array(); value != values.end_array(); ++value)
    fields.insert(value->asString());

  return fields;
}

std::set<std::string> DeclaredProperties()
{
  std::set<std::string> properties;

  const CVariant& values{Definition("PVR.Details.Recording")["properties"]};
  for (auto value = values.begin_map(); value != values.end_map(); ++value)
    properties.insert(value->first);

  return properties;
}

/*!
 \brief A recording as a client reports one.

 The channel type is stated so that the recording does not have to consult the
 PVR manager for it, which no test has running.
 */
CPVRRecording ClientRecording()
{
  PVR_RECORDING recording{};
  recording.strRecordingId = "0815";
  recording.strTitle = "Heroes";
  recording.strEpisodeName = "Genesis";
  recording.strTitleExtraInfo = "Drama, USA, 2006";
  recording.strChannelName = "Example Channel";
  recording.channelType = PVR_RECORDING_CHANNEL_TYPE_TV;
  recording.iGenreType = EPG_GENRE_USE_STRING;
  recording.strGenreDescription = "Drama";
  recording.iSeriesNumber = 1;
  recording.iEpisodeNumber = 1;
  recording.iEpisodePartNumber = 2;
  return {recording, 1};
}

} // unnamed namespace

TEST(TestPVRRecordingSchema, EveryValueTheRecordingAddsIsRequestable)
{
  // CPVRRecording::Serialize starts from CVideoInfoTag::Serialize, which writes the whole
  // video tag surface whether a recording has anything to say through it or not. Those keys
  // answer to the video library's Fields types, so what has to be reachable here is what the
  // recording adds on top of them.
  CVariant base;
  CVideoInfoTag{}.Serialize(base);

  const CPVRRecording recording{ClientRecording()};

  CVariant serialized;
  recording.Serialize(serialized);

  const std::set<std::string> fields{RequestableFields()};

  for (auto value = serialized.begin_map(); value != serialized.end_map(); ++value)
  {
    if (base.isMember(value->first))
      continue;

    // CPVROperations answers the identifier itself, so it is not one of the
    // requestable fields
    if (value->first == "recordingid")
      continue;

    EXPECT_TRUE(fields.contains(value->first)) << "CPVRRecording::Serialize writes \""
                                               << value->first << "\", which no caller can request";
  }
}

TEST(TestPVRRecordingSchema, EveryRequestableFieldIsDeclared)
{
  const std::set<std::string> declared{DeclaredProperties()};

  for (const std::string& field : RequestableFields())
  {
    EXPECT_TRUE(declared.contains(field)) << "PVR.Fields.Recording offers \"" << field
                                          << "\", which PVR.Details.Recording does not declare";
  }
}
