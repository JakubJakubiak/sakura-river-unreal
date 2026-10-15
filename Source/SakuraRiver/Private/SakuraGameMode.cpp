#include "SakuraGameMode.h"
#include "SakuraDirector.h"
#include "SakuraWorld.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "EngineUtils.h"

ASakuraGameMode::ASakuraGameMode()
{
	DefaultPawnClass = nullptr;
	SpectatorClass = nullptr;
	HUDClass = ASakuraHUD::StaticClass();
}

void ASakuraGameMode::StartPlay()
{
	UWorld* W = GetWorld();
	bool bHasWorld = false, bHasDirector = false;
	for (TActorIterator<ASakuraWorld> It(W); It; ++It) bHasWorld = true;
	for (TActorIterator<ASakuraDirector> It(W); It; ++It) bHasDirector = true;
	if (!bHasWorld) W->SpawnActor<ASakuraWorld>();
	if (!bHasDirector) W->SpawnActor<ASakuraDirector>();
	Super::StartPlay();
}

void ASakuraHUD::DrawHUD()
{
	Super::DrawHUD();
	ASakuraDirector* D = nullptr;
	if (TActorIterator<ASakuraDirector> It(GetWorld()); It) D = *It;
	if (!D || !Canvas) return;
	UFont* Font = GEngine->GetMediumFont();
	if (D->bShowHud)
	{
		const FString S = D->StatusLine();
		const FString H = D->HelpText();
		float W1, H1, W2, H2;
		Canvas->StrLen(Font, S, W1, H1);
		Canvas->StrLen(Font, H, W2, H2);
		const float Wd = FMath::Max(W1, W2) + 24.f;
		Canvas->K2_DrawBox(FVector2D(10, 10), FVector2D(Wd, H1 + H2 + 22), 0.f, FLinearColor(0, 0, 0, 0.0f));
		FCanvasTileItem Bg(FVector2D(10, 10), FVector2D(Wd, H1 + H2 + 22), FLinearColor(0, 0, 0, 0.45f));
		Bg.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Bg);
		Canvas->SetDrawColor(FColor(255, 236, 240));
		Canvas->DrawText(Font, S, 22, 16);
		Canvas->SetDrawColor(FColor(220, 220, 230));
		Canvas->DrawText(Font, H, 22, 18 + H1);
	}
	else if (D->HintTime > 0.f)
	{
		const float A = FMath::Clamp(D->HintTime / 1.5f, 0.f, 1.f);
		Canvas->SetDrawColor(FColor(255, 255, 255, uint8(200 * A)));
		Canvas->DrawText(Font, TEXT("H — pomoc i stan"), 18, Canvas->ClipY - 34);
	}
}
