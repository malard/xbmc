/*
 *  Copyright (C) 2025 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace KODI::LANGUAGE::I18N
{
class CIso639_2
{
public:
  CIso639_2() = delete;

  /*!
   * \brief Provide a list of defined ISO 639-2 languages, including B/T variants
   * \param[in,out] langMap map to add languages to
   */
  static void ListLanguages(std::map<std::string, std::string>& langMap);

  /*!
   * \brief Provide every name an ISO 639-2 language is known by, main and alternative alike,
   *        mapped to its ISO 639-2/T code
   * \param[in,out] nameMap map to add the names to; an entry already present is kept
   */
  static void ListLanguageNames(std::map<std::string, std::string>& nameMap);

  /*!
   * \brief Retrieve the ISO 639-2/T code for an ISO 639-2/B code
   * \param bCode the ISO 639-2/B code
   * \return The matching ISO 639-2/T code, nullopt if there isn't one.
   */
  static std::optional<std::string> BCodeToTCode(std::string_view bCode);

  /*!
   * \brief Retrieve the ISO 639-2/B code for an ISO 639-2/T code
   * \param tCode the ISO 639-2/T code
   * \return The matching ISO 639-2/B code, nullopt if there isn't one.
   */
  static std::optional<std::string> TCodeToBCode(std::string_view tCode);
};
} // namespace KODI::LANGUAGE::I18N
