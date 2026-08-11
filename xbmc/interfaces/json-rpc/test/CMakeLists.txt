set(SOURCES TestAddonsExecuteAddon.cpp
            TestDeprecatedMethodSchema.cpp
            TestFileItemHandler.cpp
            TestFileOperations.cpp
            TestJSONRPCPermission.cpp
            TestJSONRPCStatus.cpp
            TestJSONServiceDescription.cpp
            TestJSONServiceDescriptionIntrospect.cpp
            TestPVRBroadcastSchema.cpp
            TestPVREpgFields.cpp
            TestPVRProviderSchema.cpp
            TestSettingsLevel.cpp
            TestSharedFieldDeclarations.cpp
            TestVideoLibraryPlaybackState.cpp
            TestVideoLibraryRefreshSchema.cpp
            TestVideoLibrarySetSourceContent.cpp)

core_add_test_library(jsonrpc_test)
