/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "SystemOperations.h"

#include "JSONUtils.h"
#include "ServiceBroker.h"
#include "interfaces/builtins/Builtins.h"
#include "messaging/ApplicationMessenger.h"
#include "powermanagement/PowerManager.h"
#include "utils/Variant.h"

using namespace JSONRPC;

JSONRPC_STATUS CSystemOperations::GetProperties(ITransportLayer* transport,
                                                IClient* client,
                                                const CVariant& parameterObject,
                                                CVariant& result)
{
  return GetNamedProperties(
      parameterObject, result, [client](const std::string& property, CVariant& value)
      { return GetPropertyValue(client->GetPermissionFlags(), property, value); });
}

JSONRPC_STATUS CSystemOperations::EjectOpticalDrive(const CVariant& parameterObject,
                                                    CVariant& result)
{
  return CBuiltins::GetInstance().Execute("EjectTray") == 0 ? ACK : FailedToExecute;
}

JSONRPC_STATUS CSystemOperations::Shutdown(const CVariant& parameterObject, CVariant& result)
{
  if (CServiceBroker::GetPowerManager().CanPowerdown())
  {
    CServiceBroker::GetAppMessenger()->PostMsg(TMSG_POWERDOWN);
    return ACK;
  }
  else
    return Fail(result, FailedToExecute, Reason::NotSupported);
}

JSONRPC_STATUS CSystemOperations::Suspend(const CVariant& parameterObject, CVariant& result)
{
  if (CServiceBroker::GetPowerManager().CanSuspend())
  {
    CServiceBroker::GetAppMessenger()->PostMsg(TMSG_SUSPEND);
    return ACK;
  }
  else
    return Fail(result, FailedToExecute, Reason::NotSupported);
}

JSONRPC_STATUS CSystemOperations::Hibernate(const CVariant& parameterObject, CVariant& result)
{
  if (CServiceBroker::GetPowerManager().CanHibernate())
  {
    CServiceBroker::GetAppMessenger()->PostMsg(TMSG_HIBERNATE);
    return ACK;
  }
  else
    return Fail(result, FailedToExecute, Reason::NotSupported);
}

JSONRPC_STATUS CSystemOperations::Reboot(const CVariant& parameterObject, CVariant& result)
{
  if (CServiceBroker::GetPowerManager().CanReboot())
  {
    CServiceBroker::GetAppMessenger()->PostMsg(TMSG_RESTART);
    return ACK;
  }
  else
    return Fail(result, FailedToExecute, Reason::NotSupported);
}

JSONRPC_STATUS CSystemOperations::GetPropertyValue(int permissions,
                                                   const std::string& property,
                                                   CVariant& result)
{
  if (property == "canShutdown")
    result = CServiceBroker::GetPowerManager().CanPowerdown() && (permissions & ControlPower);
  else if (property == "canSuspend")
    result = CServiceBroker::GetPowerManager().CanSuspend() && (permissions & ControlPower);
  else if (property == "canHibernate")
    result = CServiceBroker::GetPowerManager().CanHibernate() && (permissions & ControlPower);
  else if (property == "canReboot")
    result = CServiceBroker::GetPowerManager().CanReboot() && (permissions & ControlPower);
  else
    return InvalidParams;

  return OK;
}
