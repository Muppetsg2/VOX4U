// vox4c - single header MagicaVoxel (.vox) loader, MIT License (see the end of vox4c.h).
// https://github.com/Muppetsg2/vox4c

using UnrealBuildTool;

public class vox4c : ModuleRules
{
	public vox4c(ReadOnlyTargetRules Target) : base(Target)
	{
		// Header only: nothing is compiled here. The implementation is compiled once by the module
		// that uses it (VOX4UEditor/Private/Vox4cImplementation.cpp).
		Type = ModuleType.External;

		// A system include path: warnings inside the header are not reported for the code using it.
		PublicSystemIncludePaths.Add(ModuleDirectory);
	}
}
