/*
 *  Copyright (C) 2024 mod.io Pty Ltd. <https://mod.io>
 *
 *  This file is part of the mod.io UE Plugin.
 *
 *  Distributed under the MIT License. (See accompanying file LICENSE or
 *   view online at <https://github.com/modio/modio-ue/blob/main/LICENSE>)
 *
 */

using UnrealBuildTool;
using System.IO;
using System.Collections.Generic;

public class ModioUIEditor : ModuleRules
{
    public ModioUIEditor(ReadOnlyTargetRules Target) : base(Target)
    {
	    PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
#if UE_5_2_OR_LATER
	    IWYUSupport = IWYUSupport.Full;
#else
	    bEnforceIWYU = true;
#endif

        PublicDependencyModuleNames.AddRange(new string[]
        {
	        "Core",
	        "CoreUObject",
	        "Engine",
	        "UnrealEd",
	        "Modio",
	        "ModioUI",
	        "ModioUICore",
	        "DetailCustomizations",
	        "PropertyEditor",
	        "PropertyPath",
	        "UMGEditor",
	        "MainFrame",
	        "UnrealEd",
	        "UMG",
	        "Blutility",
	        "DesktopPlatform",
	        "AssetTools",
	        "ModioEditor",
			"Settings"

        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
	        "UMG",
	        "Slate",
	        "SlateCore",
	        "UnrealEd",
	        "AssetTools",
	        "AssetRegistry",
	        "AppFramework",
	        "InputCore",
	        "SharedSettingsWidgets"
        });

        PublicIncludePaths.AddRange(new string[] { Path.Combine(ModuleDirectory, "Public") });
        PrivateIncludePaths.AddRange(new string[] { Path.Combine(ModuleDirectory, "Private")
        });

    }
}