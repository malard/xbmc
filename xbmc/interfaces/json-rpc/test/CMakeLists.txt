set(SOURCES TestAddonsExecuteAddon.cpp
            TestApplicationLogLevel.cpp
            TestDeprecatedMethodSchema.cpp
            TestFileItemHandler.cpp
            TestFileOperations.cpp
            TestJSONRPCPermission.cpp
            TestJSONRPCStatus.cpp
            TestJSONServiceDescription.cpp
            TestJSONServiceDescriptionIntrospect.cpp
            TestPlayerOpenDirectory.cpp
            TestPVRBroadcastRange.cpp
            TestPVRBroadcastSchema.cpp
            TestPVREpgFields.cpp
            TestPVRProviderSchema.cpp
            TestPVRRecordingSchema.cpp
            TestSettingsLevel.cpp
            TestSettingsValueGate.cpp
            TestSharedFieldDeclarations.cpp
            TestVideoLibraryPlaybackState.cpp
            TestVideoLibraryRefreshSchema.cpp
            TestVideoLibrarySetSourceContent.cpp)

core_add_test_library(jsonrpc_test)
