#include "SakuraDirector.h"
#include "SakuraWorld.h"
#include "SakuraLayout.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Paths.h"
#include "RenderUtils.h"
#include "RHI.h"
#include "Scalability.h"
#include "Styling/CoreStyle.h"
#include "UnrealClient.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "MediaCapture.h"
#include "NDIMediaOutput.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogSakura, Log, All);

using namespace Sakura;

namespace
{
	template <typename T>
	T* LoadAsset(const FString& Path) { return LoadObject<T>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet); }

	const TMap<FString, FString>& Commands()
	{
		static const TMap<FString, FString> M = {
			{TEXT("!sakura"), TEXT("sakura")}, {TEXT("!kwiaty"), TEXT("sakura")}, {TEXT("!桜"), TEXT("sakura")}, {TEXT("!platki"), TEXT("sakura")},
			{TEXT("!wiatr"), TEXT("wind")}, {TEXT("!wind"), TEXT("wind")}, {TEXT("!kaze"), TEXT("wind")},
			{TEXT("!deszcz"), TEXT("rain")}, {TEXT("!rain"), TEXT("rain")}, {TEXT("!ame"), TEXT("rain")},
			{TEXT("!hanabi"), TEXT("fireworks")}, {TEXT("!fajerwerki"), TEXT("fireworks")}, {TEXT("!fireworks"), TEXT("fireworks")},
			{TEXT("!swit"), TEXT("dawn")}, {TEXT("!świt"), TEXT("dawn")}, {TEXT("!dawn"), TEXT("dawn")},
			{TEXT("!dzien"), TEXT("day")}, {TEXT("!dzień"), TEXT("day")}, {TEXT("!day"), TEXT("day")},
			{TEXT("!zachod"), TEXT("sunset")}, {TEXT("!zachód"), TEXT("sunset")}, {TEXT("!sunset"), TEXT("sunset")},
			{TEXT("!noc"), TEXT("night")}, {TEXT("!night"), TEXT("night")}, {TEXT("!yoru"), TEXT("night")},
			{TEXT("!most"), TEXT("view:bridge")}, {TEXT("!bridge"), TEXT("view:bridge")}, {TEXT("!rzeka"), TEXT("view:river")},
			{TEXT("!river"), TEXT("view:river")}, {TEXT("!fuji"), TEXT("view:fuji")}, {TEXT("!pagoda"), TEXT("view:pagoda")},
			{TEXT("!wieza"), TEXT("view:pagoda")}, {TEXT("!wieża"), TEXT("view:pagoda")},
		};
		return M;
	}

	const TArray<FString>& ViewNames()
	{
		static const TArray<FString> V = { TEXT("river"), TEXT("fuji"), TEXT("bridge"), TEXT("bridge2"), TEXT("bridgeside"), TEXT("pagoda") };
		return V;
	}

	FString NormalizeView(const FString& In)
	{
		const FString V = In.ToLower();
		if (V == TEXT("wulkan") || V == TEXT("volcano") || V == TEXT("river2") || V == TEXT("rzeka2")) return TEXT("fuji");
		if (V == TEXT("rzeka")) return TEXT("river");
		if (V == TEXT("most")) return TEXT("bridge");
		if (V == TEXT("most2") || V == TEXT("bridgefar")) return TEXT("bridge2");
		if (V == TEXT("side") || V == TEXT("bok") || V == TEXT("mostbok") || V == TEXT("most-bok")) return TEXT("bridgeside");
		if (V == TEXT("wieza") || V == TEXT("wieża") || V == TEXT("tower")) return TEXT("pagoda");
		return ViewNames().Contains(V) ? V : TEXT("river");
	}

	FLinearColor NickColor(const FString& User)
	{
		static const FColor Pal[] = { FColor(255, 105, 97), FColor(255, 180, 80), FColor(255, 214, 102), FColor(160, 220, 120), FColor(110, 200, 220),
			FColor(140, 160, 255), FColor(200, 140, 255), FColor(255, 140, 200) };
		return FLinearColor(Pal[GetTypeHash(User.ToLower()) % UE_ARRAY_COUNT(Pal)]);
	}

	// pozycja punktu widokowego pagody (oko w wieży) — jak w wersji web: TRS(pagoda) * (0, 13.15, 1.2)
	FVector PagodaLocal(double LX, double LY, double LZ)
	{
		const double S = PagodaScale, A = PagodaRot;
		const double X = LX * S * FMath::Cos(A) + LZ * S * FMath::Sin(A), Z = -LX * S * FMath::Sin(A) + LZ * S * FMath::Cos(A);
		return FVector(PagodaX + X, GroundH(PagodaX, PagodaZ) - 0.3 + LY * S, PagodaZ + Z);
	}
}

ASakuraDirector::ASakuraDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	auto MakeISM = [this](const TCHAR* Name, int32 CustomFloats)
	{
		UInstancedStaticMeshComponent* C = CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		C->SetupAttachment(Root);
		C->SetMobility(EComponentMobility::Movable);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		C->NumCustomDataFloats = CustomFloats;
		C->bAffectDistanceFieldLighting = false;
		C->bVisibleInRayTracing = false;
		C->SetCanEverAffectNavigation(false);
		return C;
	};
	Petals = MakeISM(TEXT("Petals"), 3);
	RainDrops = MakeISM(TEXT("Rain"), 3);
	Fireflies = MakeISM(TEXT("Fireflies"), 3);
	Sparks = MakeISM(TEXT("Sparks"), 5);
	Petals->SetUsingAbsoluteLocation(true);
	RainDrops->SetUsingAbsoluteLocation(true);
	for (int32 I = 0; I < 16; ++I)
	{
		UPointLightComponent* L = CreateDefaultSubobject<UPointLightComponent>(*FString::Printf(TEXT("Burst%d"), I));
		L->SetupAttachment(Root);
		L->SetMobility(EComponentMobility::Movable);
		L->IntensityUnits = ELightUnits::Candelas;
		L->Intensity = 0.f;
		L->AttenuationRadius = 25000.f;
		L->SourceRadius = 200.f;
		L->SetVisibility(false);
		L->bAffectsWorld = true;
		BurstLights.Add(L);
	}
}

// ------------------------------------------------------------------ start

void ASakuraDirector::BeginPlay()
{
	Super::BeginPlay();
	Cfg = FSakuraConfig::Load();
	UE_LOG(LogSakura, Log, TEXT("Ustawienia: %s | kanał '%s' demo=%d pora=%s widok=%s grafika=%s"),
		FSakuraConfig::LoadedFrom.IsEmpty() ? TEXT("(domyślne)") : *FSakuraConfig::LoadedFrom, *Cfg.Channel, Cfg.bDemo, *Cfg.Time, *Cfg.View, *Cfg.Graphics);

	if (TActorIterator<ASakuraWorld> It(GetWorld()); It) World = *It;
	if (!World) World = GetWorld()->SpawnActor<ASakuraWorld>();

	// okno: 1920×1080 w oknie (OBS: przechwytywanie okna/gry), limit klatek
	if (UGameUserSettings* GUS = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		if (!GetWorld()->IsPlayInEditor())
		{
			GUS->SetScreenResolution(FIntPoint(Cfg.ResX, Cfg.ResY));
			GUS->SetFullscreenMode(Cfg.bFullscreen ? EWindowMode::WindowedFullscreen : EWindowMode::Windowed);
		}
		GUS->SetFrameRateLimit(float(Cfg.FpsLimit));
		GUS->SetVSyncEnabled(false);
		GUS->ApplySettings(false);
	}

	ApplyGraphics(Cfg.Graphics);

	// kamera
	Camera = GetWorld()->SpawnActor<ACameraActor>();
	Camera->GetCameraComponent()->SetFieldOfView(72.7f);
	Camera->GetCameraComponent()->bConstrainAspectRatio = false;
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->SetViewTarget(Camera);
		PC->bShowMouseCursor = true;
	}
	ViewName = NormalizeView(Cfg.View);
	SetView(ViewName, true);

	// pora dnia
	BaseHour = PresetHour(Cfg.Time);
	TargetHour = CurHour = BaseHour;
	World->SetHour(CurHour);
	bShowHud = Cfg.bHud;

	// płatki, deszcz, świetliki — instancje animowane na GPU (materiał przesuwa je wokół kamery)
	FRandomStream R(7);
	if (UStaticMesh* PetalMesh = LoadAsset<UStaticMesh>(TEXT("/Game/Sakura/Meshes/SM_Petal.SM_Petal")))
	{
		Petals->SetStaticMesh(PetalMesh);
		const int32 N = Cfg.bPetals ? 6000 : 0;
		TArray<FTransform> X; X.Reserve(N);
		for (int32 I = 0; I < N; ++I) X.Add(FTransform(FVector(R.FRandRange(-1800, 1800), R.FRandRange(-1800, 1800), R.FRandRange(-400, 1400))));
		Petals->AddInstances(X, false);
		for (int32 I = 0; I < N; ++I) { const FVector P = X[I].GetLocation(); Petals->SetCustomData(I, { float(P.X), float(P.Y), float(P.Z) }, false); }
		Petals->MarkRenderStateDirty();
	}
	if (UStaticMesh* RainMesh = LoadAsset<UStaticMesh>(TEXT("/Game/Sakura/Meshes/SM_RainStreak.SM_RainStreak")))
	{
		RainDrops->SetStaticMesh(RainMesh);
		TArray<FTransform> X;
		for (int32 I = 0; I < 5000; ++I) X.Add(FTransform(FVector(R.FRandRange(-1300, 1300), R.FRandRange(-1300, 1300), R.FRandRange(-300, 1300))));
		RainDrops->AddInstances(X, false);
		for (int32 I = 0; I < X.Num(); ++I) { const FVector P = X[I].GetLocation(); RainDrops->SetCustomData(I, { float(P.X), float(P.Y), float(P.Z) }, false); }
		RainDrops->MarkRenderStateDirty();
	}
	if (UStaticMesh* SparkMesh = LoadAsset<UStaticMesh>(TEXT("/Game/Sakura/Meshes/SM_Spark.SM_Spark")))
	{
		Fireflies->SetStaticMesh(SparkMesh);
		Fireflies->SetMaterial(0, LoadAsset<UMaterialInterface>(TEXT("/Game/Sakura/Materials/M_Firefly.M_Firefly")));
		if (Cfg.bFireflies)
		{
			TArray<FTransform> X;
			for (int32 I = 0; I < 280; ++I)
			{
				const double Z = R.FRandRange(-70, 32), Side = R.FRand() < 0.5 ? -1 : 1;
				const double Xw = RiverCx(Z) + Side * (RiverHalf(Z) + R.FRandRange(-1.5, 9.0));
				const double Y = FMath::Max(0.0, TerrainHeight(Xw, Z)) + R.FRandRange(0.4, 2.4);
				X.Add(FTransform(FRotator::ZeroRotator, W(Xw, Y, Z), FVector(0.07)));
			}
			Fireflies->AddInstances(X, false);
			for (int32 I = 0; I < X.Num(); ++I) { const FVector P = X[I].GetLocation(); Fireflies->SetCustomData(I, { float(P.X), float(P.Y), float(P.Z) }, false); }
			Fireflies->MarkRenderStateDirty();
		}
		// iskry fajerwerków: stała pula instancji, chowane skalą 0
		Sparks->SetStaticMesh(SparkMesh);
		Sparks->SetMaterial(0, LoadAsset<UMaterialInterface>(TEXT("/Game/Sakura/Materials/M_Spark.M_Spark")));
		const int32 N = 3200;
		SparkPool.SetNum(N);
		SparkXf.Init(FTransform(FRotator::ZeroRotator, FVector(0, 0, -100000), FVector(0.0)), N);
		Sparks->AddInstances(SparkXf, false);
		Bursts.SetNum(BurstLights.Num());
	}

	// czat
	Twitch.OnChat.BindUObject(this, &ASakuraDirector::OnChatMsg);
	Twitch.OnEvent.BindUObject(this, &ASakuraDirector::Event);
	if (!Cfg.Channel.IsEmpty()) Twitch.Connect(Cfg.Channel);

	// automatyczne zrzuty (testy): -shots=day@river,night@bridge -shotdir=...
	TArray<FString> Items;
	Cfg.Shots.ParseIntoArray(Items, TEXT(","));
	for (const FString& It : Items)
	{
		FString T, V;
		if (!It.Split(TEXT("@"), &T, &V)) { T = It; V = TEXT("river"); }
		Shots.Add({ T.TrimStartAndEnd(), V.TrimStartAndEnd() });
	}
	if (Cfg.ShotDir.IsEmpty()) Cfg.ShotDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SakuraShots"));
	NextTour = Cfg.CameraEvery * 60.f;
}

void ASakuraDirector::EndPlay(const EEndPlayReason::Type Reason)
{
	Twitch.Disconnect();
	if (NDICapture) NDICapture->StopCapture(false);
	Super::EndPlay(Reason);
}

// ------------------------------------------------------------------ grafika

void ASakuraDirector::ApplyGraphics(const FString& PresetIn)
{
	FString P = PresetIn.ToLower();
	if (P.IsEmpty() || P == TEXT("auto")) P = IsRayTracingEnabled() ? TEXT("rtx") : TEXT("ultra");
	auto Set = [](const TCHAR* Name, float V)
	{
		if (IConsoleVariable* C = IConsoleManager::Get().FindConsoleVariable(Name)) C->Set(V, ECVF_SetByCode);
	};
	int32 Q = 3; float SP = 100.f; bool bHW = false; float Veg = 1.f;
	if (P == TEXT("low"))            { Q = 0; SP = 60.f; Veg = 0.f; }
	else if (P == TEXT("medium"))    { Q = 1; SP = 70.f; Veg = 0.5f; }
	else if (P == TEXT("high"))      { Q = 2; SP = 80.f; Veg = 0.8f; }
	else if (P == TEXT("rtx"))       { Q = 3; bHW = true; }
	else if (P == TEXT("cinematic")) { Q = 4; bHW = true; }
	else                             { P = TEXT("ultra"); }
	Scalability::FQualityLevels L;
	L.SetFromSingleQualityLevel(Q);
	Scalability::SetQualityLevels(L);
	Set(TEXT("r.ScreenPercentage"), SP);
	const bool bCanHW = IsRayTracingEnabled();
	Set(TEXT("r.Lumen.HardwareRayTracing"), (bHW && bCanHW) ? 1.f : 0.f);
	Set(TEXT("r.Lumen.HardwareRayTracing.LightingMode"), (bHW && bCanHW) ? 2.f : 0.f);   // odbicia z oświetleniem w miejscu trafienia
	Set(TEXT("r.MegaLights.HardwareRayTracing"), (bHW && bCanHW) ? 1.f : 0.f);
	if (World && !FMath::IsNearlyEqual(World->VegetationDensity, Veg)) World->SetVegetationDensity(Veg);
	GraphicsPreset = (bHW && !bCanHW) ? P + TEXT(" (bez RT — karta nie obsługuje)") : P;
	UE_LOG(LogSakura, Log, TEXT("Grafika: %s (skalowalność %d, rozdzielczość %d%%, sprzętowy RT %s)"), *GraphicsPreset, Q, int32(SP), (bHW && bCanHW) ? TEXT("tak") : TEXT("nie"));
}

// ------------------------------------------------------------------ pora dnia

float ASakuraDirector::PresetHour(const FString& Name)
{
	const FString N = Name.ToLower();
	if (N == TEXT("dawn") || N == TEXT("swit") || N == TEXT("świt")) return 5.75f;
	if (N == TEXT("day") || N == TEXT("dzien") || N == TEXT("dzień")) return 13.0f;
	if (N == TEXT("sunset") || N == TEXT("zachod") || N == TEXT("zachód")) return 17.85f;
	if (N == TEXT("night") || N == TEXT("noc")) return 21.0f;
	if (N.IsNumeric()) return FMath::Fmod(FCString::Atof(*N) + 24.f, 24.f);
	return 13.f;
}

void ASakuraDirector::SetTimePreset(const FString& Name, bool bFromChat, bool bSnap)
{
	TargetHour = PresetHour(Name);
	if (bFromChat) HoldUntil = Elapsed + Cfg.HoldSeconds;
	else { BaseHour = TargetHour; HoldUntil = -1.f; }
	if (bSnap) { CurHour = TargetHour; if (World) World->SetHour(CurHour); }
}

void ASakuraDirector::UpdateTime(float Dt)
{
	if (Cfg.CycleMinutes > 0.f) BaseHour = FMath::Fmod(BaseHour + Dt * 24.f / (Cfg.CycleMinutes * 60.f), 24.f);
	if (HoldUntil >= 0.f && Elapsed > HoldUntil) HoldUntil = -1.f;
	const float Goal = HoldUntil >= 0.f ? TargetHour : BaseHour;
	float D = FMath::Fmod(Goal - CurHour + 48.f, 24.f);           // zawsze do przodu, jak obrót słońca
	if (D > 12.f && HoldUntil < 0.f && Cfg.CycleMinutes <= 0.f) D -= 24.f;
	const float Step = FMath::Clamp(D, -Dt * 1.2f, Dt * 1.2f);    // ~1,2 h/s — przejście trwa kilka sekund
	if (FMath::Abs(D) > 0.001f) CurHour = FMath::Fmod(CurHour + (FMath::Abs(D) < 0.01f ? D : Step) + 24.f, 24.f);
	World->SetHour(CurHour);

	if (RainUntil >= 0.f && Elapsed > RainUntil) { RainTarget = 0.f; RainUntil = -1.f; }
	RainCur = FMath::FInterpConstantTo(RainCur, RainTarget, Dt, 0.15f);
	WindBoost = FMath::Max(0.f, WindBoost - Dt / 25.f);
	PetalBoost = FMath::Max(0.f, PetalBoost - Dt / 20.f);
	World->SetRain(RainCur);
	World->SetScalar(TEXT("Rain"), RainCur);
	World->SetScalar(TEXT("Wind"), 0.25f + WindBoost * 1.6f + RainCur * 0.3f);
	World->SetScalar(TEXT("PetalBoost"), Cfg.bPetals ? FMath::Clamp(PetalBoost + 0.0f, 0.f, 1.f) : -1.f);
}

// ------------------------------------------------------------------ czat

void ASakuraDirector::OnChatMsg(const FSakuraChatMessage& Msg) { Chat(Msg); }

void ASakuraDirector::Chat(const FSakuraChatMessage& Msg)
{
	const FString Text = Msg.Text.TrimStartAndEnd();
	const FLinearColor Col = Msg.bHasColor ? Msg.Color : NickColor(Msg.User);
	const FString Key = Msg.User.ToLower();
	if (int32* Idx = LanternByUser.Find(Key))
	{
		if (Floats.IsValidIndex(*Idx) && Floats[*Idx].bActive && Floats[*Idx].bHasLantern) Floats[*Idx].Flare = 1.f;   // lampion piszącego rozbłyska
	}
	if (Text.StartsWith(TEXT("!")))
	{
		FString First = Text, RestOfIt;
		Text.Split(TEXT(" "), &First, &RestOfIt);
		if (const FString* Fx = Commands().Find(First.ToLower()))
		{
			if (Msg.bPrivileged || Elapsed - LastCommand >= Cfg.CooldownSeconds)
			{
				LastCommand = Elapsed;
				Effect(*Fx);
			}
		}
		return;                                                    // komend nie wyświetlamy
	}
	const bool bWantLantern = Cfg.bLanterns && !(LanternByUser.Contains(Key) && Floats[LanternByUser[Key]].bActive);
	if ((Cfg.bText && !Text.IsEmpty()) || bWantLantern)
	{
		if (Queue.Num() > 40) Queue.RemoveAt(0);                  // przy zalewie wiadomości gubimy najstarsze z kolejki
		Queue.Add({ Msg.User, Col, Cfg.bText ? Text.Left(120) : FString(), bWantLantern });
	}
}

void ASakuraDirector::Event(const FString& Type, const FString& User, int32 Amount)
{
	UE_LOG(LogSakura, Log, TEXT("Zdarzenie %s od %s (%d)"), *Type, *User, Amount);
	if (Type == TEXT("bits")) { if (Amount >= 100) Fireworks(FMath::Clamp(1 + Amount / 500, 1, 6)); }
	else if (Type == TEXT("raid")) { Fireworks(FMath::Clamp(3 + Amount / 20, 3, 12)); PetalBoost = 1.f; }
	else if (Type == TEXT("submysterygift")) Fireworks(FMath::Clamp(Amount, 2, 8));
	else Fireworks(2);
}

void ASakuraDirector::Effect(const FString& Fx)
{
	if (Fx == TEXT("sakura")) PetalBoost = 1.f;
	else if (Fx == TEXT("wind")) { WindBoost = 1.f; PetalBoost = FMath::Max(PetalBoost, 0.5f); }
	else if (Fx == TEXT("rain")) { RainTarget = RainTarget > 0.5f ? 0.f : 1.f; RainUntil = RainTarget > 0.5f ? Elapsed + 90.f : -1.f; }
	else if (Fx == TEXT("fireworks")) Fireworks(6);
	else if (Fx == TEXT("dawn") || Fx == TEXT("day") || Fx == TEXT("sunset") || Fx == TEXT("night")) SetTimePreset(Fx, true);
	else if (Fx.StartsWith(TEXT("view:"))) SetView(Fx.Mid(5));
}

ASakuraDirector::FFloat& ASakuraDirector::Acquire()
{
	for (FFloat& F : Floats) if (!F.bActive) return F;
	if (Floats.Num() < Cfg.MaxMessages) return Floats.AddDefaulted_GetRef();
	// pula pełna: najstarszy znika
	FFloat* Oldest = &Floats[0];
	for (FFloat& F : Floats) if (F.Age > Oldest->Age) Oldest = &F;
	return *Oldest;
}

void ASakuraDirector::SetupText(FFloat& F, const FString& User, const FLinearColor& Col, const FString& Text)
{
	if (!F.Text)
	{
		UWidgetComponent* WC = NewObject<UWidgetComponent>(this);
		WC->SetupAttachment(Root);
		WC->SetWidgetSpace(EWidgetSpace::World);
		WC->SetTwoSided(true);
		WC->SetBlendMode(EWidgetBlendMode::Transparent);
		WC->SetDrawAtDesiredSize(false);
		WC->SetPivot(FVector2D(0.5, 0.5));
		WC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WC->SetCastShadow(false);
		WC->SetManuallyRedraw(true);
		WC->SetWindowFocusable(false);
		WC->bVisibleInRayTracing = false;
		WC->RegisterComponent();
		F.Text = WC;
		const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", 46);
		const FSlateFontInfo Small = FCoreStyle::GetDefaultFontStyle("Regular", 26);
		WC->SetSlateWidget(
			SNew(SBox).Padding(FMargin(18.f, 6.f)).HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[ SAssignNew(F.NameBlock, STextBlock).Font(Small).ColorAndOpacity(FLinearColor::White) ]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[ SAssignNew(F.TextBlock, STextBlock).Font(Font).ColorAndOpacity(FLinearColor::White).WrapTextAt(1150.f).Justification(ETextJustify::Center)
					.ShadowOffset(FVector2D(0, 0)) ]
			]);
	}
	const bool bName = Cfg.bNames && !User.IsEmpty();
	F.NameBlock->SetText(FText::FromString(bName ? User : FString()));
	F.NameBlock->SetColorAndOpacity(FSlateColor(Col));
	F.NameBlock->SetVisibility(bName ? EVisibility::Visible : EVisibility::Collapsed);
	F.TextBlock->SetText(FText::FromString(Text));
	// rozmiar płótna z pomiaru czcionki (zawijanie przy 1150 px)
	FVector2D Sz(Text.Len() * 24.0, 60.0);
	if (FSlateApplication::IsInitialized())
	{
		const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		const FVector2D One = Measure->Measure(Text, FCoreStyle::GetDefaultFontStyle("Bold", 46));
		const int32 Lines = FMath::Max(1, FMath::CeilToInt32(One.X / 1150.0));
		Sz = FVector2D(FMath::Min(One.X, 1150.0), One.Y * Lines);
	}
	Sz += FVector2D(48.0, bName ? 52.0 : 16.0);
	F.Size = Sz;
	F.Text->SetDrawSize(Sz);
	F.Text->SetVisibility(true);
	F.Text->RequestRedraw();
}

void ASakuraDirector::Release(const FPending& P)
{
	FFloat& F = Acquire();
	if (F.bActive && F.bHasLantern) LanternByUser.Remove(F.User.ToLower());
	F.bActive = true;
	F.User = P.User;
	F.Color = P.Color;
	F.Age = 0;
	F.Flare = 0.6f;
	F.Phase = FMath::FRand() * 10.0;
	F.Speed = Cfg.FlowSpeed * FMath::FRandRange(0.88f, 1.12f);
	double Z0, Z1; FlowRange(Z0, Z1);
	F.Z = Z0;
	// tor w poprzek rzeki: jak najdalej od kilku ostatnich
	double Best = 0, BestScore = -1;
	for (int32 K = 0; K < 8; ++K)
	{
		const double L = FMath::FRandRange(-0.78, 0.78);
		double Score = 10;
		for (double R : RecentLanes) Score = FMath::Min(Score, FMath::Abs(R - L));
		if (Score > BestScore) { BestScore = Score; Best = L; }
	}
	F.Lane = Best;
	RecentLanes.Add(Best);
	if (RecentLanes.Num() > 4) RecentLanes.RemoveAt(0);

	F.bHasText = !P.Text.IsEmpty();
	if (F.bHasText) SetupText(F, P.User, P.Color, P.Text);
	else if (F.Text) F.Text->SetVisibility(false);

	F.bHasLantern = P.bLantern;
	if (F.bHasLantern)
	{
		if (!F.Lantern)
		{
			F.Lantern = NewObject<UStaticMeshComponent>(this);
			F.Lantern->SetupAttachment(Root);
			F.Lantern->SetStaticMesh(LoadAsset<UStaticMesh>(TEXT("/Game/Sakura/Meshes/SM_FloatLantern.SM_FloatLantern")));
			F.Lantern->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			F.Lantern->SetMobility(EComponentMobility::Movable);
			F.Lantern->RegisterComponent();
			const int32 Slot = F.Lantern->GetMaterialIndex(TEXT("lanternpaper"));
			F.LanternMID = F.Lantern->CreateDynamicMaterialInstance(Slot >= 0 ? Slot : 0);
			F.Light = NewObject<UPointLightComponent>(this);
			F.Light->SetupAttachment(F.Lantern);
			F.Light->SetMobility(EComponentMobility::Movable);
			F.Light->SetIntensityUnits(ELightUnits::Candelas);
			F.Light->SetAttenuationRadius(750.f);
			F.Light->SetSourceRadius(12.f);
			F.Light->SetRelativeLocation(FVector(0, 0, 30));
			F.Light->RegisterComponent();
		}
		const FLinearColor Lc = FMath::Lerp(P.Color, FLinearColor(1.f, 0.6f, 0.3f), 0.45f);
		if (F.LanternMID) F.LanternMID->SetVectorParameterValue(TEXT("Emissive"), Lc);
		F.Light->SetLightColor(Lc);
		F.Lantern->SetVisibility(true, true);
		LanternByUser.Add(P.User.ToLower(), int32(&F - Floats.GetData()));
	}
	else if (F.Lantern) F.Lantern->SetVisibility(false, true);
}

void ASakuraDirector::FlowRange(double& Z0, double& Z1) const
{
	const bool bNear = ViewName == TEXT("river") || ViewName == TEXT("fuji");
	Z0 = bNear ? -20.5 : -38.0;
	Z1 = bNear ? 28.0 : 80.0;
}

void ASakuraDirector::UpdateFloats(float Dt)
{
	ReleaseIn -= Dt;
	if (Queue.Num() && ReleaseIn <= 0.f)
	{
		Release(Queue[0]);
		Queue.RemoveAt(0);
		ReleaseIn = Queue.Num() > 10 ? 0.35f : 0.8f;
	}
	const float Night = World ? World->GetNight() : 0.f;
	const float EmScale = FMath::Lerp(900.f, 1.f, FMath::Pow(Night, 0.35f));
	const FVector Cam = Camera ? Camera->GetActorLocation() : FVector::ZeroVector;
	const double Yaw = ViewYaw(ViewName);
	const FVector Away(FMath::Cos(Yaw), FMath::Sin(Yaw), 0.0);          // od kamery w górę rzeki (UE XY)
	double Z0, Z1; FlowRange(Z0, Z1);
	const FLinearColor Ink(0.035f, 0.03f, 0.045f, 0.82f);
	const FLinearColor Glow(1.0f, 0.88f, 0.72f, 0.95f);
	for (int32 I = 0; I < Floats.Num(); ++I)
	{
		FFloat& F = Floats[I];
		if (!F.bActive) continue;
		F.Age += Dt;
		F.Z += F.Speed * Dt;
		F.Flare = FMath::Max(0.f, F.Flare - Dt * 0.8f);
		if (F.Z > Z1 + 2.0 || F.Age > 240.0)
		{
			F.bActive = false;
			if (F.Text) F.Text->SetVisibility(false);
			if (F.Lantern) F.Lantern->SetVisibility(false, true);
			if (F.bHasLantern) LanternByUser.Remove(F.User.ToLower());
			continue;
		}
		const double X = RiverCx(F.Z) + F.Lane * RiverHalf(F.Z) * 0.78;
		const FVector P = W(X, 0.0, F.Z);
		const float Fade = FMath::Clamp(float(F.Age) / 2.f, 0.f, 1.f) * FMath::Clamp(float(Z1 + 2.0 - F.Z) / 6.f, 0.f, 1.f);
		if (F.bHasText && F.Text)
		{
			// napis leży płasko na wodzie, górą od kamery; rozciągnięty z odległością, żeby był czytelny z daleka
			const double Dist = FVector::Dist2D(Cam, P);
			const double SinA = FMath::Max(Cam.Z, 50.0) / FMath::Max(FVector::Dist(Cam, P), 1.0);
			const double Stretch = FMath::Clamp(0.42 / SinA, 1.0, 7.0) * Cfg.TextScale;
			const FMatrix M = FRotationMatrix::MakeFromXZ(FVector::UpVector, Away);
			const FVector Bob(0, 0, 3.0 + FMath::Sin(Elapsed * 1.3 + F.Phase) * 1.5);
			F.Text->SetWorldLocationAndRotation(P + Bob, M.Rotator());
			F.Text->SetWorldScale3D(FVector(1.0, Cfg.TextScale, Stretch));
			FLinearColor C = FMath::Lerp(Ink, Glow * FLinearColor(0.11f, 0.11f, 0.11f, 1.f), FMath::SmoothStep(0.3f, 0.8f, Night));
			C.A *= Fade * FMath::Clamp(1.f - float(Dist) / 9000.f, 0.25f, 1.f);
			F.Text->SetTintColorAndOpacity(C);
		}
		if (F.bHasLantern && F.Lantern)
		{
			const double Depth = F.bHasText ? F.Size.Y * FMath::Clamp(0.42 / (FMath::Max(Cam.Z, 50.0) / FMath::Max(FVector::Dist(Cam, P), 1.0)), 1.0, 7.0) * 0.5 + 130.0 : 0.0;
			const FVector LP = P + Away * Depth + FVector(0, 0, FMath::Sin(Elapsed * 1.4 + F.Phase) * 3.5 - 2.0);
			F.Lantern->SetWorldLocationAndRotation(LP, FRotator(FMath::Sin(Elapsed * 0.9 + F.Phase) * 3.0, F.Phase * 36.0 + Elapsed * 4.0, FMath::Sin(Elapsed * 0.7 + F.Phase * 2.0) * 3.0));
			const float Glow2 = (1.f + F.Flare * 2.5f) * Fade;
			if (F.LanternMID) F.LanternMID->SetScalarParameterValue(TEXT("Glow"), Glow2);
			F.Light->SetIntensity(7.f * Glow2 * FMath::SmoothStep(0.05f, 0.6f, Night));
			F.Light->SetVisibility(Night > 0.05f);
		}
	}
}

// ------------------------------------------------------------------ fajerwerki

int32 ASakuraDirector::SpawnSpark(const FVector& P, const FVector& V, float Life, float Size, const FLinearColor& C, int32 Kind)
{
	static int32 Cursor = 0;
	const int32 N = SparkPool.Num();
	if (!N) return -1;
	for (int32 K = 0; K < N; ++K)
	{
		const int32 I = (Cursor + K) % N;
		if (SparkPool[I].bAlive) continue;
		Cursor = I + 1;
		FSpark& S = SparkPool[I];
		S.P = P; S.V = V; S.Birth = GetWorld()->GetTimeSeconds(); S.Life = Life; S.Size = Size; S.C = C; S.Kind = Kind; S.bAlive = true;
		Sparks->SetCustomData(I, { C.R, C.G, C.B, S.Birth, Life }, false);
		bSparksDirty = true;
		return I;
	}
	return -1;
}

void ASakuraDirector::Fireworks(int32 Count)
{
	PendingFireworks = FMath::Min(PendingFireworks + Count, 24);
}

void ASakuraDirector::Explode(const FVector& P, int32 Pattern)
{
	static const FLinearColor Pal[] = { FLinearColor(1.0f, 0.55f, 0.75f), FLinearColor(1.0f, 0.78f, 0.35f), FLinearColor(0.45f, 0.85f, 1.0f),
		FLinearColor(1.0f, 0.3f, 0.25f), FLinearColor(0.55f, 1.0f, 0.5f), FLinearColor(0.85f, 0.6f, 1.0f), FLinearColor(1.0f, 0.95f, 0.85f) };
	const FLinearColor A = Pal[FMath::RandRange(0, 6)], B = Pal[FMath::RandRange(0, 6)];
	const int32 N = FMath::RandRange(130, 175);
	const float Speed = FMath::FRandRange(1300.f, 2000.f);
	for (int32 I = 0; I < N; ++I)
	{
		FVector D = FMath::VRand();
		float Sp = Speed * FMath::FRandRange(0.85f, 1.05f);
		if (Pattern == 1) { D.Z *= 0.35f; D.Normalize(); }                 // pierścień
		if (Pattern == 2) Sp *= FMath::FRandRange(0.3f, 1.0f);                // peonia (pełna kula)
		const bool bB = (Pattern == 3) ? (D.Z > 0) : (FMath::FRand() < 0.3f);
		SpawnSpark(P, D * Sp, FMath::FRandRange(1.6f, 2.7f), FMath::FRandRange(55.f, 85.f), bB ? B : A, 1);
	}
	// światło wybuchu — oświetla wodę, brzegi i pagodę (MegaLights)
	int32 Li = 0; float Oldest = 1e9f;
	for (int32 I = 0; I < Bursts.Num(); ++I) if (Bursts[I].T0 < Oldest) { Oldest = Bursts[I].T0; Li = I; }
	Bursts[Li].T0 = Elapsed; Bursts[Li].Peak = 26000.f;
	BurstLights[Li]->SetWorldLocation(P);
	BurstLights[Li]->SetLightColor(FMath::Lerp(A, B, 0.3f));
	BurstLights[Li]->SetVisibility(true);
}

void ASakuraDirector::UpdateSparks(float Dt)
{
	if (!SparkPool.Num()) return;
	// kolejne rakiety
	NextFirework -= Dt;
	if (PendingFireworks > 0 && NextFirework <= 0.f && Camera)
	{
		--PendingFireworks;
		NextFirework = FMath::FRandRange(0.25f, 0.7f);
		const FVector Cam = Camera->GetActorLocation();
		FVector Fwd = Camera->GetActorForwardVector(); Fwd.Z = 0; Fwd.Normalize();
		const FVector Right = FVector::CrossProduct(FVector::UpVector, Fwd);
		const float Dist = FMath::FRandRange(7500.f, 11000.f);
		const float Elev = FMath::DegreesToRadians(FMath::FRandRange(9.f, 17.f));
		const FVector Burst = Cam + Fwd * Dist + Right * FMath::FRandRange(-2600.f, 2600.f) + FVector(0, 0, Dist * FMath::Tan(Elev));
		const FVector Start = FVector(Burst.X, Burst.Y, 0.f) + FVector(FMath::FRandRange(-300.f, 300.f), FMath::FRandRange(-300.f, 300.f), 0.f);
		const float T = FMath::FRandRange(1.1f, 1.6f);
		const FVector V = (Burst - Start) / T + FVector(0, 0, 0.5f * 400.f * T);
		const int32 S = SpawnSpark(Start, V, T + 0.05f, 70.f, FLinearColor(1.f, 0.8f, 0.5f), 0);
		if (S >= 0) Rockets.Add({ S, Elapsed + T, FMath::RandRange(0, 3), 0.f });
	}
	for (int32 I = Rockets.Num() - 1; I >= 0; --I)
	{
		FRocket& R = Rockets[I];
		if (!SparkPool.IsValidIndex(R.Spark)) { Rockets.RemoveAt(I); continue; }
		FSpark& S = SparkPool[R.Spark];
		R.TrailIn -= Dt;
		if (R.TrailIn <= 0.f) { R.TrailIn = 0.035f; SpawnSpark(S.P, FMath::VRand() * 60.f - S.V * 0.05f, 0.5f, 35.f, FLinearColor(1.f, 0.7f, 0.4f), 2); }
		if (Elapsed >= R.ExplodeAt) { Explode(S.P, R.Pattern); S.bAlive = false; Rockets.RemoveAt(I); }
	}
	// fizyka iskier
	const float Now = GetWorld()->GetTimeSeconds();
	bool bAny = false;
	for (int32 I = 0; I < SparkPool.Num(); ++I)
	{
		FSpark& S = SparkPool[I];
		if (!S.bAlive)
		{
			if (SparkXf[I].GetScale3D().X != 0.0) { SparkXf[I].SetScale3D(FVector(0.0)); bSparksDirty = true; }
			continue;
		}
		if (Now - S.Birth > S.Life) { S.bAlive = false; SparkXf[I].SetScale3D(FVector(0.0)); bSparksDirty = true; continue; }
		bAny = true;
		const float G = S.Kind == 0 ? 400.f : 330.f;
		const float Drag = S.Kind == 1 ? 1.1f : 0.2f;
		S.V *= FMath::Exp(-Drag * Dt);
		S.V.Z -= G * Dt;
		S.P += S.V * Dt;
		const float Len = 1.f + FMath::Min(S.V.Size() * 0.0045f, 5.f);
		const FVector Dir = S.V.GetSafeNormal(SMALL_NUMBER, FVector::UpVector);
		SparkXf[I] = FTransform(FRotationMatrix::MakeFromX(Dir).ToQuat(), S.P, FVector(S.Size * Len, S.Size, S.Size) / 100.f);
		bSparksDirty = true;
	}
	if (bSparksDirty)
	{
		Sparks->BatchUpdateInstancesTransforms(0, SparkXf, true, true, true);
		bSparksDirty = bAny;
	}
	for (int32 I = 0; I < Bursts.Num(); ++I)
	{
		const float Age = Elapsed - Bursts[I].T0;
		const float K = Age < 0.08f ? Age / 0.08f : FMath::Exp(-(Age - 0.08f) * 2.2f);
		const float Night = World ? World->GetNight() : 0.f;
		BurstLights[I]->SetIntensity(Bursts[I].Peak * K * FMath::Lerp(25.f, 1.f, Night));
		BurstLights[I]->SetVisibility(Age < 3.f);
	}
}

// ------------------------------------------------------------------ kamera

double ASakuraDirector::ViewYaw(const FString& Name) const
{
	FVector P, L;
	ViewAt(Name, 0.0, P, L);
	const FVector D = L - P;
	return FMath::Atan2(D.Y, D.X);
}

void ASakuraDirector::ViewAt(const FString& Name, double T, FVector& OutPos, FVector& OutLook) const
{
	// pozycje z wersji web (VIEWS w index.html), w metrach web, z tym samym powolnym dryfem
	FVector P, L;
	if (Name == TEXT("fuji"))
	{
		P = FVector(FMath::Sin(T * 0.045) * 2.0, 3.5 + FMath::Sin(T * 0.07) * 0.45, 27 + FMath::Cos(T * 0.045) * 1.2);
		L = FVector(-0.8 + FMath::Sin(T * 0.03) * 0.8, 5.6, -24);
	}
	else if (Name == TEXT("bridge"))
	{
		P = FVector(1.95 + FMath::Sin(T * 0.05) * 0.22, ArcY(1.95) + 1.66 + FMath::Sin(T * 0.07) * 0.05, BridgeZ + 0.85);
		L = FVector(0.4 + FMath::Sin(T * 0.033) * 2.2, 0.8, 6);
	}
	else if (Name == TEXT("bridge2"))
	{
		P = FVector(2.25 + FMath::Sin(T * 0.05) * 0.2, ArcY(2.25) + 1.95 + FMath::Sin(T * 0.07) * 0.04, BridgeZ - 1.0);
		L = FVector(0.6 + FMath::Sin(T * 0.033) * 1.6, 2.2, 14);
	}
	else if (Name == TEXT("bridgeside"))
	{
		P = FVector(13.5 + FMath::Sin(T * 0.05) * 0.3, 3.2 + FMath::Sin(T * 0.07) * 0.08, -9 + FMath::Cos(T * 0.045) * 0.3);
		L = FVector(-2 + FMath::Sin(T * 0.03) * 1.2, 2.8, -20);
	}
	else if (Name == TEXT("pagoda"))
	{
		P = PagodaLocal(0, 13.15, 1.2) + FVector(FMath::Sin(T * 0.05) * 0.06, FMath::Sin(T * 0.07) * 0.04, 0);
		L = PagodaLocal(0, 0, 36); L.Y = 1.5; L.X += FMath::Sin(T * 0.03) * 2.5;
	}
	else
	{
		P = FVector(-1.5 + FMath::Sin(T * 0.045) * 2.0, 3.7 + FMath::Sin(T * 0.07) * 0.45, 27 + FMath::Cos(T * 0.045) * 1.2);
		L = FVector(8 + FMath::Sin(T * 0.03) * 0.8, 4.8, -24);
	}
	OutPos = W(P.X, P.Y, P.Z);
	OutLook = W(L.X, L.Y, L.Z);
}

void ASakuraDirector::SetView(const FString& NameIn, bool bInstant)
{
	FString Name = NameIn.ToLower();
	const TArray<FString>& All = ViewNames();
	if (Name == TEXT("next") || Name == TEXT("prev"))
	{
		const int32 I = FMath::Max(0, All.IndexOfByKey(ViewName));
		Name = All[(I + (Name == TEXT("next") ? 1 : All.Num() - 1)) % All.Num()];
	}
	Name = NormalizeView(Name);
	if (Camera && !bInstant)
	{
		FromPos = Camera->GetActorLocation();
		FromLook = FromPos + Camera->GetActorForwardVector() * 3000.0;
		ViewBlend = 0.f;
	}
	else ViewBlend = 1.f;
	ViewName = Name;
	if (Camera && bInstant)
	{
		FVector P, L; ViewAt(ViewName, Elapsed, P, L);
		Camera->SetActorLocationAndRotation(P, (L - P).Rotation());
	}
}

void ASakuraDirector::UpdateCamera(float Dt)
{
	if (!Camera) return;
	// przeloty co kilka minut (camera=1): na chwilę inny punkt widokowy, potem powrót
	if (Cfg.bCamera && Shots.Num() == 0)
	{
		if (TourBack < 0.f && Elapsed > NextTour) { FromView = ViewName; SetView(TEXT("next")); TourBack = Elapsed + 40.f; }
		else if (TourBack >= 0.f && Elapsed > TourBack) { SetView(FromView); TourBack = -1.f; NextTour = Elapsed + Cfg.CameraEvery * 60.f; }
	}
	FVector P, L;
	ViewAt(ViewName, Elapsed, P, L);
	if (ViewBlend < 1.f)
	{
		ViewBlend = FMath::Min(1.f, ViewBlend + Dt / 4.f);
		const float E = FMath::InterpEaseInOut(0.f, 1.f, ViewBlend, 2.2f);
		const double Lift = FMath::Square(FMath::Sin(PI * E)) * FMath::Min(600.0, FVector::Dist(FromPos, P) * 0.14);   // łukiem nad mostem i koronami
		P = FMath::Lerp(FromPos, P, E) + FVector(0, 0, Lift);
		L = FMath::Lerp(FromLook, L, E);
	}
	Camera->SetActorLocationAndRotation(P, (L - P).Rotation());
	Petals->SetWorldLocation(P);
	RainDrops->SetWorldLocation(P);
}

// ------------------------------------------------------------------ klawisze, demo, zrzuty

void ASakuraDirector::HandleKeys()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC) return;
	auto Hit = [PC](const FKey& K) { return PC->WasInputKeyJustPressed(K); };
	if (Hit(EKeys::One)) SetTimePreset(TEXT("dawn"), false);
	if (Hit(EKeys::Two)) SetTimePreset(TEXT("day"), false);
	if (Hit(EKeys::Three)) SetTimePreset(TEXT("sunset"), false);
	if (Hit(EKeys::Four)) SetTimePreset(TEXT("night"), false);
	if (Hit(EKeys::Right)) SetView(TEXT("next"));
	if (Hit(EKeys::Left)) SetView(TEXT("prev"));
	if (Hit(EKeys::F)) Fireworks(6);
	if (Hit(EKeys::P)) Effect(TEXT("sakura"));
	if (Hit(EKeys::R)) Effect(TEXT("rain"));
	if (Hit(EKeys::W)) Effect(TEXT("wind"));
	if (Hit(EKeys::H)) bShowHud = !bShowHud;
	if (Hit(EKeys::L)) Cfg.bLanterns = !Cfg.bLanterns;
	if (Hit(EKeys::N)) Cfg.bNames = !Cfg.bNames;
	if (Hit(EKeys::T))
	{
		FSakuraChatMessage M; M.User = TEXT("test"); M.Text = TEXT("桜が咲いた — sakura w pełnym rozkwicie 🌸"); Chat(M);
	}
	if (Hit(EKeys::G))
	{
		static const TCHAR* Order[] = { TEXT("low"), TEXT("medium"), TEXT("high"), TEXT("ultra"), TEXT("rtx"), TEXT("cinematic") };
		int32 I = 0;
		for (int32 K = 0; K < 6; ++K) if (GraphicsPreset.StartsWith(Order[K])) I = K;
		ApplyGraphics(Order[(I + 1) % 6]);
	}
	if (Hit(EKeys::F11))
	{
		if (UGameUserSettings* GUS = GEngine->GetGameUserSettings())
		{
			GUS->SetFullscreenMode(GUS->GetFullscreenMode() == EWindowMode::Windowed ? EWindowMode::WindowedFullscreen : EWindowMode::Windowed);
			GUS->ApplyResolutionSettings(false);
		}
	}
}

void ASakuraDirector::Demo(float Dt)
{
	if (!Cfg.bDemo) return;
	DemoIn -= Dt;
	if (DemoIn > 0.f) return;
	DemoIn = FMath::FRandRange(1.4f, 4.0f);
	static const TCHAR* Names[] = { TEXT("Hanami_Kitsune"), TEXT("kasia_zen"), TEXT("TokyoDrift92"), TEXT("mochi_cat"), TEXT("Wojtek_PL"), TEXT("yuki_onna"),
		TEXT("ramen_enjoyer"), TEXT("NightOwl"), TEXT("sakura_bot"), TEXT("Kenji"), TEXT("ola_maluje"), TEXT("lofi_beats") };
	static const TCHAR* Texts[] = { TEXT("ale pięknie dziś na rzece"), TEXT("so peaceful, love this stream"), TEXT("桜がきれい"), TEXT("dobry wieczór wszystkim!"),
		TEXT("this is my favorite background"), TEXT("ktoś jeszcze ogląda o tej porze?"), TEXT("お疲れ様です"), TEXT("hanami vibes 🌸"),
		TEXT("pozdrawiam z Krakowa"), TEXT("the lanterns look so real"), TEXT("czy to Unreal?"), TEXT("gg"), TEXT("relaksik"), TEXT("first time here, hello!"),
		TEXT("きれいな夜ですね"), TEXT("muzyka + to tło = idealnie") };
	static const TCHAR* Cmds[] = { TEXT("!sakura"), TEXT("!hanabi"), TEXT("!wiatr"), TEXT("!sakura") };
	FSakuraChatMessage M;
	M.User = Names[FMath::RandRange(0, UE_ARRAY_COUNT(Names) - 1)];
	M.Text = FMath::FRand() < 0.07f ? Cmds[FMath::RandRange(0, 3)] : Texts[FMath::RandRange(0, UE_ARRAY_COUNT(Texts) - 1)];
	Chat(M);
}

void ASakuraDirector::UpdateShots(float Dt)
{
	if (!Shots.Num()) return;
	Warm += Dt;
	if (ShotIdx < 0)
	{
		int32 Jobs = 0;
#if WITH_EDITOR
		if (GShaderCompilingManager) Jobs = GShaderCompilingManager->GetNumRemainingJobs();
#endif
		StableShaders = Jobs == 0 ? StableShaders + Dt : 0.f;
		if (Warm < 8.f || StableShaders < 3.f) return;
		ShotIdx = 0;
		SetTimePreset(Shots[0].Time, false, true);
		SetView(Shots[0].View, true);
		World->SetFastExposure(true);
		ShotTimer = Cfg.ShotWait;
		BenchSum = BenchGpu = 0; BenchN = 0;
		return;
	}
	if (ShotIdx >= Shots.Num()) return;
	ShotTimer -= Dt;
	if (ShotTimer < Cfg.ShotWait * 0.5f)
	{
		BenchSum += FApp::GetDeltaTime() * 1000.0;
		BenchGpu += FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
		++BenchN;
	}
	if (ShotTimer > 0.f) return;
	const FShot& S = Shots[ShotIdx];
	const FString Path = FPaths::Combine(Cfg.ShotDir, FString::Printf(TEXT("%02d_%s_%s.png"), ShotIdx, *S.Time, *S.View));
	FScreenshotRequest::RequestScreenshot(Path, false, false);
	const FString Line = FString::Printf(TEXT("%s@%s: klatka %.2f ms, GPU %.2f ms"), *S.Time, *S.View, BenchN ? BenchSum / BenchN : 0.0, BenchN ? BenchGpu / BenchN : 0.0);
	BenchLines.Add(Line);
	UE_LOG(LogSakura, Display, TEXT("SakuraShot %s -> %s"), *Line, *Path);
	++ShotIdx;
	if (ShotIdx < Shots.Num())
	{
		SetTimePreset(Shots[ShotIdx].Time, false, true);
		SetView(Shots[ShotIdx].View, true);
		ShotTimer = Cfg.ShotWait + 0.5f;
		BenchSum = BenchGpu = 0; BenchN = 0;
		if (Shots[ShotIdx].Time == TEXT("night")) Fireworks(3);
	}
	else
	{
		for (const FString& L : BenchLines) UE_LOG(LogSakura, Display, TEXT("SakuraBench %s"), *L);
		FTimerHandle H;
		GetWorldTimerManager().SetTimer(H, []() { FGenericPlatformMisc::RequestExit(false); }, 1.5f, false);
	}
}

// ------------------------------------------------------------------ pętla

void ASakuraDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!World) return;
	Elapsed += DeltaSeconds;
	HintTime = FMath::Max(0.f, HintTime - DeltaSeconds);
	Twitch.Tick(DeltaSeconds);
	HandleKeys();
	Demo(DeltaSeconds);
	UpdateTime(DeltaSeconds);
	UpdateCamera(DeltaSeconds);
	UpdateFloats(DeltaSeconds);
	UpdateSparks(DeltaSeconds);
	UpdateShots(DeltaSeconds);

	// NDI: zaczyna, gdy okno gry już istnieje
	if (Cfg.bNDI && !bNDIStarted && Elapsed > 1.f)
	{
		bNDIStarted = true;
		UNDIMediaOutput* Out = NewObject<UNDIMediaOutput>(this);
		Out->SourceName = Cfg.NDIName;
		Out->FrameRate = FFrameRate(Cfg.FpsLimit > 0 ? Cfg.FpsLimit : 60, 1);
		NDIOutput = Out;
		NDICapture = Out->CreateMediaCapture();
		if (NDICapture)
		{
			FMediaCaptureOptions Opt;
			Opt.bSkipFrameWhenRunningExpensiveTasks = false;
			const bool bOk = NDICapture->CaptureActiveSceneViewport(Opt);
			UE_LOG(LogSakura, Log, TEXT("NDI '%s': %s"), *Cfg.NDIName, bOk ? TEXT("nadaje") : TEXT("nie udało się (zainstalowany NDI Runtime?)"));
		}
	}
}

FString ASakuraDirector::StatusLine() const
{
	const FString Chat = Cfg.Channel.IsEmpty() ? TEXT("demo (brak kanału)") : Twitch.GetStatus() + (Cfg.bDemo ? TEXT(" + demo") : TEXT(""));
	return FString::Printf(TEXT("Sakura River HD  |  %s  |  %02d:%02d  |  widok: %s  |  grafika: %s  |  %.0f fps"),
		*Chat, int32(CurHour), int32(FMath::Frac(CurHour) * 60.f), *ViewName, *GraphicsPreset, 1.f / FMath::Max(FApp::GetDeltaTime(), 0.001));
}

FString ASakuraDirector::HelpText() const
{
	return TEXT("1–4 pora dnia   ←/→ widok   F fajerwerki   P płatki   R deszcz   W wiatr   T test wiadomości   L lampiony   N nicki   G grafika   F11 pełny ekran   H pasek");
}
