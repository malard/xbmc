/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "ServiceDescription.h"
#include "utils/JSONVariantParser.h"
#include "utils/StreamDetails.h"
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
    CVariant parsed;
    if (!CJSONVariantParser::Parse("{" + std::string(entry) + "}", parsed))
      continue;

    if (parsed.isMember(type))
      return parsed[type];
  }

  ADD_FAILURE() << type << " is not declared in the service description";
  return {};
}

std::set<std::string> Keys(const CVariant& object)
{
  std::set<std::string> keys;
  for (auto it = object.begin_map(); it != object.end_map(); ++it)
    keys.insert(it->first);
  return keys;
}

std::set<std::string> DeclaredProperties(const std::string& kind)
{
  return Keys(Definition("Video.Streams")["properties"][kind]["items"]["properties"]);
}

std::set<std::string> SerializedProperties(const CStreamDetail& detail)
{
  CVariant value(CVariant::VariantTypeObject);
  detail.Serialize(value);
  return Keys(value);
}
} // namespace

TEST(TestVideoStreamsSchema, VideoDeclaresWhatTheSerializerEmits)
{
  EXPECT_EQ(SerializedProperties(CStreamDetailVideo{}), DeclaredProperties("video"));
}

TEST(TestVideoStreamsSchema, AudioDeclaresWhatTheSerializerEmits)
{
  EXPECT_EQ(SerializedProperties(CStreamDetailAudio{}), DeclaredProperties("audio"));
}

TEST(TestVideoStreamsSchema, SubtitleDeclaresWhatTheSerializerEmits)
{
  EXPECT_EQ(SerializedProperties(CStreamDetailSubtitle{}), DeclaredProperties("subtitle"));
}
