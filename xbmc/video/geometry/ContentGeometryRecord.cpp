/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "ContentGeometryRecord.h"

#include "filesystem/File.h"
#include "utils/Archive.h"
#include "utils/StringUtils.h"
#include "utils/Variant.h"
#include "utils/XMLUtils.h"
#include "video/geometry/GeometryTransforms.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <limits>

#include <sys/stat.h>

namespace KODI::VIDEO::GEOMETRY
{

FileIdentity GetFileIdentity(const std::string& path)
{
  struct __stat64 st = {};
  if (XFILE::CFile::Stat(path, &st) != 0)
    return {};

  // Some filesystems report no modification time but a usable creation time.
  int64_t time{st.st_mtime};
  if (time == 0)
    time = st.st_ctime;

  if (time == 0 && st.st_size == 0)
    return {};

  return {static_cast<int64_t>(st.st_size), time};
}

ContentGeometryState StateOf(const ContentGeometryRecord& record)
{
  return record.algorithmVersion < CONTENT_GEOMETRY_ALGORITHM_VERSION ? ContentGeometryState::STALE
                                                                      : ContentGeometryState::VALID;
}

float WidestAspect(const ContentGeometryRecord& record)
{
  if (!record.hasReading || record.coded.IsEmpty())
    return 0.0f;

  StreamGeometry measured;
  measured.coded = record.coded;
  measured.displayAspect = record.displayAspect;

  float widest{0.0f};
  const auto consider = [&](const CRectInt& shape)
  {
    if (!shape.IsEmpty())
      widest = std::max(widest, AspectOf(ToSquarePixels(shape, measured)));
  };

  // An older or NFO-imported record carries only the rectangle and extent.
  consider(record.rect);
  consider(record.envelope);
  for (const CRectInt& section : record.sections)
    consider(section);

  return widest;
}

bool NeedsContentGeometry(const ContentGeometryAttempt& attempt, const FileIdentity& identity)
{
  if (!attempt.exists)
    return true;

  if (attempt.algorithmVersion < CONTENT_GEOMETRY_ALGORITHM_VERSION)
    return true;

  return !attempt.identity.Matches(identity);
}

std::string EncodeGeometrySections(const std::vector<CRectInt>& sections)
{
  std::string packed;
  for (const CRectInt& section : sections)
  {
    if (!packed.empty())
      packed += ';';

    packed += StringUtils::Format("{},{},{},{}", section.x1, section.y1, section.Width(),
                                  section.Height());
  }

  return packed;
}

std::vector<CRectInt> DecodeGeometrySections(const std::string& packed)
{
  std::vector<CRectInt> sections;

  const char* const end{packed.data() + packed.size()};
  const char* at{packed.data()};
  while (at < end)
  {
    std::array<int, 4> values{};
    size_t read{0};
    for (; read < values.size(); ++read)
    {
      const std::from_chars_result parsed{std::from_chars(at, end, values[read])};
      if (parsed.ec != std::errc{})
        break;

      at = parsed.ptr;
      if (at < end && (*at == ',' || *at == ';'))
        ++at;
    }

    if (read < values.size())
      break;

    const int64_t x2{static_cast<int64_t>(values[0]) + values[2]};
    const int64_t y2{static_cast<int64_t>(values[1]) + values[3]};
    if (x2 < std::numeric_limits<int>::min() || x2 > std::numeric_limits<int>::max() ||
        y2 < std::numeric_limits<int>::min() || y2 > std::numeric_limits<int>::max())
      break;

    sections.push_back(CRectInt{values[0], values[1], static_cast<int>(x2), static_cast<int>(y2)});
  }

  return sections;
}

namespace
{

void ArchiveRect(CArchive& ar, CRectInt& rect)
{
  if (ar.IsStoring())
  {
    ar << rect.x1;
    ar << rect.y1;
    ar << rect.x2;
    ar << rect.y2;
  }
  else
  {
    ar >> rect.x1;
    ar >> rect.y1;
    ar >> rect.x2;
    ar >> rect.y2;
  }
}

//! \brief Write \p rect as <prefix>x/y/width/height elements under \p node.
void SaveRectXML(TiXmlElement& node, const std::string& prefix, const CRectInt& rect)
{
  XMLUtils::SetInt(&node, (prefix + "x").c_str(), rect.x1);
  XMLUtils::SetInt(&node, (prefix + "y").c_str(), rect.y1);
  XMLUtils::SetInt(&node, (prefix + "width").c_str(), rect.Width());
  XMLUtils::SetInt(&node, (prefix + "height").c_str(), rect.Height());
}

//! \brief Read SaveRectXML() back, each missing element answered from \p fallback.
CRectInt LoadRectXML(const TiXmlElement& node, const std::string& prefix, const CRectInt& fallback)
{
  int x{fallback.x1};
  int y{fallback.y1};
  int width{fallback.Width()};
  int height{fallback.Height()};
  XMLUtils::GetInt(&node, (prefix + "x").c_str(), x);
  XMLUtils::GetInt(&node, (prefix + "y").c_str(), y);
  XMLUtils::GetInt(&node, (prefix + "width").c_str(), width);
  XMLUtils::GetInt(&node, (prefix + "height").c_str(), height);
  return OriginSizeRect(x, y, width, height);
}

} // unnamed namespace

void Archive(CArchive& ar, ContentGeometryRecord& record)
{
  const auto io = [&ar](auto& field)
  {
    if (ar.IsStoring())
      ar << field;
    else
      ar >> field;
  };

  ArchiveRect(ar, record.coded);
  ArchiveRect(ar, record.rect);
  ArchiveRect(ar, record.envelope);

  if (ar.IsStoring())
  {
    ar << static_cast<int>(record.sections.size());
  }
  else
  {
    int sections{0};
    ar >> sections;
    record.sections.assign(sections, {});
  }
  for (CRectInt& section : record.sections)
    ArchiveRect(ar, section);

  io(record.displayAspect);
  io(record.varies);
  io(record.hasReading);
  io(record.algorithmVersion);
  io(record.identity.size);
  io(record.identity.time);
  io(record.computed);
}

void SaveContentGeometryXML(TiXmlNode& movie, const ContentGeometryRecord& record)
{
  TiXmlElement geometry("contentgeometry");
  XMLUtils::SetInt(&geometry, "codedwidth", record.coded.Width());
  XMLUtils::SetInt(&geometry, "codedheight", record.coded.Height());
  SaveRectXML(geometry, "", record.rect);
  SaveRectXML(geometry, "envelope", record.envelope);
  XMLUtils::SetFloat(&geometry, "displayaspect", record.displayAspect);
  XMLUtils::SetBoolean(&geometry, "varies", record.varies);
  XMLUtils::SetBoolean(&geometry, "hasreading", record.hasReading);
  XMLUtils::SetInt(&geometry, "algorithmversion", record.algorithmVersion);
  XMLUtils::SetString(&geometry, "filesize", std::to_string(record.identity.size));
  XMLUtils::SetString(&geometry, "filemtime", std::to_string(record.identity.time));
  XMLUtils::SetString(&geometry, "computed", record.computed.GetAsDBDateTime());
  if (!record.sections.empty())
    XMLUtils::SetString(&geometry, "sections", EncodeGeometrySections(record.sections));

  movie.InsertEndChild(geometry);
}

std::optional<ContentGeometryRecord> LoadContentGeometryXML(const TiXmlElement& movie)
{
  const TiXmlElement* geometry{movie.FirstChildElement("contentgeometry")};
  if (!geometry)
    return std::nullopt;

  ContentGeometryRecord record;

  int codedWidth{0};
  int codedHeight{0};
  XMLUtils::GetInt(geometry, "codedwidth", codedWidth);
  XMLUtils::GetInt(geometry, "codedheight", codedHeight);
  record.coded = CRectInt{0, 0, codedWidth, codedHeight};
  record.rect = LoadRectXML(*geometry, "", {});

  // An NFO written before the envelope existed describes nothing wider than the rectangle, so
  // the rectangle is the envelope.
  record.envelope = LoadRectXML(*geometry, "envelope", record.rect);

  XMLUtils::GetFloat(geometry, "displayaspect", record.displayAspect);
  XMLUtils::GetBoolean(geometry, "varies", record.varies);
  XMLUtils::GetBoolean(geometry, "hasreading", record.hasReading);
  XMLUtils::GetInt(geometry, "algorithmversion", record.algorithmVersion);

  if (std::string value; XMLUtils::GetString(geometry, "filesize", value))
    record.identity.size = std::strtoll(value.c_str(), nullptr, 10);
  if (std::string value; XMLUtils::GetString(geometry, "filemtime", value))
    record.identity.time = std::strtoll(value.c_str(), nullptr, 10);
  if (std::string value; XMLUtils::GetString(geometry, "computed", value))
    record.computed.SetFromDBDateTime(value);

  // An NFO written since the shapes travelled replaces them; an older one leaves them alone.
  if (std::string value; XMLUtils::GetString(geometry, "sections", value))
    record.sections = DecodeGeometrySections(value);

  if (!record.IsValid())
    return std::nullopt;

  return record;
}

} // namespace KODI::VIDEO::GEOMETRY
