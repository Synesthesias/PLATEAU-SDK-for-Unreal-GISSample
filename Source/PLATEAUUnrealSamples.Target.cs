// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.Collections.Generic;

public class PLATEAUUnrealSamplesTarget : TargetRules
{
	public PLATEAUUnrealSamplesTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V4;

		ExtraModuleNames.AddRange( new string[] { "PLATEAUUnrealSamples" } );
	}
}
