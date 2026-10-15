#include "Modules/ModuleManager.h"
#include "Materials/Material.h"

// UE 5.8.3, macOS Shipping: przy starcie domyślne materiały silnika ładują się częściowo (ochrona przed zakleszczeniem),
// a scena Nanite powstaje wcześniej niż w Development i sięga po UMaterial::GetDefaultMaterial — w Shipping kończyło się to awarią.
// Ładujemy domyślne materiały, zanim silnik stworzy pierwszy świat.
class FSakuraRiverModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		UMaterialInterface::InitDefaultMaterials();
		UMaterial::GetDefaultMaterial(MD_Surface);
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FSakuraRiverModule, SakuraRiver, "SakuraRiver");
