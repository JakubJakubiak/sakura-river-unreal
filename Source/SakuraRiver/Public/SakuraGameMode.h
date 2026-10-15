#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "SakuraGameMode.generated.h"

UCLASS()
class SAKURARIVER_API ASakuraGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	ASakuraGameMode();
	virtual void StartPlay() override;
};

/** Pasek stanu (H) i podpowiedź klawiszy przez pierwsze sekundy. W OBS domyślnie nic nie widać. */
UCLASS()
class SAKURARIVER_API ASakuraHUD : public AHUD
{
	GENERATED_BODY()
public:
	virtual void DrawHUD() override;
};
