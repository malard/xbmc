/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "interfaces/json-rpc/FileOperations.h"
#include "utils/Variant.h"

#include <initializer_list>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

using namespace JSONRPC;

namespace
{
CVariant ParamsWithProperties(std::initializer_list<std::string_view> properties)
{
  CVariant params(CVariant::VariantTypeObject);
  params["properties"] = CVariant(CVariant::VariantTypeArray);
  for (const std::string_view property : properties)
    params["properties"].append(std::string(property));
  return params;
}

CVariant File(const std::string& path)
{
  CVariant params(CVariant::VariantTypeObject);
  params["file"] = path;
  return params;
}
} // unnamed namespace

//! \brief A shared file that does not exist is no-such-path, naming the path as given
TEST(TestFileOperations, AMissingFileIsNoSuchPath)
{
  // add-on data is always shared, so only the file's absence can refuse it
  const std::string path{"special://profile/addon_data/jsonrpc-no-such-file.txt"};

  CVariant result;
  EXPECT_EQ(NotFound, CFileOperations::GetFileDetails(File(path), result));
  EXPECT_EQ("no-such-path", result["reason"].asString());
  EXPECT_EQ(path, result["target"]["file"].asString());
}

TEST(TestFileOperations, MissingPropertiesDoesNotNeedLibraryLookup)
{
  const CVariant params(CVariant::VariantTypeObject);
  EXPECT_FALSE(CFileOperations::NeedsLibraryLookup(params));
}

TEST(TestFileOperations, BasicFilePropertiesDoNotNeedLibraryLookup)
{
  const CVariant params =
      ParamsWithProperties({"file", "fileType", "label", "mimeType", "size", "lastModified"});
  EXPECT_FALSE(CFileOperations::NeedsLibraryLookup(params));
}

TEST(TestFileOperations, VideoPropertiesNeedLibraryLookup)
{
  EXPECT_TRUE(CFileOperations::NeedsLibraryLookup(ParamsWithProperties({"thumbnail"})));
  EXPECT_TRUE(CFileOperations::NeedsLibraryLookup(ParamsWithProperties({"cast"})));
  EXPECT_TRUE(CFileOperations::NeedsLibraryLookup(ParamsWithProperties({"file", "streamDetails"})));
}
