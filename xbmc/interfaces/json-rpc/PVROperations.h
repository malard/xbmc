/*
 *  Copyright (C) 2012-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "FileItemHandler.h"

#include <memory>
#include <vector>

class CDateTime;
class CVariant;

namespace PVR
{
class CPVRChannelGroup;
class CPVREpg;
class CPVREpgInfoTag;
} // namespace PVR

namespace JSONRPC
{
class CPVROperations : public CFileItemHandler
{
public:
  static JSONRPC_STATUS GetProperties(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS GetChannelGroups(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS GetChannelGroupDetails(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS GetChannels(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS GetChannelDetails(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS GetClients(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS GetBroadcasts(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS GetBroadcastsByChannelGroup(const CVariant& parameterObject,
                                                    CVariant& result);
  static JSONRPC_STATUS GetBroadcastDetails(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS GetBroadcastIsPlayable(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS GetPlayableBroadcasts(ITransportLayer* transport,
                                              IClient* client,
                                              const CVariant& parameterObject,
                                              CVariant& result);
  static JSONRPC_STATUS GetTimers(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS GetTimerDetails(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS GetRecordings(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS GetRecordingDetails(const CVariant& parameterObject, CVariant& result);

  static JSONRPC_STATUS AddTimer(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS DeleteTimer(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS ToggleTimer(const CVariant& parameterObject, CVariant& result);

  static JSONRPC_STATUS Record(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS Scan(const CVariant& parameterObject, CVariant& result);

  static std::shared_ptr<CFileItem> GetRecordingFileItem(int recordingId);

protected:
  /*!
     \brief Read the starttime/endtime pair of a request

     Both absent leaves start and end invalid (no range) when not required; otherwise both
     must parse and end must not precede start.
     */
  static JSONRPC_STATUS ParseTimeRange(const CVariant& parameterObject,
                                       bool required,
                                       CDateTime& start,
                                       CDateTime& end);

  /*!
     \brief The broadcasts of an EPG overlapping [start, end), or all of them for an invalid range
     */
  static std::vector<std::shared_ptr<PVR::CPVREpgInfoTag>> GetBroadcastsInRange(
      const PVR::CPVREpg& epg, const CDateTime& start, const CDateTime& end);

private:
  static JSONRPC_STATUS GetPropertyValue(const std::string& property, CVariant& result);
  static void FillChannelGroupDetails(
      const std::shared_ptr<const PVR::CPVRChannelGroup>& channelGroup,
      const CVariant& parameterObject,
      CVariant& result,
      bool append = false);
};
} // namespace JSONRPC
