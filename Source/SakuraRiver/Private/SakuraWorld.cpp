#include "SakuraWorld.h"
#include "SakuraLayout.h"
#include "Components/StaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Engine/World.h"

using namespace Sakura;

namespace
{
	template <typename T>
	T* LoadAsset(const FString& Path) { return LoadObject<T>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet); }

	UStaticMesh* SakuraMesh(const TCHAR* Name)
	{
		return LoadAsset<UStaticMesh>(FString::Printf(TEXT("/Game/Sakura/Meshes/%s.%s"), Name, Name));
	}

	// latarnie kamienne z wersji web: [strona rzeki, z, odległość od brzegu]
	const double Lamps[][3] = { {-1, 8, 2.2}, {1, -2, 2.6}, {-1, -14, 2.2}, {1, -18, 2.4}, {1, 15, 2.5} };
	// wiśnie z wersji web: [strona, z, odległość od brzegu, wielkość, paleta]
	const double Trees[][5] = {
		{-1, 12, 3, 1.3, 0}, {1, 6, 2.5, 1.2, 1}, {-1, -6, 3, 1.1, 2}, {1, -10, 4, 1.4, 0}, {-1, -17, 5, 1.2, 1}, {1, -32, 4, 1.25, 2},
		{-1, -33, 3, 1.0, 0}, {1, -44, 8, 1.4, 1}, {-1, -30, 15, 1.4, 0}, {1, -22, 12, 1.1, 0}, {-1, 2, 9, 1.0, 2}, {1, 18, 4, 1.5, 0},
		{-1, -58, 6, 1.3, 1}, {1, -62, 5, 1.2, 2}, {1, 44, 4, 1.35, 1}, {1, 57, 5, 1.5, 0}, {1, 70, 7, 1.35, 2}, {-1, 42, 4, 1.2, 0} };

	bool Blocked(double X, double Z)
	{
		if (FVector2D::Distance({X, Z}, {PagodaX, PagodaZ}) < 9.5) return true;            // pagoda
		if (FMath::Abs(Z - BridgeZ) < 4.0 && FMath::Abs(FMath::Abs(X) - 14.5) < 4.0) return true;   // przyczółki mostu
		if (FVector2D::Distance({X, Z}, {-13.2, -10.0}) < 3.0) return true;                  // torii
		static const FVector2D Eyes[] = { {-1.5, 27.0}, {0.0, 27.0}, {13.5, -9.0}, {2.0, -23.0}, {2.25, -25.0} };   // punkty widokowe kamery
		for (const FVector2D& E : Eyes) if (FVector2D::Distance({X, Z}, E) < 5.5) return true;
		return false;
	}

	FTransform WebTransform(double X, double Y, double Z, double YawRad, double Scale)
	{
		return FTransform(FRotator(0.0, -FMath::RadiansToDegrees(YawRad), 0.0), W(X, Y, Z), FVector(Scale));
	}
}

ASakuraWorld::ASakuraWorld()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	auto MakeMesh = [this](const TCHAR* Name)
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(Root);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetMobility(EComponentMobility::Static);
		return C;
	};
	TerrainNear = MakeMesh(TEXT("TerrainNear"));
	TerrainFar = MakeMesh(TEXT("TerrainFar"));
	Fuji = MakeMesh(TEXT("Fuji"));
	Water = MakeMesh(TEXT("Water"));
	Water->SetCastShadow(false);
	SkyDome = MakeMesh(TEXT("SkyDome"));
	SkyDome->SetCastShadow(false);
	SkyDome->bVisibleInRayTracing = false;
	SkyDome->SetAffectDistanceFieldLighting(false);
	Pagoda = MakeMesh(TEXT("Pagoda"));
	Bridge = MakeMesh(TEXT("Bridge"));
	Torii = MakeMesh(TEXT("Torii"));

	auto MakeHISM = [this](const FString& Name, int32 WpoDist, bool bShadow)
	{
		UHierarchicalInstancedStaticMeshComponent* C = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(*Name);
		C->SetupAttachment(Root);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetMobility(EComponentMobility::Static);
		C->SetCastShadow(bShadow);
		C->WorldPositionOffsetDisableDistance = WpoDist;
		C->ShadowCacheInvalidationBehavior = EShadowCacheInvalidationBehavior::Rigid;     // wiatr nie unieważnia cieni co klatkę
		return C;
	};
	for (int32 I = 0; I < 3; ++I) Cherries.Add(MakeHISM(FString::Printf(TEXT("Cherry%d"), I), 12000, true));
	for (int32 I = 0; I < 2; ++I) Forest.Add(MakeHISM(FString::Printf(TEXT("Forest%d"), I), 8000, true));
	for (int32 I = 0; I < 4; ++I) Rocks.Add(MakeHISM(FString::Printf(TEXT("Rock%d"), I), 0, true));
	Grass = MakeHISM(TEXT("Grass"), 3500, false);
	Grass->SetCullDistances(0, 9000);
	StoneLanterns = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("StoneLanterns"));
	StoneLanterns->SetupAttachment(Root);
	StoneLanterns->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// słońce i księżyc — oba oświetlają atmosferę (indeksy 0 i 1)
	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(Root);
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->bAtmosphereSunLight = true;
	Sun->AtmosphereSunLightIndex = 0;
	Sun->Intensity = 120000.f;
	Sun->LightSourceAngle = 0.53f;
	Sun->bCastCloudShadows = true;
	Sun->ForwardShadingPriority = 1;
	Moon = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Moon"));
	Moon->SetupAttachment(Root);
	Moon->SetMobility(EComponentMobility::Movable);
	Moon->bAtmosphereSunLight = true;
	Moon->AtmosphereSunLightIndex = 1;
	Moon->Intensity = 0.f;
	Moon->LightColor = FColor(184, 204, 255);
	Moon->LightSourceAngle = 0.5f;

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(Root);
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->bRealTimeCapture = true;
	SkyLight->bLowerHemisphereIsBlack = false;

	Atmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Atmosphere"));
	Atmosphere->SetupAttachment(Root);
	Clouds = CreateDefaultSubobject<UVolumetricCloudComponent>(TEXT("Clouds"));
	Clouds->SetupAttachment(Root);
	Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Fog"));
	Fog->SetupAttachment(Root);
	Post = CreateDefaultSubobject<UPostProcessComponent>(TEXT("Post"));
	Post->SetupAttachment(Root);
	Post->bUnbound = true;

	// światła: 10 papierowych lampionów na moście, 5 kamiennych latarni, 2 w pagodzie
	auto AddLamp = [this](const FVector& P, const FLinearColor& Col, float Candela, float Radius)
	{
		UPointLightComponent* L = CreateDefaultSubobject<UPointLightComponent>(*FString::Printf(TEXT("Lamp%d"), LampLights.Num()));
		L->SetupAttachment(Root);
		L->SetMobility(EComponentMobility::Movable);
		L->SetRelativeLocation(P);
		L->IntensityUnits = ELightUnits::Candelas;
		L->Intensity = 0.f;
		L->AttenuationRadius = Radius;
		L->SourceRadius = 18.f;
		L->LightColor = Col.ToFColorSRGB();
		L->bUseInverseSquaredFalloff = true;
		LampLights.Add(L);
		LampBase.Add(Candela);
	};
	const FLinearColor Warm(1.0f, 0.62f, 0.32f);
	for (int32 S = -1; S <= 1; S += 2)
		for (double X : {-9.0, -4.5, 0.0, 4.5, 9.0})
			AddLamp(W(X, ArcY(X) + 1.87, BridgeZ + S * 2.25), Warm, 14.f, 1400.f);
	for (const auto& L : Lamps)
	{
		const double X = L[0] * (RiverHalf(L[1]) + L[2]);
		AddLamp(W(X, GroundH(X, L[1]) - 0.1 + 2.05, L[1]), FLinearColor(1.0f, 0.7f, 0.4f), 9.f, 1100.f);
	}
	const double PagodaY = GroundH(PagodaX, PagodaZ) - 0.3;
	AddLamp(W(PagodaX + 1.5, PagodaY + 2.2, PagodaZ + 4.5), Warm, 60.f, 2600.f);
	AddLamp(W(PagodaX + 0.4, PagodaY + 9.5, PagodaZ + 2.5), Warm, 25.f, 1800.f);
}

void ASakuraWorld::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Build();
	ApplyLighting();
}

void ASakuraWorld::BeginPlay()
{
	Super::BeginPlay();
	// aktor zapisany w mapie ma instancje z chwili zapisu — w grze budujemy od nowa, zawsze według bieżącego kodu
	Build();
	ApplyLighting();
}

void ASakuraWorld::Build()
{
	MPC = LoadAsset<UMaterialParameterCollection>(TEXT("/Game/Sakura/Materials/MPC_Sakura.MPC_Sakura"));

	TerrainNear->SetStaticMesh(SakuraMesh(TEXT("SM_TerrainNear")));
	TerrainFar->SetStaticMesh(SakuraMesh(TEXT("SM_TerrainFar")));
	Fuji->SetStaticMesh(SakuraMesh(TEXT("SM_Fuji")));
	Fuji->SetRelativeLocation(W(1250.0, -80.0, -10500.0));
	Fuji->SetRelativeScale3D(FVector(1.0));

	if (UStaticMesh* Plane = LoadAsset<UStaticMesh>(TEXT("/Engine/BasicShapes/Plane.Plane")))
	{
		Water->SetStaticMesh(Plane);
		Water->SetRelativeLocation(FVector(500000.0, 0.0, 0.0));            // płaszczyzna 16 × 12 km: od -3 km do +13 km w osi X
		Water->SetRelativeScale3D(FVector(16000.0, 12000.0, 1.0));
		Water->SetMaterial(0, LoadAsset<UMaterialInterface>(TEXT("/Game/Sakura/Materials/M_Water.M_Water")));
	}
	UMaterialInterface* SkyMat = LoadAsset<UMaterialInterface>(TEXT("/Game/Sakura/Materials/M_Sky.M_Sky"));
	UStaticMesh* SkyMesh = LoadAsset<UStaticMesh>(TEXT("/Engine/EngineSky/SM_SkySphere.SM_SkySphere"));
	if (SkyMat && SkyMesh)
	{
		SkyDome->SetStaticMesh(SkyMesh);
		const double R = FMath::Max(1.0, SkyMesh->GetBounds().SphereRadius);
		SkyDome->SetRelativeScale3D(FVector(4.0e6 / R));
		SkyDome->SetMaterial(0, SkyMat);
		SkyDome->SetVisibility(true);
	}
	else
	{
		SkyDome->SetVisibility(false);
	}

	Pagoda->SetStaticMesh(SakuraMesh(TEXT("SM_Pagoda")));
	Pagoda->SetRelativeTransform(WebTransform(PagodaX, GroundH(PagodaX, PagodaZ) - 0.3, PagodaZ, PagodaRot, PagodaScale));
	Bridge->SetStaticMesh(SakuraMesh(TEXT("SM_Bridge")));
	Bridge->SetRelativeTransform(WebTransform(0.0, 0.0, BridgeZ, 0.0, 1.0));
	Torii->SetStaticMesh(SakuraMesh(TEXT("SM_Torii")));
	Torii->SetRelativeTransform(WebTransform(-13.2, 0.0, -10.0, 0.28, 1.0));

	if (UMaterialInterface* CloudMat = LoadAsset<UMaterialInterface>(TEXT("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst.m_SimpleVolumetricCloud_Inst")))
	{
		Clouds->SetMaterial(CloudMat);
	}
	Clouds->SetLayerBottomAltitude(3.0f);
	Clouds->SetLayerHeight(8.0f);
	Atmosphere->SetAerialPespectiveViewDistanceScale(1.0f);
	Atmosphere->SetMultiScatteringFactor(1.0f);
	Atmosphere->SetHeightFogContribution(1.0f);

	Fog->SetVolumetricFog(true);
	Fog->SetVolumetricFogScatteringDistribution(0.55f);
	Fog->SetVolumetricFogDistance(9000.f);
	Fog->SetFogHeightFalloff(0.35f);
	Fog->SetFogMaxOpacity(0.85f);
	Fog->SetStartDistance(0.f);

	FPostProcessSettings& S = Post->Settings;
	S.bOverride_AutoExposureMinBrightness = S.bOverride_AutoExposureMaxBrightness = true;
	S.bOverride_AutoExposureBias = S.bOverride_AutoExposureSpeedUp = S.bOverride_AutoExposureSpeedDown = true;
	S.AutoExposureSpeedUp = 1.5f; S.AutoExposureSpeedDown = 1.5f;
	S.bOverride_BloomIntensity = true; S.BloomIntensity = 0.55f;
	S.bOverride_VignetteIntensity = true; S.VignetteIntensity = 0.32f;
	S.bOverride_bMegaLights = true; S.bMegaLights = true;
	S.bOverride_LumenFinalGatherQuality = true; S.LumenFinalGatherQuality = 1.f;
	S.bOverride_LumenReflectionQuality = true; S.LumenReflectionQuality = 1.f;
	S.bOverride_LumenSceneLightingQuality = true; S.LumenSceneLightingQuality = 1.f;
	S.bOverride_LumenMaxTraceDistance = true; S.LumenMaxTraceDistance = 60000.f;

	PlaceVegetation();
	bBuilt = true;
}

void ASakuraWorld::SetVegetationDensity(float Density)
{
	VegetationDensity = FMath::Clamp(Density, 0.f, 1.f);
	if (bBuilt) PlaceVegetation();
}

void ASakuraWorld::PlaceVegetation()
{
	const TCHAR* CherryNames[] = { TEXT("SM_Sakura_A"), TEXT("SM_Sakura_B"), TEXT("SM_Sakura_C") };
	const TCHAR* ForestNames[] = { TEXT("SM_Tree_Broad"), TEXT("SM_Tree_Sugi") };
	for (int32 I = 0; I < 3; ++I) { Cherries[I]->SetStaticMesh(SakuraMesh(CherryNames[I])); Cherries[I]->ClearInstances(); }
	for (int32 I = 0; I < 2; ++I) { Forest[I]->SetStaticMesh(SakuraMesh(ForestNames[I])); Forest[I]->ClearInstances(); }
	for (int32 I = 0; I < 4; ++I)
	{
		Rocks[I]->SetStaticMesh(SakuraMesh(*FString::Printf(TEXT("SM_Rock_%d"), I + 1)));
		Rocks[I]->ClearInstances();
	}
	Grass->SetStaticMesh(SakuraMesh(TEXT("SM_GrassPatch")));
	Grass->ClearInstances();
	StoneLanterns->SetStaticMesh(SakuraMesh(TEXT("SM_StoneLantern")));
	StoneLanterns->ClearInstances();

	FRandomStream R(20261013);
	TArray<FTransform> C[3], F[2], K[4], G, L;

	// --- wiśnie: układ z wersji web + szpalery wzdłuż rzeki w górę i w dół nurtu
	for (int32 I = 0; I < UE_ARRAY_COUNT(Trees); ++I)
	{
		const auto& T = Trees[I];
		const double Z = T[1];
		double X = RiverCx(Z) + T[0] * (RiverHalf(Z) + T[2]);
		if (Blocked(X, Z)) X += T[0] * 6.0;                    // z punktu widokowego odsuwamy drzewo dalej od rzeki
		int32 V = (I == 2 || I == 6 || I == 12) ? 2 : (int32(T[4]) == 1 ? 1 : 0);
		C[V].Add(WebTransform(X, TerrainHeight(X, Z) - 0.15, Z, R.FRandRange(0, 2 * PI), T[3] * 0.95));
	}
	auto Row = [&](double Z0, double Z1, double StepMin, double StepMax)
	{
		for (int32 Side = -1; Side <= 1; Side += 2)
			for (double Z = Z0; Z > Z1; Z -= R.FRandRange(StepMin, StepMax))
			{
				const double Off = R.FRandRange(2.5, 7.5), X = RiverCx(Z) + Side * (RiverHalf(Z) + Off);
				if (Blocked(X, Z)) continue;
				const int32 V = R.FRand() < 0.15 ? 2 : (R.FRand() < 0.5 ? 0 : 1);
				C[V].Add(WebTransform(X, TerrainHeight(X, Z) - 0.15, Z, R.FRandRange(0, 2 * PI), R.FRandRange(0.95, 1.35)));
				if (R.FRand() < 0.45)        // drugi rząd dalej od brzegu
				{
					const double X2 = RiverCx(Z) + Side * (RiverHalf(Z) + Off + R.FRandRange(7, 13));
					if (!Blocked(X2, Z)) C[R.FRand() < 0.5 ? 0 : 1].Add(WebTransform(X2, TerrainHeight(X2, Z) - 0.15, Z + R.FRandRange(-2, 2), R.FRandRange(0, 2 * PI), R.FRandRange(0.9, 1.3)));
				}
			}
	};
	Row(-66.0, -290.0, 7.0, 11.0);
	Row(105.0, 28.0, 8.0, 12.0);

	// --- las na wzgórzach (bliski teren) i daleko na górach
	const double Dens = VegetationDensity;
	for (int32 N = 0, Tries = 0; N < 1700 * Dens && Tries < 40000; ++Tries)
	{
		const double X = R.FRandRange(-158, 158), Z = R.FRandRange(-298, 108);
		const double D = RiverDist(X, Z);
		if (D < 18 || Blocked(X, Z)) continue;
		if (Fbm(X * 0.02 + 5, Z * 0.02 - 3, 3) < 0.44 - Smooth(18, 60, D) * 0.12) continue;
		const int32 V = R.FRand() < 0.68 ? 0 : 1;
		F[V].Add(WebTransform(X, TerrainHeight(X, Z) - 0.4, Z, R.FRandRange(0, 2 * PI), R.FRandRange(0.8, 1.3)));
		++N;
	}
	for (int32 N = 0, Tries = 0; N < 5000 * Dens && Tries < 60000; ++Tries)
	{
		const double A = R.FRandRange(0, 2 * PI), Rad = FMath::Sqrt(R.FRand()) * 1800.0;
		const double X = FMath::Cos(A) * Rad, Z = -300.0 + FMath::Sin(A) * Rad * 1.3;
		if (X > -160 && X < 160 && Z > -300 && Z < 110) continue;
		if (RiverDist(X, Z) < 25) continue;
		const double H = TerrainHeight(X, Z);
		if (H > 650 || Fbm(X * 0.006, Z * 0.006, 3) < 0.45) continue;
		const int32 V = (H > 250 || R.FRand() < 0.35) ? 1 : 0;
		F[V].Add(WebTransform(X, H - 1.5, Z, R.FRandRange(0, 2 * PI), R.FRandRange(1.3, 2.2)));
		++N;
	}

	// --- kamienie wzdłuż linii wody, większe na brzegach, kilka w nurcie
	for (int32 Side = -1; Side <= 1; Side += 2)
		for (double Z = 75; Z > -170; Z -= R.FRandRange(0.8, 2.2))
		{
			const double Dd = R.FRandRange(-1.3, 0.6), X = RiverCx(Z) + Side * (RiverHalf(Z) + Dd);
			if (Blocked(X, Z)) continue;
			const double S = R.FRandRange(0.22, 0.75);
			K[R.RandRange(0, 3)].Add(FTransform(FRotator(R.FRandRange(-8, 8), R.FRandRange(0, 360), R.FRandRange(-8, 8)), W(X, TerrainHeight(X, Z) - 0.12 * S, Z), FVector(S)));
		}
	for (int32 N = 0; N < 60; ++N)
	{
		const double Z = R.FRandRange(-160, 70), Side = R.FRand() < 0.5 ? -1 : 1;
		const double X = RiverCx(Z) + Side * (RiverHalf(Z) + R.FRandRange(1.0, 14.0));
		if (Blocked(X, Z)) continue;
		const double S = R.FRandRange(0.8, 2.0);
		K[R.RandRange(0, 3)].Add(FTransform(FRotator(0, R.FRandRange(0, 360), 0), W(X, TerrainHeight(X, Z) - 0.2 * S, Z), FVector(S)));
	}
	for (int32 N = 0; N < 14; ++N)
	{
		const double Z = R.FRandRange(-120, 40), X = RiverCx(Z) + R.FRandRange(-1, 1) * (RiverHalf(Z) - 4.0);
		if (FMath::Abs(Z - BridgeZ) < 6 || (Z > -2 && Z < 30 && FMath::Abs(X) < 8)) continue;   // nie w nurcie napisów przed kamerą
		const double S = R.FRandRange(0.7, 1.4);
		K[R.RandRange(0, 3)].Add(FTransform(FRotator(0, R.FRandRange(0, 360), 0), W(X, TerrainHeight(X, Z) + 0.9 * S, Z), FVector(S)));
	}

	// --- trawa przy brzegach (kępy 2×2 m)
	if (Dens > 0.05)
	{
		const double Step = 2.1 / FMath::Sqrt(Dens);
		for (double Z = -140; Z < 75; Z += Step)
			for (double X = -70; X < 70; X += Step)
			{
				const double JX = X + R.FRandRange(-0.8, 0.8) * Step, JZ = Z + R.FRandRange(-0.8, 0.8) * Step;
				const double D = RiverDist(JX, JZ);
				if (D < 0.7 || D > 42 || Blocked(JX, JZ)) continue;
				if (D > 15 && Fbm(JX * 0.08, JZ * 0.08, 2) < 0.45) continue;
				G.Add(FTransform(FRotator(0, R.FRandRange(0, 360), 0), W(JX, TerrainHeight(JX, JZ) - 0.02, JZ), FVector(R.FRandRange(0.85, 1.3))));
			}
	}

	// --- kamienne latarnie
	for (const auto& Lp : Lamps)
	{
		const double X = Lp[0] * (RiverHalf(Lp[1]) + Lp[2]);
		L.Add(WebTransform(X, GroundH(X, Lp[1]) - 0.1, Lp[1], 0.0, 1.0));
	}

	for (int32 I = 0; I < 3; ++I) Cherries[I]->AddInstances(C[I], false);
	for (int32 I = 0; I < 2; ++I) Forest[I]->AddInstances(F[I], false);
	for (int32 I = 0; I < 4; ++I) Rocks[I]->AddInstances(K[I], false);
	Grass->AddInstances(G, false);
	StoneLanterns->AddInstances(L, false);
}

void ASakuraWorld::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ApplyLighting();
}

void ASakuraWorld::SetHour(float NewHour)
{
	Hour = FMath::Fmod(NewHour + 24.f, 24.f);
	ApplyLighting();
}

void ASakuraWorld::SetFastExposure(bool bFast)
{
	Post->Settings.AutoExposureSpeedUp = Post->Settings.AutoExposureSpeedDown = bFast ? 20.f : 1.5f;
}

void ASakuraWorld::SetScalar(FName Name, float Value)
{
	if (!MPC || !GetWorld()) return;
	if (UMaterialParameterCollectionInstance* I = GetWorld()->GetParameterCollectionInstance(MPC)) I->SetScalarParameterValue(Name, Value);
}

void ASakuraWorld::SetVector(FName Name, const FLinearColor& Value)
{
	if (!MPC || !GetWorld()) return;
	if (UMaterialParameterCollectionInstance* I = GetWorld()->GetParameterCollectionInstance(MPC)) I->SetVectorParameterValue(Name, Value);
}

void ASakuraWorld::ApplyLighting()
{
	// słońce: wschodzi za kamerą (z lewej), w południe wysoko po lewej, zachodzi przed kamerą po prawej — pod światło nad rzeką
	const double Theta = (Hour - 6.0) / 12.0 * PI;
	const FVector Rise = FVector(-0.94, -0.34, 0).GetSafeNormal();
	const double NoonEl = FMath::DegreesToRadians(58.0);
	const FVector Noon = (FVector(0.34, -0.94, 0) * FMath::Cos(NoonEl) + FVector::UpVector * FMath::Sin(NoonEl)).GetSafeNormal();
	SunDir = (Rise * FMath::Cos(Theta) + Noon * FMath::Sin(Theta)).GetSafeNormal();
	const float SunEl = FMath::RadiansToDegrees(FMath::Asin(SunDir.Z));

	// księżyc: wieczorem nisko z lewej przed kamerą, w nocy przesuwa się w prawo
	const double H = Hour < 12.0 ? Hour + 24.0 : Hour;
	const double MoonEl = FMath::DegreesToRadians(26.0 * FMath::Sin(PI * (H - 18.5) / 11.0));
	const double MoonAz = FMath::DegreesToRadians(-40.0 + (H - 18.5) * 9.0);
	MoonDir = FVector(FMath::Cos(MoonEl) * FMath::Cos(MoonAz), FMath::Cos(MoonEl) * FMath::Sin(MoonAz), FMath::Sin(MoonEl));

	Night = 1.f - FMath::SmoothStep(-8.f, 2.f, SunEl);
	Sunset = (1.f - FMath::SmoothStep(4.f, 20.f, SunEl)) * FMath::SmoothStep(-7.f, 0.f, SunEl);
	const float Dawn = (Hour < 12.f ? 1.f : 0.f) * Sunset;

	Sun->SetWorldRotation((-SunDir).Rotation());
	Sun->SetIntensity(120000.f * FMath::SmoothStep(-4.f, 1.f, SunEl) * (1.f - 0.75f * Rain));
	Sun->SetVisibility(SunEl > -6.f);
	Moon->SetWorldRotation((-MoonDir).Rotation());
	Moon->SetIntensity(0.5f * Night * FMath::SmoothStep(-2.f, 6.f, float(FMath::RadiansToDegrees(MoonEl))) * (1.f - 0.6f * Rain));
	Moon->SetVisibility(Night > 0.01f);
	SkyLight->SetIntensity(1.f + Night * 0.4f);

	// ekspozycja: dzień EV100 ~12–15, noc ~-2.5..-1 (światło księżyca)
	FPostProcessSettings& S = Post->Settings;
	S.AutoExposureMinBrightness = FMath::Lerp(10.5f, -2.8f, Night);
	S.AutoExposureMaxBrightness = FMath::Lerp(15.0f, 0.5f, Night);
	S.AutoExposureBias = FMath::Lerp(0.2f, -0.5f, Night) + Sunset * 0.2f;
	S.BloomIntensity = FMath::Lerp(0.45f, 0.85f, Night);

	Fog->SetFogDensity(0.004f + 0.02f * Dawn + 0.001f * Night + 0.02f * Rain);
	Fog->SetVolumetricFogExtinctionScale(1.0f + 0.5f * Night + 2.0f * Dawn);

	const float LampOn = FMath::Pow(FMath::SmoothStep(0.15f, 0.85f, Night), 1.5f);
	for (int32 I = 0; I < LampLights.Num(); ++I)
	{
		const float Flicker = 1.f + 0.04f * FMath::Sin(GetWorld() ? GetWorld()->GetTimeSeconds() * (7.f + I) + I : 0.f);
		LampLights[I]->SetIntensity(LampBase[I] * LampOn * Flicker);
		LampLights[I]->SetVisibility(LampOn > 0.001f);
	}

	SetScalar(TEXT("Night"), Night);
	SetScalar(TEXT("Sunset"), Sunset);
	SetScalar(TEXT("EmissiveScale"), FMath::Lerp(900.f, 1.f, FMath::Pow(Night, 0.35f)));
	SetVector(TEXT("SunDir"), FLinearColor(SunDir.X, SunDir.Y, SunDir.Z, 0));
	SetVector(TEXT("MoonDir"), FLinearColor(MoonDir.X, MoonDir.Y, MoonDir.Z, 0));
	SetScalar(TEXT("Wet"), Rain);
}
