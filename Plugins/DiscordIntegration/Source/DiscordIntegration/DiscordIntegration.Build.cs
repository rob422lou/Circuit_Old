// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class DiscordIntegration : ModuleRules
{
	public DiscordIntegration(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoSharedPCHs;
		CppStandard = CppStandardVersion.Latest;

		PublicIncludePaths.Add(DiscordPath);


		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Projects"
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				// ... add private dependencies that you statically link with here ...	
				 "CoreUObject", "Engine"
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);

		PublicAdditionalLibraries.Add(DiscordLibFile);
		PublicAdditionalLibraries.Add(Path.GetFullPath(Path.Combine(ModuleDirectory, "../ThirdParty/discord-files/Win64/dpp.lib")));

		if (IsWin64())
		{
			PublicDelayLoadDLLs.Add("discord_game_sdk.dll");

			PublicDelayLoadDLLs.Add("dpp.dll");
			PublicDelayLoadDLLs.Add("libcrypto-1_1-x64.dll");
			PublicDelayLoadDLLs.Add("libsodium.dll");
			PublicDelayLoadDLLs.Add("libssl-1_1-x64.dll");
			PublicDelayLoadDLLs.Add("opus.dll");
			PublicDelayLoadDLLs.Add("zlib1.dll");
		}

		RuntimeDependencies.Add(DiscordRuntimeDepFile);
		RuntimeDependencies.Add(Path.GetFullPath(Path.Combine(ModuleDirectory, "../ThirdParty/discord-files/Win64/dpp.dll")));
		RuntimeDependencies.Add(Path.GetFullPath(Path.Combine(ModuleDirectory, "../ThirdParty/discord-files/Win64/libcrypto-1_1-x64.dll")));
		RuntimeDependencies.Add(Path.GetFullPath(Path.Combine(ModuleDirectory, "../ThirdParty/discord-files/Win64/libsodium.dll")));
		RuntimeDependencies.Add(Path.GetFullPath(Path.Combine(ModuleDirectory, "../ThirdParty/discord-files/Win64/libssl-1_1-x64.dll")));
		RuntimeDependencies.Add(Path.GetFullPath(Path.Combine(ModuleDirectory, "../ThirdParty/discord-files/Win64/opus.dll")));
		RuntimeDependencies.Add(Path.GetFullPath(Path.Combine(ModuleDirectory, "../ThirdParty/discord-files/Win64/zlib1.dll")));
	}

	private string DiscordPath
	{
		get { return Path.GetFullPath(Path.Combine(ModuleDirectory, "discord-files")); }
	}

	private string DiscordLibFile
	{
		get
		{
			return Path.GetFullPath(Path.Combine(ModuleDirectory, "../ThirdParty/discord-files/", GetPlatformName, DiscordLibFileName));
		}
	}

	private string DiscordLibFileName
	{
		get
		{
			if (IsWin64())
			{
				return "discord_game_sdk.dll.lib";
			}
			else if (IsMac())
			{
				return "discord_game_sdk.dylib";
			}
			else if (IsLinux())
			{
				return "discord_game_sdk.so";
			}

			return null;
		}
	}

	private string DiscordRuntimeDepFile
	{
		get
		{
			// Only Windows needs separate library and runtime dependency files
			if (IsWin64())
			{
				return Path.GetFullPath(Path.Combine(ModuleDirectory, "../ThirdParty/discord-files/Win64/discord_game_sdk.dll"));
			}
			else
			{
				return DiscordLibFile;
			}
		}
	}

	private bool IsWin64()
	{
		return Target.Platform == UnrealTargetPlatform.Win64;
	}

	private bool IsMac()
	{
		return Target.Platform == UnrealTargetPlatform.Mac;
	}

	private bool IsLinux()
	{
		return Target.Platform == UnrealTargetPlatform.Linux;
	}

	private string GetPlatformName
	{
		get
		{
			if (IsWin64())
			{
				return "Win64";
			}
			else if (IsMac())
			{
				return "Mac";
			}
			else if (IsLinux())
			{
				return "Linux/x86_64-unknown-linux-gnu";
			}

			return null;
		}
	}
}
