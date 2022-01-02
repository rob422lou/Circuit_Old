// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FDiscordIntegrationModule : public IModuleInterface
{
private:

	static void* DiscordHandle;

public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:

	static bool Internal_LoadDependency(const FString& Dir, const FString& Name, void*& Handle);
	static void Internal_FreeDependency(void*& Handle);
};
