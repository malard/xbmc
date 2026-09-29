/*
 *  Copyright (C) 2012-2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "IClient.h"
#include "ITransportLayer.h"
#include "utils/Artwork.h"

#include <array>
#include <cstddef>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <string>

class CFileItem;
class CVariant;
class CVideoInfoTag;

namespace JSONRPC
{
/*!
 \ingroup jsonrpc
 \brief Possible status codes of a response
 to a JSON-RPC request
 */
enum JSONRPC_STATUS
{
  OK = 0,
  ACK = -1,
  InvalidRequest = -32600,
  MethodNotFound = -32601,
  InvalidParams = -32602,
  InternalError = -32603,
  ParseError = -32700,
  //-32100..-32000 Reserved for implementation-defined server-errors.
  BadPermission = -32099,
  NotFound = -32098,
  Unavailable = -32097,
  AccessDenied = -32096,
  FailedToExecute = -32100
};

/*!
 \ingroup jsonrpc
 \brief Describes a JSONRPC_STATUS that is reported to a client as an error

 The message is what CJSONRPC::BuildResponse puts in "error.message"; the description is
 served through JSONRPC.Introspect so that a client can discover how a call may fail
 without reading this header.
 */
struct JsonRpcStatusDescription
{
  JSONRPC_STATUS status;
  const char* name;
  const char* message;
  const char* description;
  //! Whether responses with this status populate "error.data" even without a reason
  bool hasData;
};

/*!
 \ingroup jsonrpc
 \brief The error taxonomy of the JSON-RPC API

 Every JSONRPC_STATUS that reaches a client as an error appears here exactly once. OK and
 ACK are absent because they produce a result rather than an error.
 */
inline constexpr std::array<JsonRpcStatusDescription, 10> JSONRPC_STATUS_DESCRIPTIONS{{
    {ParseError, "ParseError", "Parse error.", "The request could not be parsed as JSON.", false},
    {InvalidRequest, "InvalidRequest", "Invalid request.",
     "The request parsed as JSON but is not a well-formed JSON-RPC 2.0 request object.", false},
    {MethodNotFound, "MethodNotFound", "Method not found.",
     "The requested method does not exist, the client lacks the permission to see it, or it is "
     "not available over the transport the request arrived on.",
     false},
    {InvalidParams, "InvalidParams", "Invalid params.",
     "The given parameters do not validate against the schema of the method. The \"data\" member "
     "names the offending parameter and the constraint it failed.",
     true},
    {InternalError, "InternalError", "Internal error.",
     "The method failed for a reason that no other status describes.", false},
    {FailedToExecute, "FailedToExecute", "Failed to execute method.",
     "The method was called correctly but the operation it requested did not succeed. Its code "
     "sits one below the -32099..-32000 range JSON-RPC 2.0 reserves for server errors.",
     false},
    {BadPermission, "BadPermission", "Bad client permission.",
     "The client does not hold every permission the method requires.", false},
    {NotFound, "NotFound", "Not found.", "The requested item does not exist.", false},
    {Unavailable, "Unavailable", "Requested item is unavailable.",
     "The requested item exists but cannot be provided at the moment.", false},
    {AccessDenied, "AccessDenied", "Access denied.",
     "What was asked for is locked on this installation: a path outside every source shared for "
     "remote access, or a setting level the profile's settings lock keeps.",
     false},
}};

/*!
 \brief Returns the description of the given JSONRPC_STATUS
 \param status Specific JSONRPC_STATUS
 \return Description of the given status, or nullptr if it is not reported as an error
 */
inline const JsonRpcStatusDescription* StatusToDescription(JSONRPC_STATUS status)
{
  for (const auto& description : JSONRPC_STATUS_DESCRIPTIONS)
  {
    if (description.status == status)
      return &description;
  }

  return nullptr;
}

/*!
 \ingroup jsonrpc
 \brief Why a call failed, reported to the client as "error.data.reason"

 A reason refines the status a call fails with, and may refine more than one. A method declares
 the reasons it can fail for under the errors they come with in methods.json.
 */
enum class Reason
{
  NothingPlaying,
  NotApplicable,
  NotSeekable,
  NotPausable,
  TempoUnsupported,
  Paused,
  NoSuchStream,
  Unreachable,
  NoSuchItem,
  NoSuchSource,
  NotInLibrary,
  NoSuchAddon,
  NoSuchPath,
  OutsideSources,
  NotAFile,
  NoSuchSetting,
  SettingDisabled,
  ChangeDeclined,
  LevelLocked,
};

struct JsonRpcReasonDescription
{
  Reason reason;
  //! The stable name a client matches on
  const char* name;
  const char* description;
};

//! Every Reason, in declaration order
inline constexpr JsonRpcReasonDescription JSONRPC_REASON_DESCRIPTIONS[] = {
    {Reason::NothingPlaying, "nothing-playing",
     "Nothing is playing, or not the playlist the call named."},
    {Reason::NotApplicable, "not-applicable",
     "The call does not apply to what it acts on, such as zooming a video, choosing a subtitle "
     "for music or repeating the picture playlist."},
    {Reason::NotSeekable, "not-seekable", "What is playing cannot seek."},
    {Reason::NotPausable, "not-pausable", "What is playing cannot pause."},
    {Reason::TempoUnsupported, "tempo-unsupported",
     "The player of what is playing cannot change its tempo."},
    {Reason::Paused, "paused", "The call needs playback that is not paused."},
    {Reason::NoSuchStream, "no-such-stream", "What is playing has no stream at the given index."},
    {Reason::Unreachable, "unreachable",
     "The path cannot be read at the moment, as when its share is offline."},
    {Reason::NoSuchItem, "no-such-item",
     "Nothing has the given id: no library item, and no PVR channel, channel group, broadcast, "
     "timer or recording."},
    {Reason::NoSuchSource, "no-such-source", "The directory lies inside no source of the library."},
    {Reason::NotInLibrary, "not-in-library", "The library holds nothing under the directory."},
    {Reason::NoSuchAddon, "no-such-addon",
     "No add-on has the given id, or none that is enabled where the call needs one."},
    {Reason::NoSuchPath, "no-such-path", "Nothing exists at the given path."},
    {Reason::OutsideSources, "outside-sources",
     "The path lies outside every source shared for remote access."},
    {Reason::NotAFile, "not-a-file", "The path names a directory, not a file."},
    {Reason::NoSuchSetting, "no-such-setting", "No setting has the given id."},
    {Reason::SettingDisabled, "setting-disabled",
     "The setting is disabled by the settings it depends on, so it cannot change now."},
    {Reason::ChangeDeclined, "change-declined",
     "Kodi declined the value, as when a new display mode is not kept."},
    {Reason::LevelLocked, "level-locked", "The profile's settings lock keeps the setting level."},
};

constexpr bool ReasonsAreDescribedInOrder()
{
  for (size_t index = 0; index < std::size(JSONRPC_REASON_DESCRIPTIONS); ++index)
  {
    if (JSONRPC_REASON_DESCRIPTIONS[index].reason != static_cast<Reason>(index))
      return false;
  }
  return true;
}
static_assert(ReasonsAreDescribedInOrder());

inline const JsonRpcReasonDescription& ReasonToDescription(Reason reason)
{
  return JSONRPC_REASON_DESCRIPTIONS[static_cast<size_t>(reason)];
}

/*!
 \brief Fails a call for a declared reason, which the response carries in "error.data"
 \param result The handler's result, replaced by the error data
 \param status The status the call fails with
 \param reason Why the call failed
 \param target What the failure concerns, as the caller addresses it, e.g. {"movieId": 3}
 \return status
 */
JSONRPC_STATUS Fail(CVariant& result, JSONRPC_STATUS status, Reason reason);
JSONRPC_STATUS Fail(CVariant& result, JSONRPC_STATUS status, Reason reason, const CVariant& target);

//! A failure's target of one member, e.g. Target("playlist", "audio")
CVariant Target(const std::string& key, const CVariant& value);

/*!
 \brief The handler of a JSON-RPC method

 A handler takes the validated parameters and fills in the result. The few that answer
 differently depending on who is asking also take the transport and the client.
 */
class MethodCall
{
public:
  using Handler = JSONRPC_STATUS (*)(const CVariant& parameterObject, CVariant& result);
  using CallerHandler = JSONRPC_STATUS (*)(ITransportLayer* transport,
                                           IClient* client,
                                           const CVariant& parameterObject,
                                           CVariant& result);

  constexpr MethodCall() = default;
  constexpr MethodCall(Handler handler) : m_handler(handler) {}
  constexpr MethodCall(CallerHandler handler) : m_callerHandler(handler) {}

  explicit operator bool() const { return m_handler != nullptr || m_callerHandler != nullptr; }

  JSONRPC_STATUS operator()(ITransportLayer* transport,
                            IClient* client,
                            const CVariant& parameterObject,
                            CVariant& result) const
  {
    if (m_handler)
      return m_handler(parameterObject, result);
    return m_callerHandler(transport, client, parameterObject, result);
  }

private:
  Handler m_handler = nullptr;
  CallerHandler m_callerHandler = nullptr;
};

/*!
 \ingroup jsonrpc
 \brief Permission categories for json rpc methods

 A JSON-RPC method will only be called if the caller
 has the correct permissions to execute the method.
 The method call needs to be perfectly threadsafe.
 */
enum OperationPermission
{
  ReadData = 0x1,
  ControlPlayback = 0x2,
  ControlNotify = 0x4,
  ControlPower = 0x8,
  UpdateData = 0x10,
  RemoveData = 0x20,
  Navigate = 0x40,
  WriteFile = 0x80,
  ControlSystem = 0x100,
  ControlGUI = 0x200,
  ManageAddon = 0x400,
  ExecuteAddon = 0x800,
  ControlPVR = 0x1000,
  WriteSetting = 0x2000
};

const int OPERATION_PERMISSION_ALL =
    (ReadData | ControlPlayback | ControlNotify | ControlPower | UpdateData | RemoveData |
     Navigate | WriteFile | ControlSystem | ControlGUI | ManageAddon | ExecuteAddon | ControlPVR |
     WriteSetting);

const int OPERATION_PERMISSION_NOTIFICATION =
    (ControlPlayback | ControlNotify | ControlPower | UpdateData | RemoveData | Navigate |
     WriteFile | ControlSystem | ControlGUI | ManageAddon | ExecuteAddon | ControlPVR |
     WriteSetting);

/*!
 \brief Returns a string representation for the
 given OperationPermission
 \param permission Specific OperationPermission
 \return String representation of the given OperationPermission
 */
inline const char* PermissionToString(const OperationPermission& permission)
{
  switch (permission)
  {
    case ReadData:
      return "ReadData";
    case ControlPlayback:
      return "ControlPlayback";
    case ControlNotify:
      return "ControlNotify";
    case ControlPower:
      return "ControlPower";
    case UpdateData:
      return "UpdateData";
    case RemoveData:
      return "RemoveData";
    case Navigate:
      return "Navigate";
    case WriteFile:
      return "WriteFile";
    case ControlSystem:
      return "ControlSystem";
    case ControlGUI:
      return "ControlGUI";
    case ManageAddon:
      return "ManageAddon";
    case ExecuteAddon:
      return "ExecuteAddon";
    case ControlPVR:
      return "ControlPVR";
    case WriteSetting:
      return "WriteSetting";
    default:
      return "Unknown";
  }
}

/*!
    \brief Returns the OperationPermission value for the given
    string representation
    \param permission String representation of the OperationPermission
    \return OperationPermission value of the given string representation, or
    nothing for a string that is not the name of a permission
    */
inline std::optional<OperationPermission> StringToPermission(const std::string& permission)
{
  if (permission.compare("ReadData") == 0)
    return ReadData;
  if (permission.compare("ControlPlayback") == 0)
    return ControlPlayback;
  if (permission.compare("ControlNotify") == 0)
    return ControlNotify;
  if (permission.compare("ControlPower") == 0)
    return ControlPower;
  if (permission.compare("UpdateData") == 0)
    return UpdateData;
  if (permission.compare("RemoveData") == 0)
    return RemoveData;
  if (permission.compare("Navigate") == 0)
    return Navigate;
  if (permission.compare("WriteFile") == 0)
    return WriteFile;
  if (permission.compare("ControlSystem") == 0)
    return ControlSystem;
  if (permission.compare("ControlGUI") == 0)
    return ControlGUI;
  if (permission.compare("ManageAddon") == 0)
    return ManageAddon;
  if (permission.compare("ExecuteAddon") == 0)
    return ExecuteAddon;
  if (permission.compare("ControlPVR") == 0)
    return ControlPVR;
  if (permission.compare("WriteSetting") == 0)
    return WriteSetting;

  return std::nullopt;
}

class CJSONRPCUtils
{
public:
  static void NotifyItemUpdated();
  static void NotifyItemUpdated(const std::shared_ptr<CFileItem>& item);
  static void NotifyItemUpdated(const CVideoInfoTag& info, const KODI::ART::Artwork& artwork);
};
} // namespace JSONRPC
