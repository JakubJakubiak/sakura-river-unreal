// Reżyser Sakura River HD: czat → napisy płynące rzeką i lampiony, komendy (pora dnia, deszcz, wiatr, płatki, fajerwerki),
// kamera i punkty widokowe, presety grafiki, wyjście NDI, automatyczne zrzuty do testów.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SakuraConfig.h"
#include "SakuraTwitch.h"
#include "SakuraDirector.generated.h"

class ASakuraWorld;
class ACameraActor;
class UWidgetComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMediaOutput;
class UMediaCapture;
class STextBlock;

UCLASS()
class SAKURARIVER_API ASakuraDirector : public AActor
{
	GENERATED_BODY()

public:
	ASakuraDirector();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Wiadomość z czatu (Twitch albo inne źródło). */
	void Chat(const FSakuraChatMessage& Msg);
	/** sub | resub | subgift | submysterygift | raid | bits */
	void Event(const FString& Type, const FString& User, int32 Amount);
	/** Komenda efektu: sakura, wind, rain, fireworks, dawn, day, sunset, night, view:<nazwa> */
	void Effect(const FString& Fx);
	void SetTimePreset(const FString& Name, bool bFromChat, bool bSnap = false);
	void SetView(const FString& Name, bool bInstant = false);
	void Fireworks(int32 Count);
	void ApplyGraphics(const FString& Preset);

	FString StatusLine() const;
	FString HelpText() const;
	bool bShowHud = false;
	float HintTime = 6.f;
	const FSakuraConfig& GetConfig() const { return Cfg; }

	UPROPERTY(VisibleAnywhere, Category = "Sakura") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "Sakura") TObjectPtr<UInstancedStaticMeshComponent> Petals;
	UPROPERTY(VisibleAnywhere, Category = "Sakura") TObjectPtr<UInstancedStaticMeshComponent> RainDrops;
	UPROPERTY(VisibleAnywhere, Category = "Sakura") TObjectPtr<UInstancedStaticMeshComponent> Fireflies;
	UPROPERTY(VisibleAnywhere, Category = "Sakura") TObjectPtr<UInstancedStaticMeshComponent> Sparks;
	UPROPERTY(VisibleAnywhere, Category = "Sakura") TArray<TObjectPtr<UPointLightComponent>> BurstLights;

private:
	struct FFloat
	{
		TObjectPtr<UWidgetComponent> Text;
		TObjectPtr<UStaticMeshComponent> Lantern;
		TObjectPtr<UPointLightComponent> Light;
		TObjectPtr<UMaterialInstanceDynamic> LanternMID;
		TSharedPtr<STextBlock> TextBlock, NameBlock;
		bool bActive = false, bHasText = false, bHasLantern = false;
		FString User;
		FLinearColor Color = FLinearColor::White;
		double Lane = 0, Z = 0, Speed = 1, Age = 0, Phase = 0;
		float Flare = 0;
		FVector2D Size = FVector2D(400, 80);
	};
	struct FPending { FString User; FLinearColor Color; FString Text; bool bLantern; };
	struct FSpark { FVector P = FVector::ZeroVector, V = FVector::ZeroVector; float Birth = 0, Life = 0, Size = 1; int32 Kind = 0; bool bAlive = false; FLinearColor C; };
	struct FBurstLight { float T0 = -100, Peak = 0; };
	struct FShot { FString Time, View; };

	void OnChatMsg(const FSakuraChatMessage& Msg);
	void Release(const FPending& P);
	FFloat& Acquire();
	void SetupText(FFloat& F, const FString& User, const FLinearColor& Col, const FString& Text);
	void UpdateFloats(float Dt);
	void UpdateCamera(float Dt);
	void UpdateTime(float Dt);
	void UpdateSparks(float Dt);
	void UpdateShots(float Dt);
	void HandleKeys();
	void Demo(float Dt);
	void Explode(const FVector& P, int32 Pattern);
	int32 SpawnSpark(const FVector& P, const FVector& V, float Life, float Size, const FLinearColor& C, int32 Kind);
	void ViewAt(const FString& Name, double T, FVector& OutPos, FVector& OutLook) const;
	double ViewYaw(const FString& Name) const;
	void FlowRange(double& Z0, double& Z1) const;
	static float PresetHour(const FString& Name);

	FSakuraConfig Cfg;
	FSakuraTwitch Twitch;
	UPROPERTY(Transient) TObjectPtr<ASakuraWorld> World;
	UPROPERTY(Transient) TObjectPtr<ACameraActor> Camera;
	UPROPERTY(Transient) TObjectPtr<UMediaOutput> NDIOutput;
	UPROPERTY(Transient) TObjectPtr<UMediaCapture> NDICapture;
	UPROPERTY(Transient) TArray<TObjectPtr<UObject>> KeepAlive;

	TArray<FFloat> Floats;
	TArray<FPending> Queue;
	TMap<FString, int32> LanternByUser;
	float ReleaseIn = 0.f;
	TArray<double> RecentLanes;

	TArray<FSpark> SparkPool;
	TArray<FBurstLight> Bursts;
	TArray<FTransform> SparkXf;
	struct FRocket { int32 Spark; float ExplodeAt; int32 Pattern; float TrailIn; };
	TArray<FRocket> Rockets;
	int32 PendingFireworks = 0;
	float NextFirework = 0.f;
	bool bSparksDirty = false;

	FString ViewName = TEXT("river"), FromView;
	float ViewBlend = 1.f;
	FVector FromPos, FromLook;
	float NextTour = 0.f, TourBack = -1.f;

	float BaseHour = 13.f, TargetHour = 13.f, CurHour = 13.f, HoldUntil = -1.f;
	float RainTarget = 0.f, RainCur = 0.f, RainUntil = -1.f;
	float WindBoost = 0.f, PetalBoost = 0.f;
	float LastCommand = -100.f;
	float DemoIn = 1.f;
	int32 DemoSeed = 1;

	FString GraphicsPreset;
	TArray<FShot> Shots;
	int32 ShotIdx = -1;
	float ShotTimer = 0.f, Warm = 0.f, StableShaders = 0.f;
	double BenchSum = 0, BenchGpu = 0; int32 BenchN = 0;
	TArray<FString> BenchLines;
	bool bNDIStarted = false;
	float Elapsed = 0.f;
};
