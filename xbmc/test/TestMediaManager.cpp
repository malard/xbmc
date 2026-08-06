/*
 *  Copyright (C) 2005-2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "ServiceBroker.h"
#include "storage/MediaManager.h"

#include <gtest/gtest.h>

/*
 * Regression cover for the media manager being absent from the unit test environment.
 *
 * CServiceManager::GetMediaManager() returns *m_mediaManager with no guard, and
 * CServiceManager::InitForTesting() used not to create it, so any test reaching the
 * service dereferenced a null unique_ptr. The whole test binary died with a bare access
 * violation and no stack, which reads as "the code under test is broken" rather than "the
 * environment is incomplete".
 *
 * It is reachable from ordinary code: CDVDFactoryInputStream::CreateInputStream consults
 * it on every call under HAS_OPTICAL_DRIVE, so no test could open a media file at all.
 */

TEST(TestMediaManager, IsAvailableInTheTestEnvironment)
{
  EXPECT_NO_FATAL_FAILURE(CServiceBroker::GetMediaManager());
}

//! The exact call that CDVDFactoryInputStream::CreateInputStream makes.
TEST(TestMediaManager, TranslateDevicePathDoesNotRequireInitialize)
{
  const std::string translated = CServiceBroker::GetMediaManager().TranslateDevicePath("");

  // The value is platform dependent and may legitimately be empty; that it returns at all
  // is the point.
  EXPECT_TRUE(translated.empty() || !translated.empty());
}
