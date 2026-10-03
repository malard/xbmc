/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "JSONUtils.h"

#include "ServiceBroker.h"
#include "XBDateTime.h"
#include "addons/AddonManager.h"
#include "addons/Scraper.h"

namespace JSONRPC
{

void CJSONUtils::SetFromDBDate(const CVariant& jsonDate, CDateTime& date)
{
  if (!jsonDate.isString())
    return;

  if (jsonDate.empty())
    date.Reset();
  else
    date.SetFromDBDate(jsonDate.asString());
}

void CJSONUtils::SetFromDBDateTime(const CVariant& jsonDate, CDateTime& date)
{
  if (!jsonDate.isString())
    return;

  if (jsonDate.empty())
    date.Reset();
  else
    date.SetFromDBDateTime(jsonDate.asString());
}

JSONRPC_STATUS CJSONUtils::ResolveScraper(const std::string& scraperId,
                                          ADDON::ContentType content,
                                          const std::string& settings,
                                          std::shared_ptr<ADDON::CScraper>& scraper,
                                          CVariant& result)
{
  // Looked up by type: a scraper serving more than one content type has an instance per
  // type, and the binding is stored with the instance's own content.
  ADDON::AddonPtr addon;
  ADDON::CAddonMgr& addonMgr = CServiceBroker::GetAddonMgr();
  if (!addonMgr.GetAddon(scraperId, addon, ADDON::ScraperTypeFromContent(content),
                         ADDON::OnlyEnabled::CHOICE_YES))
  {
    if (!addonMgr.GetAddon(scraperId, addon, ADDON::OnlyEnabled::CHOICE_YES))
      return Fail(result, NotFound, Reason::NoSuchAddon, Target("scraperId", scraperId));
    return InvalidParams;
  }

  scraper = std::dynamic_pointer_cast<ADDON::CScraper>(addon);
  if (!scraper)
    return InvalidParams;

  // Without supplied XML a failure is the scraper's own defaults, not the caller's doing.
  if (!scraper->SetPathSettings(content, settings) && !settings.empty())
    return InvalidParams;

  return OK;
}

} // namespace JSONRPC
