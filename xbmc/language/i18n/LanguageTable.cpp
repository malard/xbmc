/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "language/i18n/LanguageTable.h"

#include "language/i18n/Iso639_1.h"
#include "language/i18n/Iso639_2.h"
#include "utils/StringUtils.h"

#include <algorithm>
#include <mutex>
#include <shared_mutex>

using namespace KODI::LANGUAGE::I18N;

namespace
{
std::string Key(std::string_view text)
{
  std::string key{StringUtils::ToLower(text)};
  StringUtils::Trim(key);

  // A declaration may spell a code the POSIX way, pt_BR, and a tag spells it pt-BR
  std::ranges::replace(key, '_', '-');
  return key;
}
} // namespace

CLanguageTable::CLanguageTable()
{
  Seed();
}

CLanguageTable& CLanguageTable::GetInstance()
{
  static CLanguageTable table;
  return table;
}

void CLanguageTable::Seed()
{
  CIso639_1::ListLanguages(m_names);
  CIso639_2::ListLanguages(m_names);

  // ISO 639-1 is enumerated first so that a language having codes in both standards is named by
  // its alpha-2 one, which is what the rest of the application prefers
  std::map<std::string, std::string> names;
  CIso639_1::ListLanguageNames(names);
  CIso639_2::ListLanguageNames(names);

  for (const auto& [name, code] : names)
    m_codes.emplace(Key(name), code);
}

void CLanguageTable::Declare(const std::map<std::string, std::string>& languages)
{
  std::unique_lock lock(m_section);

  for (const auto& [code, name] : languages)
  {
    const std::string key{Key(code)};
    const std::string nameKey{Key(name)};
    if (key.empty() || nameKey.empty())
      continue;

    m_declared[key] = name;
    m_names[key] = name;
    m_codes[nameKey] = key;
  }
}

void CLanguageTable::DeclareNames(const std::map<std::string, std::string>& languages)
{
  std::unique_lock lock(m_section);

  for (const auto& [code, name] : languages)
  {
    const std::string key{Key(code)};
    const std::string nameKey{Key(name)};
    if (key.empty() || nameKey.empty())
      continue;

    // try_emplace, not assignment: whatever already names this language outranks an addon
    m_names.try_emplace(key, name);
    m_codes.try_emplace(nameKey, key);
  }
}

void CLanguageTable::Reset()
{
  std::unique_lock lock(m_section);

  m_names.clear();
  m_codes.clear();
  m_declared.clear();

  Seed();
}

std::optional<std::string> CLanguageTable::NameOf(std::string_view code) const
{
  std::shared_lock lock(m_section);
  if (const auto it = m_names.find(Key(code)); it != m_names.end())
    return it->second;

  return std::nullopt;
}

std::optional<std::string> CLanguageTable::CodeOf(std::string_view name) const
{
  std::shared_lock lock(m_section);
  if (const auto it = m_codes.find(Key(name)); it != m_codes.end())
    return it->second;

  return std::nullopt;
}

void CLanguageTable::List(std::map<std::string, std::string>& languages) const
{
  CIso639_1::ListLanguages(languages);

  std::shared_lock lock(m_section);
  for (const auto& [code, name] : m_declared)
    languages.insert_or_assign(code, name);
}
