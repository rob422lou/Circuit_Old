// Fill out your copyright notice in the Description page of Project Settings.

using System.IO;
using UnrealBuildTool;

public class DiscordIntegrationLibrary : ModuleRules
{
	public DiscordIntegrationLibrary(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		Type = ModuleType.External;
	}
}
