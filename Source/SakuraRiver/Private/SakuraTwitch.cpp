#include "SakuraTwitch.h"
#include "IWebSocket.h"
#include "WebSocketsModule.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogSakuraTwitch, Log, All);

FSakuraTwitch::~FSakuraTwitch() { Disconnect(); }

void FSakuraTwitch::Connect(const FString& InChannel)
{
	Channel = InChannel.ToLower().Replace(TEXT("#"), TEXT(""));
	bWanted = !Channel.IsEmpty();
	if (bWanted) Open();
}

void FSakuraTwitch::Disconnect()
{
	bWanted = false;
	if (Socket.IsValid())
	{
		Socket->OnConnected().Clear();
		Socket->OnConnectionError().Clear();
		Socket->OnClosed().Clear();
		Socket->OnMessage().Clear();
		if (Socket->IsConnected()) Socket->Close();
		Socket.Reset();
	}
	bJoined = false;
	Status = TEXT("offline");
}

void FSakuraTwitch::Open()
{
	FWebSocketsModule& WS = FModuleManager::LoadModuleChecked<FWebSocketsModule>(TEXT("WebSockets"));
	if (Socket.IsValid()) { Socket->OnClosed().Clear(); Socket->OnConnectionError().Clear(); if (Socket->IsConnected()) Socket->Close(); }
	Socket = WS.CreateWebSocket(TEXT("wss://irc-ws.chat.twitch.tv:443"), TEXT("irc"));
	Status = FString::Printf(TEXT("łączenie z #%s…"), *Channel);
	SinceTraffic = 0.f;

	Socket->OnConnected().AddLambda([this]()
	{
		Backoff = 2.f;
		Socket->Send(TEXT("CAP REQ :twitch.tv/tags twitch.tv/commands"));
		Socket->Send(TEXT("PASS SCHMOOPIIE"));
		Socket->Send(FString::Printf(TEXT("NICK justinfan%d"), FMath::RandRange(10000, 99999)));
		Socket->Send(FString::Printf(TEXT("JOIN #%s"), *Channel));
		Status = FString::Printf(TEXT("dołączanie do #%s…"), *Channel);
	});
	Socket->OnConnectionError().AddLambda([this](const FString& Err)
	{
		UE_LOG(LogSakuraTwitch, Warning, TEXT("Błąd połączenia: %s"), *Err);
		Status = TEXT("błąd połączenia — ponawiam");
		ScheduleReconnect();
	});
	Socket->OnClosed().AddLambda([this](int32 Code, const FString& Reason, bool)
	{
		UE_LOG(LogSakuraTwitch, Log, TEXT("Rozłączono (%d) %s"), Code, *Reason);
		Status = TEXT("rozłączono — ponawiam");
		ScheduleReconnect();
	});
	Socket->OnMessage().AddLambda([this](const FString& Msg)
	{
		SinceTraffic = 0.f;
		TArray<FString> Lines;
		Msg.ParseIntoArray(Lines, TEXT("\r\n"), true);
		for (const FString& L : Lines) HandleLine(L);
	});
	Socket->Connect();
}

void FSakuraTwitch::ScheduleReconnect()
{
	bJoined = false;
	if (!bWanted) return;
	ReconnectIn = Backoff;
	Backoff = FMath::Min(Backoff * 2.f, 60.f);
}

void FSakuraTwitch::Tick(float DeltaSeconds)
{
	if (!bWanted) return;
	SinceTraffic += DeltaSeconds;
	if (ReconnectIn >= 0.f)
	{
		ReconnectIn -= DeltaSeconds;
		if (ReconnectIn < 0.f) Open();
	}
	else if (SinceTraffic > 360.f && Socket.IsValid())       // Twitch wysyła PING co ~5 min; cisza = martwe połączenie
	{
		SinceTraffic = 0.f;
		Socket->Close();
	}
}

namespace
{
	FString UnescapeTag(const FString& V)
	{
		return V.Replace(TEXT("\\s"), TEXT(" ")).Replace(TEXT("\\:"), TEXT(";")).Replace(TEXT("\\\\"), TEXT("\\"))
			.Replace(TEXT("\\r"), TEXT("")).Replace(TEXT("\\n"), TEXT(""));
	}
}

void FSakuraTwitch::HandleLine(const FString& Line)
{
	if (Line.StartsWith(TEXT("PING")))
	{
		if (Socket.IsValid()) Socket->Send(TEXT("PONG") + Line.Mid(4));
		return;
	}
	// @tagi :prefiks KOMENDA #kanał :treść
	FString Rest = Line;
	TMap<FString, FString> Tags;
	if (Rest.StartsWith(TEXT("@")))
	{
		int32 Sp;
		if (!Rest.FindChar(TEXT(' '), Sp)) return;
		TArray<FString> Parts;
		Rest.Mid(1, Sp - 1).ParseIntoArray(Parts, TEXT(";"));
		for (const FString& P : Parts)
		{
			FString K, V;
			if (P.Split(TEXT("="), &K, &V)) Tags.Add(K, UnescapeTag(V)); else Tags.Add(P, FString());
		}
		Rest = Rest.Mid(Sp + 1);
	}
	FString Prefix;
	if (Rest.StartsWith(TEXT(":")))
	{
		int32 Sp;
		if (!Rest.FindChar(TEXT(' '), Sp)) return;
		Prefix = Rest.Mid(1, Sp - 1);
		Rest = Rest.Mid(Sp + 1);
	}
	FString Command, Params, Trailing;
	int32 Colon = Rest.Find(TEXT(" :"));
	if (Colon != INDEX_NONE) { Trailing = Rest.Mid(Colon + 2); Rest = Rest.Left(Colon); }
	if (!Rest.Split(TEXT(" "), &Command, &Params)) Command = Rest;

	if (Command == TEXT("JOIN") && !bJoined)
	{
		bJoined = true;
		Status = FString::Printf(TEXT("czat #%s"), *Channel);
		UE_LOG(LogSakuraTwitch, Log, TEXT("Dołączono do #%s"), *Channel);
	}
	else if (Command == TEXT("ROOMSTATE") && !bJoined)
	{
		bJoined = true;
		Status = FString::Printf(TEXT("czat #%s"), *Channel);
	}
	else if (Command == TEXT("RECONNECT"))
	{
		if (Socket.IsValid()) Socket->Close();
	}
	else if (Command == TEXT("PRIVMSG"))
	{
		FSakuraChatMessage M;
		M.Login = Prefix.Left(Prefix.Find(TEXT("!")));
		M.User = Tags.FindRef(TEXT("display-name"));
		if (M.User.IsEmpty()) M.User = M.Login;
		const FString Col = Tags.FindRef(TEXT("color"));
		if (Col.Len() == 7) { M.Color = FLinearColor(FColor::FromHex(Col)); M.bHasColor = true; }
		M.Text = Trailing;
		if (M.Text.StartsWith(TEXT("\x01" "ACTION "))) M.Text = M.Text.Mid(8).LeftChop(1);
		M.Bits = FCString::Atoi(*Tags.FindRef(TEXT("bits")));
		const FString Badges = Tags.FindRef(TEXT("badges"));
		M.bPrivileged = Tags.FindRef(TEXT("mod")) == TEXT("1") || Badges.Contains(TEXT("broadcaster")) || Badges.Contains(TEXT("vip"));
		OnChat.ExecuteIfBound(M);
		if (M.Bits > 0) OnEvent.ExecuteIfBound(TEXT("bits"), M.User, M.Bits);
	}
	else if (Command == TEXT("USERNOTICE"))
	{
		const FString Id = Tags.FindRef(TEXT("msg-id"));
		FString User = Tags.FindRef(TEXT("display-name"));
		if (User.IsEmpty()) User = Tags.FindRef(TEXT("login"));
		int32 Amount = 1;
		if (Id == TEXT("raid")) Amount = FCString::Atoi(*Tags.FindRef(TEXT("msg-param-viewerCount")));
		else if (Id == TEXT("resub")) Amount = FCString::Atoi(*Tags.FindRef(TEXT("msg-param-cumulative-months")));
		else if (Id == TEXT("submysterygift")) Amount = FCString::Atoi(*Tags.FindRef(TEXT("msg-param-mass-gift-count")));
		if (Id == TEXT("sub") || Id == TEXT("resub") || Id == TEXT("subgift") || Id == TEXT("submysterygift") || Id == TEXT("raid"))
		{
			OnEvent.ExecuteIfBound(Id, User, FMath::Max(1, Amount));
		}
	}
}
