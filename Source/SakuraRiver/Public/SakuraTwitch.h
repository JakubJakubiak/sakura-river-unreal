// Czat Twitcha bez logowania: anonimowe połączenie IRC przez WebSocket (justinfan), jak w wersji web.
#pragma once

#include "CoreMinimal.h"

class IWebSocket;

struct FSakuraChatMessage
{
	FString User;          // nazwa wyświetlana
	FString Login;         // login (małe litery)
	FLinearColor Color = FLinearColor::White;
	bool bHasColor = false;
	FString Text;
	int32 Bits = 0;
	bool bPrivileged = false;   // mod, VIP albo streamer — omija cooldown komend
};

DECLARE_DELEGATE_OneParam(FOnSakuraChat, const FSakuraChatMessage&);
/** Type: sub | resub | subgift | raid | bits; Amount: miesiące / widzowie / bity */
DECLARE_DELEGATE_ThreeParams(FOnSakuraEvent, const FString& /*Type*/, const FString& /*User*/, int32 /*Amount*/);

class FSakuraTwitch
{
public:
	~FSakuraTwitch();
	void Connect(const FString& InChannel);
	void Disconnect();
	void Tick(float DeltaSeconds);
	FString GetStatus() const { return Status; }
	bool IsJoined() const { return bJoined; }

	FOnSakuraChat OnChat;
	FOnSakuraEvent OnEvent;

private:
	void Open();
	void HandleLine(const FString& Line);
	void ScheduleReconnect();

	TSharedPtr<IWebSocket> Socket;
	FString Channel;
	FString Status = TEXT("offline");
	float ReconnectIn = -1.f;
	float Backoff = 2.f;
	float SinceTraffic = 0.f;
	bool bJoined = false;
	bool bWanted = false;
};
