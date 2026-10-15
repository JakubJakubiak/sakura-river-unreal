#include "Modules/ModuleManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

class FSakuraShadersModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		const FString Dir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Shaders"));
		if (!AllShaderSourceDirectoryMappings().Contains(TEXT("/Project")))
		{
			AddShaderSourceDirectoryMapping(TEXT("/Project"), Dir);
		}
	}
};

IMPLEMENT_MODULE(FSakuraShadersModule, SakuraShaders);
