#include "SakuraConfig.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"

FString FSakuraConfig::LoadedFrom;

namespace
{
	bool ToBool(const FString& V)
	{
		const FString L = V.TrimStartAndEnd().ToLower();
		return L == TEXT("1") || L == TEXT("true") || L == TEXT("yes") || L == TEXT("tak") || L == TEXT("on");
	}

	void Apply(FSakuraConfig& C, const FString& KeyIn, const FString& ValIn)
	{
		const FString K = KeyIn.TrimStartAndEnd().ToLower();
		const FString V = ValIn.TrimStartAndEnd().TrimQuotes();
		if (K == TEXT("channel") || K == TEXT("kanal")) C.Channel = V.Replace(TEXT("#"), TEXT("")).ToLower();
		else if (K == TEXT("demo")) C.bDemo = ToBool(V);
		else if (K == TEXT("time") || K == TEXT("pora")) C.Time = V.ToLower();
		else if (K == TEXT("cycle") || K == TEXT("cykl")) C.CycleMinutes = FCString::Atof(*V);
		else if (K == TEXT("view") || K == TEXT("widok")) C.View = V.ToLower();
		else if (K == TEXT("graphics") || K == TEXT("grafika")) C.Graphics = V.ToLower();
		else if (K == TEXT("text") || K == TEXT("tekst")) C.bText = ToBool(V);
		else if (K == TEXT("lantern") || K == TEXT("lanterns") || K == TEXT("lampiony")) C.bLanterns = ToBool(V);
		else if (K == TEXT("names") || K == TEXT("nicki")) C.bNames = ToBool(V);
		else if (K == TEXT("petals") || K == TEXT("platki")) C.bPetals = ToBool(V);
		else if (K == TEXT("fireflies") || K == TEXT("swietliki")) C.bFireflies = ToBool(V);
		else if (K == TEXT("hud")) C.bHud = ToBool(V);
		else if (K == TEXT("camera") || K == TEXT("kamera")) C.bCamera = ToBool(V);
		else if (K == TEXT("cameraevery")) C.CameraEvery = FMath::Max(0.2f, FCString::Atof(*V));
		else if (K == TEXT("max") || K == TEXT("maxmessages")) C.MaxMessages = FMath::Clamp(FCString::Atoi(*V), 1, 200);
		else if (K == TEXT("hold")) C.HoldSeconds = FCString::Atof(*V);
		else if (K == TEXT("cooldown")) C.CooldownSeconds = FCString::Atof(*V);
		else if (K == TEXT("textsize") || K == TEXT("textscale")) C.TextScale = FMath::Clamp(FCString::Atof(*V), 0.4f, 3.f);
		else if (K == TEXT("flow") || K == TEXT("nurt")) C.FlowSpeed = FMath::Clamp(FCString::Atof(*V), 0.2f, 4.f);
		else if (K == TEXT("fps")) C.FpsLimit = FMath::Max(0, FCString::Atoi(*V));
		else if (K == TEXT("resx")) C.ResX = FMath::Max(320, FCString::Atoi(*V));
		else if (K == TEXT("resy")) C.ResY = FMath::Max(240, FCString::Atoi(*V));
		else if (K == TEXT("res"))
		{
			FString A, B;
			if (V.Split(TEXT("x"), &A, &B)) { C.ResX = FMath::Max(320, FCString::Atoi(*A)); C.ResY = FMath::Max(240, FCString::Atoi(*B)); }
		}
		else if (K == TEXT("fullscreen")) C.bFullscreen = ToBool(V);
		else if (K == TEXT("minimized") || K == TEXT("minimised")) C.bRenderMinimized = ToBool(V);
		else if (K == TEXT("ndi")) C.bNDI = ToBool(V);
		else if (K == TEXT("ndiname")) C.NDIName = V;
		else if (K == TEXT("shots")) C.Shots = V;
		else if (K == TEXT("shotdir")) C.ShotDir = V;
		else if (K == TEXT("shotwait")) C.ShotWait = FCString::Atof(*V);
	}

	const TCHAR* Keys[] = {
		TEXT("channel"), TEXT("demo"), TEXT("time"), TEXT("cycle"), TEXT("view"), TEXT("graphics"), TEXT("text"), TEXT("lanterns"),
		TEXT("lantern"), TEXT("names"), TEXT("petals"), TEXT("fireflies"), TEXT("hud"), TEXT("camera"), TEXT("cameraevery"), TEXT("max"),
		TEXT("hold"), TEXT("cooldown"), TEXT("textsize"), TEXT("flow"), TEXT("fps"), TEXT("res"), TEXT("fullscreen"), TEXT("minimized"), TEXT("ndi"),
		TEXT("ndiname"), TEXT("shots"), TEXT("shotdir"), TEXT("shotwait")
	};
}

FSakuraConfig FSakuraConfig::Load()
{
	FSakuraConfig C;
	const FString Candidates[] = {
		FPaths::Combine(FPaths::LaunchDir(), TEXT("Sakura.ini")),
		FPaths::Combine(FPaths::ProjectDir(), TEXT("Sakura.ini")),
	};
	for (const FString& Path : Candidates)
	{
		TArray<FString> Lines;
		if (!FFileHelper::LoadFileToStringArray(Lines, *Path)) continue;
		for (FString Line : Lines)
		{
			Line.TrimStartAndEndInline();
			if (Line.IsEmpty() || Line.StartsWith(TEXT("#")) || Line.StartsWith(TEXT(";")) || Line.StartsWith(TEXT("["))) continue;
			FString Key, Val;
			if (Line.Split(TEXT("="), &Key, &Val))
			{
				int32 Hash;
				if (Val.FindChar(TEXT('#'), Hash) && Hash > 0 && FChar::IsWhitespace(Val[Hash - 1])) Val.LeftInline(Hash);
				Apply(C, Key, Val);
			}
		}
		LoadedFrom = FPaths::ConvertRelativePathToFull(Path);
		break;
	}
	// linia poleceń ma pierwszeństwo: -channel=nick -time=night -demo
	const TCHAR* Cmd = FCommandLine::Get();
	for (const TCHAR* Key : Keys)
	{
		FString Val;
		if (FParse::Value(Cmd, *(FString(Key) + TEXT("=")), Val, false)) Apply(C, Key, Val);
		else if (FParse::Param(Cmd, Key)) Apply(C, Key, TEXT("1"));
	}
	if (C.Channel.IsEmpty()) C.bDemo = true;
	return C;
}
