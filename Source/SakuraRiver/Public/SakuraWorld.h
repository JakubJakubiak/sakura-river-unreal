// Świat Sakura River HD: teren, rzeka, budowle, roślinność (Nanite), niebo (SkyAtmosphere + chmury wolumetryczne),
// słońce i księżyc, mgła wolumetryczna, światła latarni (MegaLights). Buduje się sam w OnConstruction — widać go też w edytorze.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SakuraWorld.generated.h"

class UStaticMeshComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UDirectionalLightComponent;
class USkyLightComponent;
class USkyAtmosphereComponent;
class UVolumetricCloudComponent;
class UExponentialHeightFogComponent;
class UPostProcessComponent;
class UPointLightComponent;
class UMaterialParameterCollection;

UCLASS()
class SAKURARIVER_API ASakuraWorld : public AActor
{
	GENERATED_BODY()

public:
	ASakuraWorld();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }

	/** pora dnia w godzinach (0–24); światło, niebo, mgła i latarnie przeliczają się od razu */
	void SetHour(float NewHour);
	float GetHour() const { return Hour; }
	float GetNight() const { return Night; }
	float GetSunset() const { return Sunset; }
	FVector GetSunDir() const { return SunDir; }
	FVector GetMoonDir() const { return MoonDir; }

	/** deszcz 0–1: przyciemnia słońce, zagęszcza mgłę, moczy teren */
	void SetRain(float NewRain) { Rain = NewRain; }
	/** Szybkie dopasowanie ekspozycji (zrzuty testowe). */
	void SetFastExposure(bool bFast);
	void SetScalar(FName Name, float Value);
	void SetVector(FName Name, const FLinearColor& Value);

	/** Gęstość trawy i lasu: 0 = brak trawy (słabe GPU), 1 = pełna. Przebudowuje instancje. */
	void SetVegetationDensity(float Density);

	UPROPERTY(EditAnywhere, Category = "Sakura", meta = (ClampMin = 0, ClampMax = 24))
	float Hour = 13.f;

	UPROPERTY(EditAnywhere, Category = "Sakura", meta = (ClampMin = 0, ClampMax = 1))
	float Rain = 0.f;

	UPROPERTY(EditAnywhere, Category = "Sakura", meta = (ClampMin = 0, ClampMax = 1))
	float VegetationDensity = 1.f;

	UPROPERTY(VisibleAnywhere, Category = "Sakura") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "Sakura") TObjectPtr<UStaticMeshComponent> TerrainNear;
	UPROPERTY(VisibleAnywhere, Category = "Sakura") TObjectPtr<UStaticMeshComponent> TerrainFar;
	UPROPERTY(VisibleAnywhere, Category = "Sakura") TObjectPtr<UStaticMeshComponent> Fuji;
	UPROPERTY(VisibleAnywhere, Category = "Sakura") TObjectPtr<UStaticMeshComponent> Water;
	UPROPERTY(VisibleAnywhere, Category = "Sakura") TObjectPtr<UStaticMeshComponent> SkyDome;
	UPROPERTY(VisibleAnywhere, Category = "Sakura") TObjectPtr<UStaticMeshComponent> Pagoda;
	UPROPERTY(VisibleAnywhere, Category = "Sakura") TObjectPtr<UStaticMeshComponent> Bridge;
	UPROPERTY(VisibleAnywhere, Category = "Sakura") TObjectPtr<UStaticMeshComponent> Torii;
	UPROPERTY(VisibleAnywhere, Category = "Sakura") TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Cherries;
	UPROPERTY(VisibleAnywhere, Category = "Sakura") TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Forest;
	UPROPERTY(VisibleAnywhere, Category = "Sakura") TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Rocks;
	UPROPERTY(VisibleAnywhere, Category = "Sakura") TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Grass;
	UPROPERTY(VisibleAnywhere, Category = "Sakura") TObjectPtr<UInstancedStaticMeshComponent> StoneLanterns;
	UPROPERTY(VisibleAnywhere, Category = "Sakura") TObjectPtr<UDirectionalLightComponent> Sun;
	UPROPERTY(VisibleAnywhere, Category = "Sakura") TObjectPtr<UDirectionalLightComponent> Moon;
	UPROPERTY(VisibleAnywhere, Category = "Sakura") TObjectPtr<USkyLightComponent> SkyLight;
	UPROPERTY(VisibleAnywhere, Category = "Sakura") TObjectPtr<USkyAtmosphereComponent> Atmosphere;
	UPROPERTY(VisibleAnywhere, Category = "Sakura") TObjectPtr<UVolumetricCloudComponent> Clouds;
	UPROPERTY(VisibleAnywhere, Category = "Sakura") TObjectPtr<UExponentialHeightFogComponent> Fog;
	UPROPERTY(VisibleAnywhere, Category = "Sakura") TObjectPtr<UPostProcessComponent> Post;
	UPROPERTY(VisibleAnywhere, Category = "Sakura") TArray<TObjectPtr<UPointLightComponent>> LampLights;

private:
	void Build();
	void PlaceVegetation();
	void ApplyLighting();

	UPROPERTY(Transient) TObjectPtr<UMaterialParameterCollection> MPC;
	TArray<float> LampBase;
	float Night = 0.f, Sunset = 0.f;
	FVector SunDir = FVector::UpVector, MoonDir = FVector::UpVector;
	bool bBuilt = false;
};
