// Copyright Epic Games, Inc. All Rights Reserved.

#include "DiscordIntegration.h"
#include "Core.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "FDiscordIntegrationModule"

DEFINE_LOG_CATEGORY_STATIC(LogDiscordUE4, All, All)

#define LOG_NORMAL(StringParam)		UE_LOG(LogDiscordUE4, Display, TEXT("%s"), *FString(StringParam))
#define LOG_ERROR(StringParam)		UE_LOG(LogDiscordUE4, Error, TEXT("%s"), *FString(StringParam))

void* FDiscordIntegrationModule::DiscordHandle = nullptr;

void FDiscordIntegrationModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	// Get the base directory of this plugin
	FString BaseDir = IPluginManager::Get().FindPlugin("DiscordIntegration")->GetBaseDir();

	// Add on the relative location of the third party dll and load it
	FString LibraryPath;
#if PLATFORM_WINDOWS
	const FString DiscordDir = FPaths::Combine(*BaseDir, TEXT("Source"), TEXT("ThirdParty"), TEXT("discord-files"), TEXT("Win64"));
#elif PLATFORM_MAC
	const FString DiscordDir = FPaths::Combine(*BaseDir, TEXT("Source"), TEXT("ThirdParty"), TEXT("discord-files"), TEXT("Mac"));
#elif PLATFORM_LINUX
	const FString DiscordDir = FPaths::Combine(*BaseDir, TEXT("Source"), TEXT("ThirdParty"), TEXT("discord-files"), TEXT("Linux"), TEXT("x86_64-unknown-linux-gnu"));
#endif

#if PLATFORM_LINUX
	static const FString DiscordLibName = "libdiscord_game_sdk";
#else
	static const FString DiscordLibName = "discord_game_sdk";
#endif

	const bool bDependencyLoaded = Internal_LoadDependency(DiscordDir, DiscordLibName, DiscordHandle);
	if (!bDependencyLoaded)
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("Name"), FText::FromString(DiscordLibName));
		Arguments.Add(TEXT("Path"), FText::FromString(DiscordDir));
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("LoadDependencyError", "Failed to load {Name} from path {Path}."), Arguments));
		Internal_FreeDependency(DiscordHandle);
	}
}

void FDiscordIntegrationModule::ShutdownModule()
{
	Internal_FreeDependency(DiscordHandle);
	LOG_NORMAL("Shutting down Discord Module.");
}

bool FDiscordIntegrationModule::Internal_LoadDependency(const FString& Dir, const FString& Name, void*& Handle)
{
#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
	FString Lib = Name + TEXT(".") + FPlatformProcess::GetModuleExtension();
	FString Path = Dir.IsEmpty() ? *Lib : FPaths::Combine(*Dir, *Lib);

	Handle = FPlatformProcess::GetDllHandle(*Path);

	if (Handle == nullptr)
	{
		LOG_ERROR(FString::Printf(TEXT("Dependency %s failed to load in directory %s"), *Lib, *Dir));
		return false;
	}

	LOG_NORMAL(FString::Printf(TEXT("Dependency %s successfully loaded from directory %s"), *Lib, *Dir));
	return true;
#else
	return false;
#endif
}

void FDiscordIntegrationModule::Internal_FreeDependency(void*& Handle)
{
#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
	if (Handle != nullptr)
	{
		FPlatformProcess::FreeDllHandle(Handle);
		Handle = nullptr;
	}
#endif
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FDiscordIntegrationModule, DiscordIntegration)
