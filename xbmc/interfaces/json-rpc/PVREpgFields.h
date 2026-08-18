/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <set>
#include <string>

class CVariant;

namespace JSONRPC
{

/*! \brief Translate an EPG tag's cast string into a Video.Cast array.

 An EPG tag carries its cast as one separator-joined string of names, while the generic list
 types declare the field as Video.Cast, whose entries require a role and an order as well as a
 name. The EPG format carries neither, so the role is empty and the order is the position in
 the string.

 \param cast the cast as CPVREpgInfoTag::Serialize writes it
 \return a Video.Cast array, empty when there are no names
 */
CVariant TranslateEpgCast(const std::string& cast);

/*! \brief The fields a broadcast nested inside a channel answers with.

 One field of the parent names the whole sub-object, leaving its caller no way to select
 within it, so it answers with everything PVR.Fields.Broadcast declares. Read from the
 service description rather than restated, so a field added there reaches the nested copy.

 \return the field names, empty when the service description has not been parsed
 */
std::set<std::string> BroadcastFields();

} // namespace JSONRPC
