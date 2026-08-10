/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

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

} // namespace JSONRPC
