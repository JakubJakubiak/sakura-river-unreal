// Ustawienia Sakura River HD: plik Sakura.ini (obok gry albo w folderze projektu) + parametry linii poleceń (-channel=nick -time=night ...).
#pragma once

#include "CoreMinimal.h"

struct FSakuraConfig
{
	FString Channel;                         // kanał Twitcha (bez #); pusty = tryb demo
	bool bDemo = false;                      // sztuczny czat (do testów i podglądu)
	FString Time = TEXT("day");              // dawn | day | sunset | night | godzina, np. 19.5
	float CycleMinutes = 0.f;                // pełna doba w minutach (0 = stała pora)
	FString View = TEXT("river");            // river | fuji | bridge | bridge2 | bridgeside | pagoda
	FString Graphics = TEXT("auto");         // auto | low | medium | high | ultra | rtx | cinematic
	bool bText = true;                       // wiadomości płyną po wodzie
	bool bLanterns = false;                  // lampiony (tōrō nagashi) z prawdziwym światłem — po jednym na osobę
	bool bNames = false;                     // nick nad wiadomością
	bool bPetals = true;                     // opadające płatki
	bool bFireflies = true;                  // świetliki nocą
	bool bHud = false;                       // pasek stanu w rogu (H przełącza)
	bool bCamera = false;                    // przeloty kamery co kilka minut
	float CameraEvery = 3.f;                 // co ile minut przelot
	int32 MaxMessages = 50;                  // ile wiadomości naraz na rzece
	float HoldSeconds = 120.f;               // ile trzyma się pora dnia ustawiona z czatu
	float CooldownSeconds = 12.f;            // globalny cooldown komend (mod/streamer omija)
	float TextScale = 1.f;                   // wielkość napisów
	float FlowSpeed = 1.f;                   // prędkość nurtu (m/s)
	int32 FpsLimit = 60;                     // limit klatek (0 = bez limitu)
	int32 ResX = 1920, ResY = 1080;          // rozmiar okna
	bool bFullscreen = false;
	bool bNDI = false;                       // wyjście NDI (OBS: wtyczka DistroAV) — działa także przy zminimalizowanym oknie
	FString NDIName = TEXT("Sakura River HD");
	FString Shots;                           // automatyczne zrzuty: "day@river,night@bridge" (do testów)
	FString ShotDir;
	float ShotWait = 4.f;

	static FSakuraConfig Load();
	/** Ścieżka pliku, z którego wczytano ustawienia (albo pusta). */
	static FString LoadedFrom;
};
