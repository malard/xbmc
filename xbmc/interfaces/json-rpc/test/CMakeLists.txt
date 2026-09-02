set(SOURCES TestApplicationLogLevel.cpp
            TestDeprecatedMethodSchema.cpp
            TestFileItemHandler.cpp
            TestJSONRPCPermission.cpp
            TestJSONRPCStatus.cpp
            TestJSONServiceDescription.cpp
            TestJSONServiceDescriptionIntrospect.cpp
            TestPlayerIds.cpp
            TestPlayerOpenDirectory.cpp
            TestPVRBroadcastRange.cpp
            TestPVRBroadcastSchema.cpp
            TestPVREpgFields.cpp
            TestPVRProviderSchema.cpp
            TestPVRRecordingSchema.cpp
            TestSetConfiguration.cpp
            TestSettingsLevel.cpp
            TestSettingsValueGate.cpp
            TestSharedFieldDeclarations.cpp
            TestVideoLibraryPlaybackState.cpp
            TestVideoLibraryRefreshSchema.cpp
            TestVideoLibrarySetSourceContent.cpp
            TestVideoStreamsSchema.cpp)

core_add_test_library(jsonrpc_test)
