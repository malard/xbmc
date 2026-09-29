/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "SampledGeometry.h"

#include "XBDateTime.h"

namespace KODI::VIDEO::GEOMETRY
{

ContentGeometryRecord MakeContentGeometryRecord(const SampledGeometry& scan,
                                                const FileIdentity& identity,
                                                const CDateTime& computed)
{
  ContentGeometryRecord record;
  record.identity = identity;
  record.computed = computed;

  if (!scan.succeeded)
  {
    record.outcome = ContentGeometryOutcome::Failed;
    return record;
  }

  record.outcome = ContentGeometryOutcome::Measured;
  record.coded = scan.coded;
  record.rect = scan.combined.rect;

  // The combiner leaves the envelope empty only when it had no cluster to take an extent of.
  record.envelope = scan.combined.envelope.IsEmpty() ? scan.combined.rect : scan.combined.envelope;

  record.displayAspect = scan.displayAspect;
  record.varies = scan.combined.varies;
  record.hasReading = scan.combined.hasReading;

  // The clusters' rectangles alone, without the rest of what produced them.
  record.sections.reserve(scan.combined.clusters.size());
  for (const GeometryCluster& cluster : scan.combined.clusters)
    record.sections.push_back(cluster.rect);

  return record;
}

} // namespace KODI::VIDEO::GEOMETRY
