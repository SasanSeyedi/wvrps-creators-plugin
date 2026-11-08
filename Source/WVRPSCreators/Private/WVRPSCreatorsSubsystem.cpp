// WVRPS Creators subsystem: pairing (device-code grant), creator token in GConfig, catalog (ETag), download with the glTF
// magic check, Interchange import (ImportAssetAsync), MML characters as body-then-gear bound to the body's USkeleton.
#include "WVRPSCreatorsSubsystem.h"

#include "WVRPSMml.h"
#include "WVRPSCreatorsPanelHost.h"

#include "Async/Async.h"
#include "AssetImportTask.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundNode.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "Components/AudioComponent.h"
#include "Factories/SoundCueFactoryNew.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Editor.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Animation/Skeleton.h"
#include "FileHelpers.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HttpModule.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Interfaces/IPluginManager.h"
#include "InterchangeGenericAssetsPipeline.h"
#include "InterchangeGenericAssetsPipelineSharedSettings.h"
#include "InterchangeManager.h"
#include "InterchangePipelineBase.h"
#include "InterchangeProjectSettings.h"
#include "InterchangeResult.h"
#include "InterchangeResultsContainer.h"
#include "InterchangeSourceData.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Package.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SWindow.h"

DEFINE_LOG_CATEGORY_STATIC(LogWVRPSCreators, Log, All);

namespace
{
	const TCHAR* GSection = TEXT("WVRPSCreators");

	TSharedPtr<FJsonObject> ParseJson(const FString& Text)
	{
		TSharedPtr<FJsonObject> Obj;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Obj))
		{
			return nullptr;
		}
		return Obj;
	}

	FString JStr(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		FString S;
		if (O.IsValid())
		{
			const TSharedPtr<FJsonValue> V = O->TryGetField(Key);
			if (V.IsValid() && (V->Type == EJson::String || V->Type == EJson::Number))
			{
				S = V->AsString();
			}
		}
		return S;
	}

	double JNum(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, double Default = -1.0)
	{
		if (O.IsValid())
		{
			const TSharedPtr<FJsonValue> V = O->TryGetField(Key);
			if (V.IsValid() && V->Type == EJson::Number)
			{
				return V->AsNumber();
			}
		}
		return Default;
	}

	bool JBool(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, bool Default = false)
	{
		if (O.IsValid())
		{
			const TSharedPtr<FJsonValue> V = O->TryGetField(Key);
			if (V.IsValid() && V->Type == EJson::Boolean)
			{
				return V->AsBool();
			}
		}
		return Default;
	}

	FString Sanitize(const FString& In, int32 MaxLen = 48)
	{
		FString Out;
		for (TCHAR C : In)
		{
			if (Out.Len() >= MaxLen)
			{
				break;
			}
			const bool bOk = (C >= 'A' && C <= 'Z') || (C >= 'a' && C <= 'z') || (C >= '0' && C <= '9') || C == '_';
			Out.AppendChar(bOk ? C : TEXT('_'));
		}
		return Out.IsEmpty() ? FString(TEXT("asset")) : Out;
	}

	/** "https://x/y/boom-hammer.b12c2659ef.glb?v=1" -> "boom_hammer_b12c2659ef" */
	FString BaseNameOfUrl(const FString& Url)
	{
		FString Path = Url;
		int32 Q = INDEX_NONE;
		if (Path.FindChar(TEXT('?'), Q))
		{
			Path.LeftInline(Q);
		}
		FString Leaf = FPaths::GetCleanFilename(Path);
		if (Leaf.EndsWith(TEXT(".glb"), ESearchCase::IgnoreCase))
		{
			Leaf.LeftChopInline(4);
		}
		else if (Leaf.EndsWith(TEXT(".wav"), ESearchCase::IgnoreCase) || Leaf.EndsWith(TEXT(".mp3"), ESearchCase::IgnoreCase))
		{
			Leaf.LeftChopInline(4);
		}
		return Sanitize(Leaf, 40);
	}

	bool HasGlbMagic(const TArray<uint8>& Bytes)
	{
		return Bytes.Num() >= 12 && Bytes[0] == 'g' && Bytes[1] == 'l' && Bytes[2] == 'T' && Bytes[3] == 'F';
	}

	/** RIFF....WAVE for wav; ID3 tag or an MPEG frame sync for mp3. The SPA fallback (text/html) fails both. */
	bool HasAudioMagic(const TArray<uint8>& B, const FString& Kind)
	{
		if (B.Num() < 12)
		{
			return false;
		}
		if (Kind == TEXT("wav"))
		{
			return B[0] == 'R' && B[1] == 'I' && B[2] == 'F' && B[3] == 'F' && B[8] == 'W' && B[9] == 'A' && B[10] == 'V' && B[11] == 'E';
		}
		return (B[0] == 'I' && B[1] == 'D' && B[2] == '3') || (B[0] == 0xFF && (B[1] & 0xE0) == 0xE0);
	}

	FString KindFromExtension(const FString& Url)
	{
		FString Path = Url;
		int32 Q = INDEX_NONE;
		if (Path.FindChar(TEXT('?'), Q))
		{
			Path.LeftInline(Q);
		}
		const FString Ext = FPaths::GetExtension(Path).ToLower();
		if (Ext == TEXT("glb")) return TEXT("glb");
		if (Ext == TEXT("wav")) return TEXT("wav");
		if (Ext == TEXT("mp3")) return TEXT("mp3");
		if (Ext == TEXT("mml")) return TEXT("mml");
		return Ext; // "json" (brick kit) and anything else: listed, not importable
	}

	USkeleton* FindSkeletonUnder(const FString& PackagePath)
	{
		IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		TArray<FAssetData> Found;
		AR.GetAssetsByPath(FName(*PackagePath), Found, true);
		for (const FAssetData& A : Found)
		{
			if (A.AssetClassPath == USkeleton::StaticClass()->GetClassPathName())
			{
				return Cast<USkeleton>(A.GetAsset());
			}
		}
		return nullptr;
	}

	const TCHAR* StateName(EWVRPSJobState S)
	{
		switch (S)
		{
		case EWVRPSJobState::Queued: return TEXT("queued");
		case EWVRPSJobState::Resolving: return TEXT("resolving");
		case EWVRPSJobState::Downloading: return TEXT("downloading");
		case EWVRPSJobState::Importing: return TEXT("importing");
		case EWVRPSJobState::Done: return TEXT("done");
		case EWVRPSJobState::Failed: return TEXT("failed");
		}
		return TEXT("?");
	}
}

// ------------------------------------------------------------------------------------------------------------------
// Lifecycle, settings

const TCHAR* UWVRPSCreatorsSubsystem::LicenseLine()
{
	// RULING 16 (founder, verbatim intent). Same line on the Hub page and in this plugin.
	// Identical to the API's `license` (api/creator/README.md, whoami.php / catalog.php).
	return TEXT("You may use these WVRPS assets in your own games and projects, including commercially. ")
		TEXT("You may not resell the assets, or transfer any commercial rights in them to anyone else.");
}

FString UWVRPSCreatorsSubsystem::PluginVersion()
{
	const TSharedPtr<IPlugin> P = IPluginManager::Get().FindPlugin(TEXT("WVRPSCreators"));
	return P.IsValid() ? P->GetDescriptor().VersionName : FString(TEXT("?"));
}

FString UWVRPSCreatorsSubsystem::GetLicense() const
{
	// The server's text wins when present (whoami/manifest); the built-in line is the floor.
	if (!WhoLicense.IsEmpty() && WhoLicense.Len() > 24)
	{
		return WhoLicense;
	}
	if (!ManifestLicense.IsEmpty() && ManifestLicense.Len() > 24)
	{
		return ManifestLicense;
	}
	return LicenseLine();
}

UWVRPSCreatorsSubsystem* UWVRPSCreatorsSubsystem::Get()
{
	return GEditor ? GEditor->GetEditorSubsystem<UWVRPSCreatorsSubsystem>() : nullptr;
}

void UWVRPSCreatorsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	CacheDir = FPaths::Combine(FPlatformProcess::UserSettingsDir(), TEXT("WVRPSCreators"), TEXT("Cache"));
	LoadSettings();
	ImportTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UWVRPSCreatorsSubsystem::TickImports), 0.1f);
	PollTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UWVRPSCreatorsSubsystem::TickPoll), 0.25f);
	Log(FString::Printf(TEXT("WVRPS Creators %s ready. Base %s, signed in: %s"), *PluginVersion(), *BaseUrl, IsSignedIn() ? TEXT("yes") : TEXT("no")));
}

void UWVRPSCreatorsSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(ImportTicker);
	FTSTicker::GetCoreTicker().RemoveTicker(PollTicker);
	Super::Deinitialize();
}

void UWVRPSCreatorsSubsystem::LoadSettings()
{
	// Editor per-project user settings (Saved/Config/.../EditorPerProjectUserSettings.ini). Never a file in Content.
	FString S;
	if (GConfig->GetString(GSection, TEXT("BaseUrl"), S, GEditorPerProjectIni) && !S.IsEmpty())
	{
		BaseUrl = S;
	}
	GConfig->GetString(GSection, TEXT("Token"), Token, GEditorPerProjectIni);
	GConfig->GetDouble(GSection, TEXT("TokenExp"), TokenExp, GEditorPerProjectIni);
	if (GConfig->GetString(GSection, TEXT("CacheDir"), S, GEditorPerProjectIni) && !S.IsEmpty())
	{
		CacheDir = S;
	}
	if (!Token.IsEmpty() && TokenExp > 0.0 && TokenExp < FDateTime::UtcNow().ToUnixTimestamp())
	{
		Log(TEXT("Stored creator key has expired: pair again."));
		Token.Reset();
	}
}

void UWVRPSCreatorsSubsystem::SaveSettings()
{
	GConfig->SetString(GSection, TEXT("BaseUrl"), *BaseUrl, GEditorPerProjectIni);
	GConfig->SetString(GSection, TEXT("Token"), *Token, GEditorPerProjectIni);
	GConfig->SetDouble(GSection, TEXT("TokenExp"), TokenExp, GEditorPerProjectIni);
	GConfig->SetString(GSection, TEXT("CacheDir"), *CacheDir, GEditorPerProjectIni);
	GConfig->Flush(false, GEditorPerProjectIni);
}

void UWVRPSCreatorsSubsystem::SetBaseUrl(const FString& InBaseUrl)
{
	FString U = InBaseUrl.TrimStartAndEnd();
	while (U.EndsWith(TEXT("/")))
	{
		U.LeftChopInline(1);
	}
	if (U.IsEmpty())
	{
		U = TEXT("https://wvrps.com");
	}
	if (U != BaseUrl)
	{
		BaseUrl = U;
		CatalogETag.Reset();
		SaveSettings();
		Log(FString::Printf(TEXT("Base URL set to %s"), *BaseUrl));
		Bump();
	}
}

void UWVRPSCreatorsSubsystem::SetCacheDir(const FString& Dir)
{
	CacheDir = Dir;
	SaveSettings();
}

void UWVRPSCreatorsSubsystem::Log(const FString& Line)
{
	const FString Stamped = FDateTime::Now().ToString(TEXT("%H:%M:%S  ")) + Line;
	UE_LOG(LogWVRPSCreators, Display, TEXT("%s"), *Line);
	LogLines.Add(Stamped);
	if (LogLines.Num() > 400)
	{
		LogLines.RemoveAt(0, LogLines.Num() - 400);
	}
	Bump();
}

FString UWVRPSCreatorsSubsystem::GetLogText() const
{
	return FString::Join(LogLines, TEXT("\n"));
}

void UWVRPSCreatorsSubsystem::Bump()
{
	++Revision;
	Changed.Broadcast();
}

FString UWVRPSCreatorsSubsystem::ApiUrl(const TCHAR* Path) const
{
	return BaseUrl + Path;
}

TSharedRef<IHttpRequest, ESPMode::ThreadSafe> UWVRPSCreatorsSubsystem::MakeRequest(const FString& Verb, const FString& Url, bool bAuth) const
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(Url);
	Req->SetVerb(Verb);
	Req->SetHeader(TEXT("User-Agent"), FString::Printf(TEXT("WVRPSCreators-UE/%s"), *PluginVersion()));
	Req->SetHeader(TEXT("Accept"), TEXT("application/json, model/gltf-binary, */*"));
	if (bAuth && !Token.IsEmpty())
	{
		Req->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + Token);
	}
	Req->SetTimeout(120.0f);
	return Req;
}

// ------------------------------------------------------------------------------------------------------------------
// Pairing

FString UWVRPSCreatorsSubsystem::GetApproveUrl() const
{
	return UserCode.IsEmpty() ? FString() : BaseUrl + TEXT("/creators?code=") + UserCode;
}

void UWVRPSCreatorsSubsystem::StartPairing(bool bOpenBrowser)
{
	if (IsSignedIn())
	{
		// A second grant while a key is stored only mints a second key (5-key cap). The panel hides Pair while signed in;
		// this guards every other caller (console, Python).
		Log(TEXT("Already signed in: press Sign out first if you want to pair again."));
		return;
	}
	PairState = TEXT("starting");
	UserCode.Reset();
	GrantId.Reset();
	ClaimSecret.Reset();
	LastError.Reset();
	Bump();

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = MakeRequest(TEXT("POST"), ApiUrl(TEXT("/api/device/grant_start.php")), false);
	Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	{
		// The label is what the user sees on wvrps.com before approving ("Creator key · UE 5.5 - MyGame"), as the README's example.
		TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
		Body->SetStringField(TEXT("client"), TEXT("creator"));
		Body->SetStringField(TEXT("label"), FString::Printf(TEXT("UE %d.%d - %s"), FEngineVersion::Current().GetMajor(), FEngineVersion::Current().GetMinor(), FApp::GetProjectName()).Left(60));
		FString BodyText;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> W = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&BodyText);
		FJsonSerializer::Serialize(Body, W);
		Req->SetContentAsString(BodyText);
	}
	TWeakObjectPtr<UWVRPSCreatorsSubsystem> Weak(this);
	Req->OnProcessRequestComplete().BindLambda([Weak, bOpenBrowser](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
	{
		UWVRPSCreatorsSubsystem* Self = Weak.Get();
		if (!Self)
		{
			return;
		}
		const TSharedPtr<FJsonObject> J = (bOk && Resp.IsValid()) ? ParseJson(Resp->GetContentAsString()) : nullptr;
		if (!J.IsValid() || !J->HasField(TEXT("grant_id")))
		{
			Self->PairState = TEXT("error");
			Self->LastError = bOk && Resp.IsValid()
				? FString::Printf(TEXT("Pairing failed: HTTP %d from %s"), Resp->GetResponseCode(), *Self->BaseUrl)
				: FString::Printf(TEXT("Pairing failed: cannot reach %s"), *Self->BaseUrl);
			Self->Log(Self->LastError);
			return;
		}
		Self->GrantId = JStr(J, TEXT("grant_id"));
		Self->ClaimSecret = JStr(J, TEXT("claim_secret"));
		Self->UserCode = JStr(J, TEXT("user_code"));
		const double ExpiresIn = JNum(J, TEXT("expires_in"), 300.0);
		Self->PairExpiresAt = FPlatformTime::Seconds() + ExpiresIn;
		Self->NextPollAt = FPlatformTime::Seconds() + 2.0;
		Self->PairState = TEXT("awaiting_approval");
		Self->Log(FString::Printf(TEXT("Pairing code %s. Approve it at %s (expires in %.0f s)."), *Self->UserCode, *Self->GetApproveUrl(), ExpiresIn));
		if (bOpenBrowser)
		{
			FPlatformProcess::LaunchURL(*Self->GetApproveUrl(), nullptr, nullptr);
		}
	});
	Req->ProcessRequest();
	Log(FString::Printf(TEXT("Requesting a pairing code from %s ..."), *BaseUrl));
}

void UWVRPSCreatorsSubsystem::CancelPairing()
{
	if (PairState == TEXT("awaiting_approval") || PairState == TEXT("starting"))
	{
		PairState = TEXT("idle");
		UserCode.Reset();
		GrantId.Reset();
		ClaimSecret.Reset();
		Log(TEXT("Pairing cancelled."));
	}
}

bool UWVRPSCreatorsSubsystem::TickPoll(float)
{
	if (PairState != TEXT("awaiting_approval"))
	{
		return true;
	}
	const double Now = FPlatformTime::Seconds();
	if (Now > PairExpiresAt)
	{
		PairState = TEXT("expired");
		Log(TEXT("The pairing code expired before it was approved. Press Pair to get a new one."));
		return true;
	}
	if (!bPollInFlight && Now >= NextPollAt)
	{
		PollGrant();
	}
	return true;
}

void UWVRPSCreatorsSubsystem::PollGrant()
{
	bPollInFlight = true;
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = MakeRequest(TEXT("POST"), ApiUrl(TEXT("/api/device/grant_poll.php")), false);
	Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("grant_id"), GrantId);
	Body->SetStringField(TEXT("claim_secret"), ClaimSecret);
	FString BodyText;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> W = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&BodyText);
	FJsonSerializer::Serialize(Body, W);
	Req->SetContentAsString(BodyText);
	TWeakObjectPtr<UWVRPSCreatorsSubsystem> Weak(this);
	const FString ForGrant = GrantId;
	Req->OnProcessRequestComplete().BindLambda([Weak, ForGrant](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
	{
		UWVRPSCreatorsSubsystem* Self = Weak.Get();
		if (!Self)
		{
			return;
		}
		Self->bPollInFlight = false;
		Self->NextPollAt = FPlatformTime::Seconds() + 2.0;
		if (Self->GrantId != ForGrant || Self->PairState != TEXT("awaiting_approval"))
		{
			return; // cancelled or superseded
		}
		const TSharedPtr<FJsonObject> J = (bOk && Resp.IsValid()) ? ParseJson(Resp->GetContentAsString()) : nullptr;
		if (!J.IsValid())
		{
			return; // transient: keep polling until expiry
		}
		const FString Status = JStr(J, TEXT("status"));
		if (Status == TEXT("expired"))
		{
			Self->PairState = TEXT("expired");
			Self->Log(TEXT("The pairing code expired. Press Pair to get a new one."));
		}
		else if (Status == TEXT("approved"))
		{
			const FString Tok = JStr(J, TEXT("token"));
			if (Tok.IsEmpty())
			{
				Self->PairState = TEXT("error");
				Self->LastError = TEXT("Approved, but the token was already delivered to another client. Pair again.");
				Self->Log(Self->LastError);
				return;
			}
			Self->Token = Tok;
			Self->TokenExp = JNum(J, TEXT("exp"), 0.0);
			Self->PairState = TEXT("approved");
			Self->UserCode.Reset();
			Self->SaveSettings();
			Self->Log(TEXT("Approved. Creator key stored in the editor's per-project user settings."));
			Self->RefreshWhoAmI();
			Self->RefreshCatalog();
		}
	});
	Req->ProcessRequest();
}

void UWVRPSCreatorsSubsystem::SignOut()
{
	const bool bHadKey = !Token.IsEmpty();
	if (bHadKey)
	{
		// README: revoke_self is the plugin's "Sign out" (works even while the API is paused). Fire-and-forget: the
		// local key is dropped either way, so a dead network never leaves the editor signed in. The bearer is the key
		// being revoked, so the request is built before Token is cleared.
		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = MakeRequest(TEXT("POST"), ApiUrl(TEXT("/api/creator/revoke_self.php")), true);
		TWeakObjectPtr<UWVRPSCreatorsSubsystem> Weak(this);
		Req->OnProcessRequestComplete().BindLambda([Weak](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
		{
			if (UWVRPSCreatorsSubsystem* Self = Weak.Get())
			{
				const int32 Code = (bOk && Resp.IsValid()) ? Resp->GetResponseCode() : 0;
				Self->Log(Code == 200 ? FString(TEXT("The old creator key was revoked on the server."))
					: FString::Printf(TEXT("Server-side revoke of the old key did not confirm (HTTP %d); revoke it on wvrps.com (Signed-in devices) if it is still listed."), Code));
			}
		});
		Req->ProcessRequest();
	}
	Token.Reset();
	TokenExp = 0.0;
	WhoLabel.Reset();
	WhoWallet.Reset();
	WhoTier.Reset();
	QuotaLimit = QuotaUsed = QuotaRemaining = -1;
	PairState = TEXT("idle");
	SaveSettings();
	Log(bHadKey ? TEXT("Signed out: the creator key was removed from this editor and a revoke was sent to the server.") : TEXT("Signed out."));
}

void UWVRPSCreatorsSubsystem::RefreshWhoAmI()
{
	if (!IsSignedIn())
	{
		return;
	}
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = MakeRequest(TEXT("GET"), ApiUrl(TEXT("/api/creator/whoami.php")), true);
	TWeakObjectPtr<UWVRPSCreatorsSubsystem> Weak(this);
	Req->OnProcessRequestComplete().BindLambda([Weak](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
	{
		UWVRPSCreatorsSubsystem* Self = Weak.Get();
		if (!Self)
		{
			return;
		}
		if (bOk && Resp.IsValid() && Resp->GetResponseCode() == 401)
		{
			Self->Log(TEXT("The creator key was rejected (expired or revoked). Pair again."));
			Self->SignOut();
			return;
		}
		const TSharedPtr<FJsonObject> J = (bOk && Resp.IsValid() && Resp->GetResponseCode() == 200) ? ParseJson(Resp->GetContentAsString()) : nullptr;
		if (!J.IsValid())
		{
			Self->Log(TEXT("whoami failed; the key is kept, try Refresh."));
			return;
		}
		Self->WhoLabel = JStr(J, TEXT("label"));
		Self->WhoWallet = JStr(J, TEXT("wallet"));
		Self->WhoTier = JStr(J, TEXT("tier"));
		Self->WhoLicense = JStr(J, TEXT("license"));
		const double Exp = JNum(J, TEXT("exp"), 0.0);
		if (Exp > 0.0)
		{
			Self->TokenExp = Exp;
		}
		const TSharedPtr<FJsonObject>* Q = nullptr;
		if (J->TryGetObjectField(TEXT("quota"), Q) && Q && Q->IsValid())
		{
			Self->QuotaLimit = (int64)JNum(*Q, TEXT("limit_bytes"), -1.0);
			Self->QuotaUsed = (int64)JNum(*Q, TEXT("used_bytes"), -1.0);
			Self->QuotaRemaining = (int64)JNum(*Q, TEXT("remaining_bytes"), -1.0);
		}
		const FString Who = FString::Printf(TEXT("Signed in as %s (%s tier)."), Self->WhoLabel.IsEmpty() ? TEXT("creator") : *Self->WhoLabel, *Self->WhoTier);
		if (!Self->LogLines.ContainsByPredicate([&Who](const FString& L) { return L.EndsWith(Who); }))
		{
			Self->Log(Who);
		}
		if (Self->QuotaUsed >= 0 && Self->QuotaLimit > 0)
		{
			const FString QuotaLine = FString::Printf(TEXT("Quota today: %.1f MB of %.0f MB used (%lld bytes), %.1f MB left."),
				Self->QuotaUsed / (1024.0 * 1024.0), Self->QuotaLimit / (1024.0 * 1024.0), Self->QuotaUsed, FMath::Max<int64>(0, Self->QuotaRemaining) / (1024.0 * 1024.0));
			if (QuotaLine != Self->LastQuotaLine)
			{
				Self->LastQuotaLine = QuotaLine;
				Self->Log(QuotaLine);
			}
		}
		Self->Bump();
	});
	Req->ProcessRequest();
}

// ------------------------------------------------------------------------------------------------------------------
// Catalog

void UWVRPSCreatorsSubsystem::RefreshCatalog()
{
	if (!IsSignedIn())
	{
		CatalogState = TEXT("error");
		LastError = TEXT("Pair this editor first (Connect tab).");
		Bump();
		return;
	}
	CatalogState = TEXT("loading");
	CatalogRequestedAt = FPlatformTime::Seconds();
	Bump();
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = MakeRequest(TEXT("GET"), ApiUrl(TEXT("/api/creator/catalog.php")), true);
	if (!CatalogETag.IsEmpty() && Items.Num() > 0)
	{
		Req->SetHeader(TEXT("If-None-Match"), CatalogETag);
	}
	TWeakObjectPtr<UWVRPSCreatorsSubsystem> Weak(this);
	Req->OnProcessRequestComplete().BindLambda([Weak](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
	{
		UWVRPSCreatorsSubsystem* Self = Weak.Get();
		if (!Self)
		{
			return;
		}
		const int32 Code = (bOk && Resp.IsValid()) ? Resp->GetResponseCode() : 0;
		if (Code == 304)
		{
			Self->CatalogLoadSeconds = (float)(FPlatformTime::Seconds() - Self->CatalogRequestedAt);
			Self->CatalogParseSeconds = 0.0f;
			Self->CatalogState = TEXT("ready");
			Self->Log(FString::Printf(TEXT("Catalog unchanged (ETag %s), %d items."), *Self->CatalogETag, Self->Items.Num()));
			return;
		}
		if (Code == 401)
		{
			Self->CatalogState = TEXT("error");
			Self->Log(TEXT("Catalog: the creator key was rejected. Pair again."));
			Self->SignOut();
			return;
		}
		const double ParseStart = FPlatformTime::Seconds();
		const TSharedPtr<FJsonObject> J = Code == 200 ? ParseJson(Resp->GetContentAsString()) : nullptr;
		if (!J.IsValid())
		{
			Self->CatalogState = TEXT("error");
			Self->LastError = Code == 0 ? FString::Printf(TEXT("Cannot reach %s"), *Self->BaseUrl)
				: FString::Printf(TEXT("Catalog failed: HTTP %d"), Code);
			Self->Log(Self->LastError);
			return;
		}
		Self->CatalogETag = Resp->GetHeader(TEXT("ETag"));
		Self->HandleCatalogJson(J);
		const double Now = FPlatformTime::Seconds();
		Self->CatalogParseSeconds = (float)(Now - ParseStart);
		Self->CatalogLoadSeconds = (float)(Now - Self->CatalogRequestedAt);
		Self->Log(FString::Printf(TEXT("Catalog loaded in %.2f s (%.2f s JSON parse + model), %d bytes."), Self->CatalogLoadSeconds, Self->CatalogParseSeconds, Resp->GetContent().Num()));
	});
	Req->ProcessRequest();
}

void UWVRPSCreatorsSubsystem::HandleCatalogJson(const TSharedPtr<FJsonObject>& Root)
{
	Items.Reset();
	Biomes.Reset();
	Categories.Reset();
	ManifestGenerated = JStr(Root, TEXT("generated"));
	ManifestLicense = JStr(Root, TEXT("license"));
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (Root->TryGetArrayField(TEXT("biomes"), Arr) && Arr)
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject> B = V.IsValid() ? V->AsObject() : nullptr;
			if (!B.IsValid())
			{
				continue;
			}
			FWVRPSBiome Bio;
			Bio.Id = JStr(B, TEXT("id"));
			Bio.Name = JStr(B, TEXT("name"));
			Bio.Descriptor = JStr(B, TEXT("descriptor"));
			Bio.ThumbUrl = JStr(B, TEXT("thumb_url"));
			if (!Bio.Id.IsEmpty())
			{
				Biomes.Add(Bio);
			}
		}
	}
	if (Root->TryGetArrayField(TEXT("categories"), Arr) && Arr)
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject> C = V.IsValid() ? V->AsObject() : nullptr;
			if (!C.IsValid())
			{
				continue;
			}
			FWVRPSCategory Cat;
			Cat.Slug = JStr(C, TEXT("slug"));
			Cat.Label = JStr(C, TEXT("label"));
			Cat.Engine = JStr(C, TEXT("engine"));
			Cat.Group = JStr(C, TEXT("group"));
			Cat.PerBiome = (int32)JNum(C, TEXT("per_biome"), 0.0);
			if (Cat.Group.IsEmpty())
			{
				// Fixed fallback (not schema-driven re-derivation): the real taxonomy's two groups.
				static const TSet<FString> MusicSlugs = { TEXT("explore"), TEXT("tension"), TEXT("combat"), TEXT("theme"), TEXT("rest") };
				Cat.Group = MusicSlugs.Contains(Cat.Slug) ? TEXT("music") : TEXT("sfx");
			}
			if (!Cat.Slug.IsEmpty())
			{
				Categories.Add(Cat);
			}
		}
	}
	if (Root->TryGetArrayField(TEXT("items"), Arr) && Arr)
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject> O = V.IsValid() ? V->AsObject() : nullptr;
			if (!O.IsValid())
			{
				continue;
			}
			TSharedPtr<FWVRPSCatalogItem> It = MakeShared<FWVRPSCatalogItem>();
			It->Id = JStr(O, TEXT("id"));
			It->Class = JStr(O, TEXT("class"));
			It->Name = JStr(O, TEXT("name"));
			It->ThumbUrl = JStr(O, TEXT("thumb_url"));
			It->ThumbHqUrl = JStr(O, TEXT("thumb_hq_url"));
			It->TokenId = JStr(O, TEXT("token_id"));
			It->Bytes = (int64)JNum(O, TEXT("bytes"), -1.0);
			It->Tier = JStr(O, TEXT("tier"));
			It->License = JStr(O, TEXT("license"));
			It->Source = JStr(O, TEXT("source"));
			It->Character = JStr(O, TEXT("character"));
			It->Variant = JStr(O, TEXT("variant"));
			It->Biome = JStr(O, TEXT("biome"));
			It->Category = JStr(O, TEXT("category"));
			It->Engine = JStr(O, TEXT("engine"));
			It->Mood = JStr(O, TEXT("mood"));
			It->Format = JStr(O, TEXT("format"));
			It->Key = JStr(O, TEXT("key"));
			It->VariationGroup = JStr(O, TEXT("variation_group"));
			It->QualityStatus = JStr(O, TEXT("quality_status"));
			It->Intensity = (int32)JNum(O, TEXT("intensity"), 0.0);
			It->DurationSec = (float)JNum(O, TEXT("duration_sec"), 0.0);
			It->bLoopable = JBool(O, TEXT("loopable"), false);
			It->Bpm = JNum(O, TEXT("bpm"), 0.0);
			struct FK { const TCHAR* Field; const TCHAR* Kind; };
			for (const FK& K : { FK{ TEXT("glb_url"), TEXT("glb") }, FK{ TEXT("mml_url"), TEXT("mml") }, FK{ TEXT("voxel_raw_url"), TEXT("voxel") }, FK{ TEXT("mp3_url"), TEXT("mp3") }, FK{ TEXT("wav_url"), TEXT("wav") } })
			{
				const FString U = JStr(O, K.Field);
				if (!U.IsEmpty())
				{
					It->Url = U;
					It->UrlKind = K.Kind;
					break;
				}
			}
			if (It->Url.IsEmpty())
			{
				// The real catalog projects only glb/stl/mp3 typed URLs; per-token songs (.wav) and brick kits (.json)
				// arrive as plain `url`. Type them by extension.
				It->Url = JStr(O, TEXT("url"));
				It->UrlKind = KindFromExtension(It->Url);
			}
			if (It->Id.IsEmpty())
			{
				continue;
			}
			Items.Add(It);
		}
	}
	CatalogState = TEXT("ready");
	Log(FString::Printf(TEXT("Catalog: %d items (manifest generated %s)."), Items.Num(), *ManifestGenerated));
}

// ------------------------------------------------------------------------------------------------------------------
// Import queue

bool UWVRPSCreatorsSubsystem::ImportItem(const FString& Id, const FString& DestRoot)
{
	const TSharedPtr<FWVRPSCatalogItem>* Found = Items.FindByPredicate([&Id](const TSharedPtr<FWVRPSCatalogItem>& I) { return I->Id == Id; });
	if (!Found)
	{
		Log(FString::Printf(TEXT("Import: unknown item %s (refresh the catalog)."), *Id));
		return false;
	}
	FString Root = DestRoot.IsEmpty() ? FString(TEXT("/Game/WVRPS")) : DestRoot;
	while (Root.EndsWith(TEXT("/")))
	{
		Root.LeftChopInline(1);
	}
	if (!Root.StartsWith(TEXT("/Game")))
	{
		Log(FString::Printf(TEXT("Import: destination must be under /Game (got %s)."), *Root));
		return false;
	}
	TSharedPtr<FWVRPSImportJob> Job = MakeShared<FWVRPSImportJob>();
	Job->ItemId = Id;
	Job->Item = **Found;
	Job->DestRoot = FString::Printf(TEXT("%s/%s/%s"), *Root, *Sanitize(Job->Item.Class.IsEmpty() ? FString(TEXT("misc")) : Job->Item.Class), *Sanitize(Id));
	Job->Message = TEXT("Waiting");
	if (!Job->Item.IsImportable())
	{
		Log(FString::Printf(TEXT("Import: %s not imported. %s"), *Id, *Job->Item.NotImportableReason()));
		return false;
	}

	// RULING 11: fitted gear ("character": "dragoon" | "gnar" | "nayomi") is bound to that body's skeleton. The body
	// comes from the same catalog (body:<character>); an already-imported body is reused (no second download).
	if (Job->Item.Class == TEXT("house_gear") && !Job->Item.Character.IsEmpty() && !Job->Item.Character.StartsWith(TEXT("rig:")))
	{
		const FString BodyId = TEXT("body:") + Job->Item.Character;
		const TSharedPtr<FWVRPSCatalogItem>* Body = Items.FindByPredicate([&BodyId](const TSharedPtr<FWVRPSCatalogItem>& I) { return I->Id == BodyId; });
		if (Body && (*Body)->UrlKind == TEXT("glb"))
		{
			const FString BodyDest = FString::Printf(TEXT("%s/body/%s"), *Root, *Sanitize(BodyId));
			if (USkeleton* Existing = FindSkeletonUnder(BodyDest))
			{
				Job->BodySkeleton = Existing;
				Log(FString::Printf(TEXT("%s: reusing the imported %s skeleton %s"), *Id, *BodyId, *Existing->GetPathName()));
			}
			else
			{
				FWVRPSImportPart B;
				B.Url = (*Body)->Url;
				B.ApiItemId = BodyId;
				B.Role = TEXT("body");
				B.DestPath = BodyDest;
				Job->Parts.Add(B);
			}
			FWVRPSImportPart G;
			G.Url = Job->Item.Url;
			G.ApiItemId = Id;
			G.Role = TEXT("gear");
			G.DestPath = Job->DestRoot;
			Job->Parts.Add(G);
		}
	}
	Jobs.Add(Job);
	Log(FString::Printf(TEXT("Queued %s (%s) -> %s"), *(Job->Item.Name.IsEmpty() ? Id : Job->Item.Name), *Id, *Job->DestRoot));
	PumpQueue();
	return true;
}

FString UWVRPSCreatorsSubsystem::GetImportState(const FString& Id) const
{
	for (int32 i = Jobs.Num() - 1; i >= 0; --i)
	{
		if (Jobs[i]->ItemId == Id)
		{
			return StateName(Jobs[i]->State);
		}
	}
	return TEXT("none");
}

bool UWVRPSCreatorsSubsystem::IsBusy() const
{
	return ActiveJob.IsValid() || Jobs.ContainsByPredicate([](const TSharedPtr<FWVRPSImportJob>& J) { return J->State == EWVRPSJobState::Queued; });
}

void UWVRPSCreatorsSubsystem::PumpQueue()
{
	if (ActiveJob.IsValid())
	{
		return;
	}
	for (const TSharedPtr<FWVRPSImportJob>& J : Jobs)
	{
		if (J->State == EWVRPSJobState::Queued)
		{
			StartJob(J);
			return;
		}
	}
}

void UWVRPSCreatorsSubsystem::FailJob(const TSharedPtr<FWVRPSImportJob>& Job, const FString& Why)
{
	Job->State = EWVRPSJobState::Failed;
	Job->Message = Why;
	Job->Result.Reset();
	Job->Source.Reset();
	Job->KeepPipelines.Reset();
	Log(FString::Printf(TEXT("FAILED %s: %s"), *Job->ItemId, *Why));
	if (ActiveJob == Job)
	{
		ActiveJob.Reset();
	}
	RefreshWhoAmI();   // the ledger may have been charged before the failure: show the real quota
	PumpQueue();
}

void UWVRPSCreatorsSubsystem::StartJob(const TSharedPtr<FWVRPSImportJob>& Job)
{
	ActiveJob = Job;
	Job->StartedAt = FPlatformTime::Seconds();
	Job->State = EWVRPSJobState::Resolving;
	Job->Message = TEXT("Asking the Creator API for the download");
	Job->Progress = 0.02f;
	Bump();
	if (!IsSignedIn())
	{
		FailJob(Job, TEXT("Not signed in. Pair this editor first."));
		return;
	}
	if (Job->Parts.Num() > 0)
	{
		// Pre-planned parts (gear on its body): each part goes through download.php itself.
		Log(FString::Printf(TEXT("%s: %d part(s), %s"), *Job->ItemId, Job->Parts.Num(), Job->BodySkeleton.IsValid() ? TEXT("body already in the project") : TEXT("body first")));
		DownloadPart(Job, 0);
		return;
	}
	const FString Url = ApiUrl(TEXT("/api/creator/download.php?id=")) + FGenericPlatformHttp::UrlEncode(Job->ItemId);
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = MakeRequest(TEXT("GET"), Url, true);
	TWeakObjectPtr<UWVRPSCreatorsSubsystem> Weak(this);
	Req->OnRequestProgress64().BindLambda([Job](FHttpRequestPtr, uint64, uint64 Received)
	{
		if (Job->Item.Bytes > 0 && Job->Item.UrlKind != TEXT("mml"))
		{
			Job->State = EWVRPSJobState::Downloading;
			Job->Progress = FMath::Clamp(0.05f + 0.6f * (float)((double)Received / (double)Job->Item.Bytes), 0.05f, 0.65f);
			Job->Message = FString::Printf(TEXT("Downloading %.1f MB"), Received / (1024.0 * 1024.0));
		}
	});
	Req->OnProcessRequestComplete().BindLambda([Weak, Job](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
	{
		UWVRPSCreatorsSubsystem* Self = Weak.Get();
		if (!Self)
		{
			return;
		}
		if (!bOk || !Resp.IsValid())
		{
			Self->FailJob(Job, FString::Printf(TEXT("Network error talking to %s"), *Self->BaseUrl));
			return;
		}
		const int32 Code = Resp->GetResponseCode();
		if (Code >= 300 && Code < 400 && !Resp->GetHeader(TEXT("Location")).IsEmpty())
		{
			// The HTTP backend did not follow the 302: fetch the public URL ourselves, WITHOUT the bearer.
			const FString Loc = WVRPSMml::ResolveRelative(Resp->GetHeader(TEXT("Location")), Resp->GetURL());
			TSharedRef<IHttpRequest, ESPMode::ThreadSafe> R2 = Self->MakeRequest(TEXT("GET"), Loc, false);
			R2->OnProcessRequestComplete().BindLambda([Weak, Job](FHttpRequestPtr, FHttpResponsePtr Resp2, bool bOk2)
			{
				if (UWVRPSCreatorsSubsystem* S2 = Weak.Get())
				{
					if (!bOk2 || !Resp2.IsValid())
					{
						S2->FailJob(Job, TEXT("Network error fetching the asset."));
						return;
					}
					S2->OnResolved(Job, Resp2);
				}
			});
			R2->ProcessRequest();
			return;
		}
		Self->OnResolved(Job, Resp);
	});
	Req->ProcessRequest();
	Log(FString::Printf(TEXT("Downloading %s (%s)"), *Job->ItemId, *Job->Item.UrlKind));
}

void UWVRPSCreatorsSubsystem::OnResolved(const TSharedPtr<FWVRPSImportJob>& Job, FHttpResponsePtr Resp)
{
	const int32 Code = Resp->GetResponseCode();
	if (FailOnApiError(Job, Code, Resp->GetContentAsString()))
	{
		return;
	}
	const FString Effective = Resp->GetEffectiveURL().IsEmpty() ? Job->Item.Url : Resp->GetEffectiveURL();

	if (Job->Item.UrlKind == TEXT("mml"))
	{
		FString Body;
		TArray<FString> Gear;
		const FString Text = Resp->GetContentAsString();
		if (!WVRPSMml::Parse(Text, Effective, Body, Gear))
		{
			FailJob(Job, TEXT("The MML document has no body GLB (src=...glb). Nothing to import."));
			return;
		}
		FWVRPSImportPart B;
		B.Url = Body;
		B.Role = TEXT("body");
		B.DestPath = Job->DestRoot;
		Job->Parts.Add(B);
		for (int32 i = 0; i < Gear.Num(); ++i)
		{
			FWVRPSImportPart G;
			G.Url = Gear[i];
			G.Role = TEXT("gear");
			G.DestPath = FString::Printf(TEXT("%s/Gear_%02d_%s"), *Job->DestRoot, i + 1, *BaseNameOfUrl(Gear[i]).Left(24));
			Job->Parts.Add(G);
		}
		Log(FString::Printf(TEXT("%s: MML body %s + %d gear (each charged through download.php?url=)"), *Job->ItemId, *Body, Gear.Num()));
		DownloadPart(Job, 0);
		return;
	}

	FWVRPSImportPart P;
	P.Url = Effective;
	P.ApiItemId = Job->ItemId;   // already fetched through download.php?id= above (charged)
	P.ApiRoute = TEXT("id");
	P.Role = Job->Item.IsAudio() ? TEXT("audio") : TEXT("asset");
	P.DestPath = Job->DestRoot;
	Job->Parts.Add(P);
	OnPartBytes(Job, 0, Resp->GetContent(), Resp->GetContentType());
}

bool UWVRPSCreatorsSubsystem::FailOnApiError(const TSharedPtr<FWVRPSImportJob>& Job, int32 Code, const FString& Body)
{
	if (Code == 200)
	{
		return false;
	}
	const TSharedPtr<FJsonObject> J = ParseJson(Body);
	const FString ApiCode = JStr(J, TEXT("code"));
	const FString ApiError = JStr(J, TEXT("error"));
	if (Code == 429 && ApiCode == TEXT("QUOTA_EXCEEDED"))
	{
		const double ResetsAt = JNum(J, TEXT("resets_at"), 0.0);
		const double Needed = JNum(J, TEXT("bytes_needed"), -1.0);
		const double Left = JNum(J, TEXT("remaining_bytes"), -1.0);
		const FString Reset = ResetsAt > 0.0 ? FDateTime::FromUnixTimestamp((int64)ResetsAt).ToString(TEXT("%Y-%m-%d %H:%M UTC")) : FString(TEXT("00:00 UTC"));
		FailJob(Job, FString::Printf(TEXT("Daily download quota (200 MB per key) reached: this asset needs %.1f MB, %.1f MB left today. Resets at %s."),
			Needed / (1024.0 * 1024.0), Left / (1024.0 * 1024.0), *Reset));
		return true;
	}
	if (Code == 429)
	{
		FailJob(Job, FString::Printf(TEXT("Too many requests (%s). Wait a minute and import again. HTTP 429."), ApiCode.IsEmpty() ? TEXT("rate limited") : *ApiCode));
		return true;
	}
	if (Code == 403)
	{
		FailJob(Job, FString::Printf(TEXT("Not available to this key (%s). HTTP 403."), ApiCode.IsEmpty() ? TEXT("holder-only asset, or the key was revoked") : *ApiCode));
		return true;
	}
	if (Code == 401)
	{
		FailJob(Job, TEXT("The creator key was rejected. Pair again."));
		return true;
	}
	if (Code == 503)
	{
		FailJob(Job, ApiCode == TEXT("PAUSED") ? FString(TEXT("The Creator API is paused by WVRPS. Try again later.")) : FString(TEXT("The Creator API is busy. Try again later.")));
		return true;
	}
	FailJob(Job, FString::Printf(TEXT("Download failed: HTTP %d%s%s"), Code, ApiCode.IsEmpty() ? TEXT("") : *(TEXT(" ") + ApiCode), ApiError.IsEmpty() ? TEXT("") : *(TEXT(" - ") + ApiError)));
	return true;
}

void UWVRPSCreatorsSubsystem::OnPartBytes(const TSharedPtr<FWVRPSImportJob>& Job, int32 Index, TArray<uint8> Bytes, const FString& ContentType)
{
	FWVRPSImportPart& P = Job->Parts[Index];
	const bool bAudio = P.Role == TEXT("audio");
	// The magic check runs BEFORE anything touches an importer. The live host's SPA fallback answers a missing file
	// with 200 text/html, so a 200 alone proves nothing.
	if (bAudio ? !HasAudioMagic(Bytes, Job->Item.UrlKind) : !HasGlbMagic(Bytes))
	{
		FailJob(Job, FString::Printf(TEXT("%s is not a %s (no %s magic; got %s, %d bytes). The file is missing at the source (web-page fallback)."),
			*P.Role, bAudio ? *Job->Item.UrlKind.ToUpper() : TEXT("GLB"), bAudio ? (Job->Item.UrlKind == TEXT("wav") ? TEXT("'RIFF/WAVE'") : TEXT("ID3/MPEG")) : TEXT("'glTF'"),
			ContentType.IsEmpty() ? TEXT("unknown type") : *ContentType, Bytes.Num()));
		return;
	}
	const FString Ext = bAudio ? TEXT(".") + Job->Item.UrlKind : FString(TEXT(".glb"));
	// one folder per part: the file name becomes the asset name, so it stays "Dragoon", not "00_body_Dragoon"
	const FString File = Job->Parts.Num() > 1
		? FPaths::Combine(CacheDir, Sanitize(Job->ItemId), FString::Printf(TEXT("%02d_%s"), Index, *P.Role), BaseNameOfUrl(P.Url) + Ext)
		: FPaths::Combine(CacheDir, Sanitize(Job->ItemId), BaseNameOfUrl(P.Url) + Ext);
	Job->State = EWVRPSJobState::Downloading;
	Job->Message = FString::Printf(TEXT("Writing %.1f MB to the cache"), Bytes.Num() / (1024.0 * 1024.0));
	TWeakObjectPtr<UWVRPSCreatorsSubsystem> Weak(this);
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [Weak, Job, Index, File, bAudio, Bytes = MoveTemp(Bytes)]()
	{
		const bool bSaved = FFileHelper::SaveArrayToFile(Bytes, *File);
		AsyncTask(ENamedThreads::GameThread, [Weak, Job, Index, File, bAudio, bSaved]()
		{
			if (UWVRPSCreatorsSubsystem* S = Weak.Get())
			{
				if (!bSaved)
				{
					S->FailJob(Job, TEXT("Could not write the cache file ") + File);
					return;
				}
				Job->Parts[Index].CacheFile = File;
				if (bAudio)
				{
					S->ImportAudioPart(Job, Index);
				}
				else
				{
					S->ImportPart(Job, Index);
				}
			}
		});
	});
}

void UWVRPSCreatorsSubsystem::ImportAudioPart(const TSharedPtr<FWVRPSImportJob>& Job, int32 Index)
{
	// Interchange has no audio translator; the stock USoundFactory (wav, and on Win64 also mp3/ogg/flac via libsndfile)
	// imports through an automated AssetTools task. Synchronous, but a song decodes in well under a second or two.
	FWVRPSImportPart& Part = Job->Parts[Index];
	Job->CurrentPart = Index;
	Job->State = EWVRPSJobState::Importing;
	Job->Message = TEXT("Importing audio");
	Bump();
	UAssetImportTask* Task = NewObject<UAssetImportTask>();
	TStrongObjectPtr<UAssetImportTask> Keep(Task);
	Task->Filename = Part.CacheFile;
	Task->DestinationPath = Part.DestPath;
	Task->bAutomated = true;
	Task->bReplaceExisting = true;
	Task->bSave = true;
	const double T0 = FPlatformTime::Seconds();
	FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().ImportAssetTasks({ Task });
	USoundWave* ImportedWave = nullptr;
	for (UObject* Obj : Task->GetObjects())
	{
		if (!Obj)
		{
			continue;
		}
		Part.Imported.Add(Obj->GetPathName());
		if (USoundWave* W = Cast<USoundWave>(Obj))
		{
			++Part.SoundWaves;
			if (!ImportedWave)
			{
				ImportedWave = W;
			}
		}
	}
	if (Part.SoundWaves == 0)
	{
		FailJob(Job, FString::Printf(TEXT("The audio factory imported no SoundWave from %s (see the Output Log)."), *FPaths::GetCleanFilename(Part.CacheFile)));
		return;
	}
	Log(FString::Printf(TEXT("%s: audio imported %d object(s), %d SoundWave in %.2f s"), *Job->ItemId, Part.Imported.Num(), Part.SoundWaves, FPlatformTime::Seconds() - T0));
	if (bImportWithBlueprint && ImportedWave)
	{
		CreateSoundCueAndBlueprint(Job, ImportedWave);
	}
	StepAfterPart(Job, Index);
}

void UWVRPSCreatorsSubsystem::CreateSoundCueAndBlueprint(const TSharedPtr<FWVRPSImportJob>& Job, USoundWave* Wave)
{
	if (!Wave)
	{
		return;
	}
	const FWVRPSCatalogItem& Item = Job->Item;
	const FString Base = Sanitize(Item.Id.IsEmpty() ? TEXT("sound") : Item.Id);
	const FString PkgPath = Job->DestRoot;
	IAssetTools& AT = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();

	// The wave loops when played raw.
	Wave->bLooping = Item.bLoopable;

	// SoundCue: the playable/placeable content asset, loopable per the manifest.
	USoundCue* Cue = nullptr;
	{
		USoundCueFactoryNew* CueFactory = NewObject<USoundCueFactoryNew>();
		CueFactory->InitialSoundWaves.Add(Wave);
		if (UObject* CueAsset = AT.CreateAsset(Base + TEXT("_Cue"), PkgPath, USoundCue::StaticClass(), CueFactory))
		{
			Cue = Cast<USoundCue>(CueAsset);
		}
	}
	if (Cue && Cue->FirstNode)
	{
		TArray<USoundNode*> Nodes;
		Cue->FirstNode->GetAllNodes(Nodes);
		for (USoundNode* N : Nodes)
		{
			if (USoundNodeWavePlayer* WP = Cast<USoundNodeWavePlayer>(N))
			{
				WP->bLooping = Item.bLoopable;
			}
		}
		Cue->MarkPackageDirty();
	}

	// Blueprint actor: an AActor with an AudioComponent playing the cue (drop into a level).
	UPackage* Pkg = CreatePackage(*FString::Printf(TEXT("%s/%s_BP"), *PkgPath, *Base));
	UBlueprint* BP = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Pkg, FName(*(Base + TEXT("_BP"))), EBlueprintType::BPTYPE_Normal);
	if (BP && BP->SimpleConstructionScript)
	{
		USCS_Node* Node = BP->SimpleConstructionScript->CreateNode(UAudioComponent::StaticClass(), FName(TEXT("Audio")));
		if (Node)
		{
			if (UAudioComponent* Comp = Cast<UAudioComponent>(Node->ComponentTemplate.Get()))
			{
				USoundBase* Sound = Cue ? static_cast<USoundBase*>(Cue) : static_cast<USoundBase*>(Wave);
				Comp->SetSound(Sound);
				Comp->bAutoActivate = true;
			}
			BP->SimpleConstructionScript->AddNode(Node);   // first node -> root
		}
		FKismetEditorUtilities::CompileBlueprint(BP);
		BP->MarkPackageDirty();
	}
	Log(FString::Printf(TEXT("%s: sound + blueprint (%s): SoundWave + SoundCue%s + Blueprint actor"), *Item.Id, *Base, Cue ? TEXT("") : TEXT(" (cue skipped)")));
}

// ------------------------------------------------------------------------------------------------------------------
// Preview (Play): charged download + throwaway import + editor playback. Separate from the import queue.

bool UWVRPSCreatorsSubsystem::PreviewAudio(const FString& Id)
{
	if (!IsSignedIn())
	{
		Log(TEXT("Preview: pair this editor first (Connect tab)."));
		return false;
	}
	const TSharedPtr<FWVRPSCatalogItem>* Found = Items.FindByPredicate([&Id](const TSharedPtr<FWVRPSCatalogItem>& I) { return I->Id == Id; });
	if (!Found || !(*Found)->IsAudio())
	{
		Log(FString::Printf(TEXT("Preview: %s is not an importable audio clip (refresh the catalog)."), *Id));
		return false;
	}
	const bool bLoopable = (*Found)->bLoopable;
	const float DurationSec = (*Found)->DurationSec;
	const FString Ext = (*Found)->UrlKind;   // "wav" | "mp3"

	// Stop whatever is playing and start a fresh charged download (RULING 13: every byte via download.php).
	if (GEditor)
	{
		GEditor->ResetPreviewAudioComponent();
	}
	PreviewWave.Reset();
	PreviewId = Id;
	++PreviewGeneration;
	const int32 Gen = PreviewGeneration;
	PreviewCacheFile = FPaths::Combine(CacheDir, TEXT("_preview"), FString::Printf(TEXT("wvrps_preview.%s"), *Ext));

	const FString Url = ApiUrl(TEXT("/api/creator/download.php?id=")) + FGenericPlatformHttp::UrlEncode(Id);
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = MakeRequest(TEXT("GET"), Url, true);
	TWeakObjectPtr<UWVRPSCreatorsSubsystem> Weak(this);
	Log(FString::Printf(TEXT("Preview: downloading %s (charged to the quota)"), *Id));
	Bump();

	// Validate the audio magic, write to cache off-thread, then import + play on the game thread.
	auto Finalize = [Weak, Id, Gen, Ext, bLoopable, DurationSec](TArray<uint8> Bytes, const FString& ContentType)
	{
		UWVRPSCreatorsSubsystem* Self = Weak.Get();
		if (!Self || Gen != Self->PreviewGeneration)
		{
			return;
		}
		if (!HasAudioMagic(Bytes, Ext))
		{
			Self->PreviewId.Reset();
			Self->Log(FString::Printf(TEXT("Preview: %s is not a %s (no audio magic; got %s, %d bytes). The file is missing at the source."),
				*Id, *Ext.ToUpper(), ContentType.IsEmpty() ? TEXT("unknown type") : *ContentType, Bytes.Num()));
			Self->Bump();
			return;
		}
		const FString File = Self->PreviewCacheFile;
		AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [Weak, Gen, Id, File, Ext, bLoopable, DurationSec, Bytes = MoveTemp(Bytes)]()
		{
			const bool bSaved = FFileHelper::SaveArrayToFile(Bytes, *File);
			AsyncTask(ENamedThreads::GameThread, [Weak, Gen, Id, File, Ext, bLoopable, DurationSec, bSaved]()
			{
				UWVRPSCreatorsSubsystem* S2 = Weak.Get();
				if (!S2 || Gen != S2->PreviewGeneration)
				{
					return;
				}
				if (!bSaved)
				{
					S2->PreviewId.Reset();
					S2->Log(TEXT("Preview: could not write the cache file ") + File);
					S2->Bump();
					return;
				}
				S2->ImportPreviewWave(File, Ext, bLoopable, DurationSec);
			});
		});
	};

	Req->OnProcessRequestComplete().BindLambda([Weak, Gen, Id, Finalize](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
	{
		UWVRPSCreatorsSubsystem* Self = Weak.Get();
		if (!Self || Gen != Self->PreviewGeneration)
		{
			return;
		}
		if (!bOk || !Resp.IsValid())
		{
			Self->PreviewId.Reset();
			Self->Log(FString::Printf(TEXT("Preview: %s download failed (network)."), *Id));
			Self->Bump();
			return;
		}
		const int32 Code = Resp->GetResponseCode();
		if (Code >= 300 && Code < 400 && !Resp->GetHeader(TEXT("Location")).IsEmpty())
		{
			// Backend did not follow the 302: fetch the public URL without the bearer (as DownloadPart does).
			const FString Loc = WVRPSMml::ResolveRelative(Resp->GetHeader(TEXT("Location")), Resp->GetURL());
			TSharedRef<IHttpRequest, ESPMode::ThreadSafe> R2 = Self->MakeRequest(TEXT("GET"), Loc, false);
			R2->OnProcessRequestComplete().BindLambda([Weak, Gen, Id, Finalize](FHttpRequestPtr, FHttpResponsePtr Resp2, bool bOk2)
			{
				UWVRPSCreatorsSubsystem* S2 = Weak.Get();
				if (!S2 || Gen != S2->PreviewGeneration)
				{
					return;
				}
				if (!bOk2 || !Resp2.IsValid() || Resp2->GetResponseCode() != 200)
				{
					S2->PreviewId.Reset();
					S2->Log(FString::Printf(TEXT("Preview: %s download failed (HTTP %d)."), *Id, Resp2.IsValid() ? Resp2->GetResponseCode() : 0));
					S2->Bump();
					return;
				}
				Finalize(Resp2->GetContent(), Resp2->GetContentType());
			});
			if (!R2->ProcessRequest())
			{
				Self->PreviewId.Reset();
				Self->Log(FString::Printf(TEXT("Preview: %s redirect request failed to start."), *Id));
				Self->Bump();
				return;
			}
			return;
		}
		if (Code == 429 || Code == 401 || Code == 403 || Code == 404 || Code >= 500)
		{
			Self->PreviewId.Reset();
			Self->Log(FString::Printf(TEXT("Preview: %s failed (HTTP %d): %s"), *Id, Code, *Resp->GetContentAsString().Left(200)));
			Self->Bump();
			return;
		}
		if (Code != 200)
		{
			Self->PreviewId.Reset();
			Self->Log(FString::Printf(TEXT("Preview: %s download failed (HTTP %d)."), *Id, Code));
			Self->Bump();
			return;
		}
		Finalize(Resp->GetContent(), Resp->GetContentType());
	});
	if (!Req->ProcessRequest())
	{
		PreviewId.Reset();
		Log(FString::Printf(TEXT("Preview: %s request failed to start."), *Id));
		Bump();
		return false;
	}
	return true;
}

void UWVRPSCreatorsSubsystem::ImportPreviewWave(const FString& File, const FString& Ext, bool bLoopable, float DurationSec)
{
	// Throwaway import into a single /Game/WVRPS/_preview working slot, then the editor's own preview playback.
	UAssetImportTask* Task = NewObject<UAssetImportTask>();
	TStrongObjectPtr<UAssetImportTask> Keep(Task);
	Task->Filename = File;
	Task->DestinationPath = TEXT("/Game/WVRPS/_preview");
	Task->bAutomated = true;
	Task->bReplaceExisting = true;
	Task->bSave = false;   // preview only: do not persist into Content
	const double T0 = FPlatformTime::Seconds();
	FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().ImportAssetTasks({ Task });
	USoundWave* Wave = nullptr;
	for (UObject* Obj : Task->GetObjects())
	{
		if (USoundWave* W = Cast<USoundWave>(Obj))
		{
			Wave = W;
			break;
		}
	}
	if (!Wave)
	{
		Log(FString::Printf(TEXT("Preview: the audio factory imported no SoundWave from %s (see the Output Log)."), *FPaths::GetCleanFilename(File)));
		PreviewId.Reset();
		Bump();
		return;
	}
	Wave->bLooping = bLoopable;
	// The automated (bSave=false) import can leave Duration unset (log shows 0.00), which the audio device reads as
	// "nothing to play". The catalog item's duration is authoritative — use it when the import left it unset.
	if (Wave->Duration <= 0.0f && DurationSec > 0.0f)
	{
		Wave->Duration = DurationSec;
	}
	PreviewWave.Reset(Wave);
	const bool bHasDevice = GEngine && GEngine->GetMainAudioDeviceRaw() != nullptr;
	UAudioComponent* Played = nullptr;
	if (GEditor)
	{
		Played = GEditor->PlayPreviewSound(Wave);
	}
	Log(FString::Printf(TEXT("Preview: %s — wave dur %.2fs (item %.2fs), audio device %s, %s"), *PreviewId,
		Wave->Duration, DurationSec, bHasDevice ? TEXT("present") : TEXT("MISSING"),
		Played ? TEXT("playing") : TEXT("NOT STARTED")));
	RefreshWhoAmI();   // the charged download moved the quota; refresh the panel's numbers
	Bump();
}

void UWVRPSCreatorsSubsystem::StopPreview()
{
	if (GEditor)
	{
		GEditor->ResetPreviewAudioComponent();
	}
	PreviewWave.Reset();
	PreviewId.Reset();
	++PreviewGeneration;   // cancel any in-flight preview download
	Log(TEXT("Preview: stopped."));
	Bump();
}

void UWVRPSCreatorsSubsystem::StepAfterPart(const TSharedPtr<FWVRPSImportJob>& Job, int32 Index)
{
	if (Index + 1 < Job->Parts.Num())
	{
		DownloadPart(Job, Index + 1);
		return;
	}
	Job->State = EWVRPSJobState::Done;
	Job->Progress = 1.0f;
	Job->Message = FString::Printf(TEXT("Imported to %s in %.1f s"), *Job->DestRoot, FPlatformTime::Seconds() - Job->StartedAt);
	Log(FString::Printf(TEXT("DONE %s: %s"), *Job->ItemId, *Job->Message));
	ActiveJob.Reset();
	RefreshWhoAmI();   // RULING 13: every part was charged by download.php; pull the ledger's figure into the panel
	PumpQueue();
}

void UWVRPSCreatorsSubsystem::DownloadPart(const TSharedPtr<FWVRPSImportJob>& Job, int32 Index)
{
	Job->CurrentPart = Index;
	Job->State = EWVRPSJobState::Downloading;
	FWVRPSImportPart& Part = Job->Parts[Index];
	Job->Message = FString::Printf(TEXT("Downloading %s %d/%d"), *Part.Role, Index + 1, Job->Parts.Num());
	Bump();
	// RULING 13: EVERY byte goes through download.php and is charged to the key's quota, then 302s to the public
	// file. Catalog parts by id; MML body / gear children (no manifest id) by ?url= (server allowlists the host and
	// charges the file's real Content-Length). Nothing is fetched from a public URL directly any more.
	const bool bViaApi = true;
	Part.ApiRoute = Part.ApiItemId.IsEmpty() ? TEXT("url") : TEXT("id");
	const FString Url = Part.ApiItemId.IsEmpty()
		? ApiUrl(TEXT("/api/creator/download.php?url=")) + FGenericPlatformHttp::UrlEncode(Part.Url)
		: ApiUrl(TEXT("/api/creator/download.php?id=")) + FGenericPlatformHttp::UrlEncode(Part.ApiItemId);
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = MakeRequest(TEXT("GET"), Url, bViaApi);
	TWeakObjectPtr<UWVRPSCreatorsSubsystem> Weak(this);
	const float Base = (float)Index / FMath::Max(1, Job->Parts.Num());
	const float Span = 1.0f / FMath::Max(1, Job->Parts.Num());
	auto Progress = [Job, Base, Span](FHttpRequestPtr, uint64, uint64 Received)
	{
		// No reliable total before the headers land: ease toward 70 % of this part's span (1 - e^-MB).
		const float Frac = 1.0f - FMath::Exp(-(float)(Received / (1024.0 * 1024.0)));
		Job->Progress = FMath::Clamp(Base + Span * 0.7f * Frac, 0.0f, 0.99f);
		Job->Message = FString::Printf(TEXT("Downloading %s %.1f MB"), *Job->Parts[Job->CurrentPart].Role, Received / (1024.0 * 1024.0));
	};
	Req->OnRequestProgress64().BindLambda(Progress);
	Req->OnProcessRequestComplete().BindLambda([Weak, Job, Index, bViaApi, Progress](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
	{
		UWVRPSCreatorsSubsystem* Self = Weak.Get();
		if (!Self)
		{
			return;
		}
		FWVRPSImportPart& P = Job->Parts[Index];
		if (!bOk || !Resp.IsValid())
		{
			Self->FailJob(Job, FString::Printf(TEXT("%s download failed (network): %s"), *P.Role, *P.Url));
			return;
		}
		const int32 Code = Resp->GetResponseCode();
		if (bViaApi && Code >= 300 && Code < 400 && !Resp->GetHeader(TEXT("Location")).IsEmpty())
		{
			// The HTTP backend did not follow the 302: fetch the public URL ourselves, WITHOUT the bearer.
			const FString Loc = WVRPSMml::ResolveRelative(Resp->GetHeader(TEXT("Location")), Resp->GetURL());
			P.Url = Loc;
			TSharedRef<IHttpRequest, ESPMode::ThreadSafe> R2 = Self->MakeRequest(TEXT("GET"), Loc, false);
			R2->OnRequestProgress64().BindLambda(Progress);
			R2->OnProcessRequestComplete().BindLambda([Weak, Job, Index](FHttpRequestPtr, FHttpResponsePtr Resp2, bool bOk2)
			{
				if (UWVRPSCreatorsSubsystem* S2 = Weak.Get())
				{
					FWVRPSImportPart& P2 = Job->Parts[Index];
					if (!bOk2 || !Resp2.IsValid() || Resp2->GetResponseCode() != 200)
					{
						S2->FailJob(Job, FString::Printf(TEXT("%s download failed (HTTP %d): %s"), *P2.Role, Resp2.IsValid() ? Resp2->GetResponseCode() : 0, *P2.Url));
						return;
					}
					S2->OnPartBytes(Job, Index, Resp2->GetContent(), Resp2->GetContentType());
				}
			});
			R2->ProcessRequest();
			return;
		}
		if (bViaApi && Self->FailOnApiError(Job, Code, Resp->GetContentAsString()))
		{
			return;
		}
		if (Code != 200)
		{
			Self->FailJob(Job, FString::Printf(TEXT("%s download failed (HTTP %d): %s"), *P.Role, Code, *P.Url));
			return;
		}
		if (bViaApi && !Resp->GetEffectiveURL().IsEmpty())
		{
			P.Url = Resp->GetEffectiveURL();
		}
		Self->OnPartBytes(Job, Index, Resp->GetContent(), Resp->GetContentType());
	});
	Req->ProcessRequest();
}

TArray<FSoftObjectPath> UWVRPSCreatorsSubsystem::BuildPipelines(const UInterchangeSourceData* Source, USkeleton* Skeleton, FWVRPSImportJob& Job)
{
	// The project's own glTF stack (Project Settings > Interchange), duplicated, with the generic assets pipeline told
	// to bind every skeletal mesh to the body's skeleton. Override pipelines keep their skeleton (IsFromReimportOrOverride).
	TArray<FSoftObjectPath> Out;
	const FInterchangeImportSettings& Settings = FInterchangeProjectSettingsUtils::GetDefaultImportSettings(false);
	const FName StackName = FInterchangeProjectSettingsUtils::GetDefaultPipelineStackName(false, *Source);
	const FInterchangePipelineStack* Stack = Settings.PipelineStacks.Find(StackName);
	if (!Stack)
	{
		Log(FString::Printf(TEXT("No Interchange pipeline stack named %s: gear uses the default stack (skeleton auto-pick by folder)."), *StackName.ToString()));
		return Out;
	}
	const TArray<FSoftObjectPath>* Paths = &Stack->Pipelines;
	for (const FInterchangeTranslatorPipelines& Per : Stack->PerTranslatorPipelines)
	{
		if (Per.Translator.ToSoftObjectPath().ToString().Contains(TEXT("GLTF")))
		{
			Paths = &Per.Pipelines;
			break;
		}
	}
	bool bSet = false;
	for (const FSoftObjectPath& Path : *Paths)
	{
		UInterchangePipelineBase* Asset = Cast<UInterchangePipelineBase>(Path.TryLoad());
		if (!Asset)
		{
			Out.Add(Path); // blueprint / python pipeline assets pass through unchanged
			continue;
		}
		UInterchangePipelineBase* Dup = DuplicateObject<UInterchangePipelineBase>(Asset, GetTransientPackage());
		if (UInterchangeGenericAssetsPipeline* Generic = Cast<UInterchangeGenericAssetsPipeline>(Dup))
		{
			if (Generic->CommonSkeletalMeshesAndAnimationsProperties)
			{
				Generic->CommonSkeletalMeshesAndAnimationsProperties->Skeleton = Skeleton;
				Generic->CommonSkeletalMeshesAndAnimationsProperties->bImportOnlyAnimations = false;
				bSet = true;
			}
		}
		Job.KeepPipelines.Emplace(Dup);
		Out.Add(FSoftObjectPath(Dup));
	}
	if (!bSet)
	{
		Log(TEXT("Warning: no generic assets pipeline in the glTF stack; the skeleton override could not be applied."));
	}
	return Out;
}

void UWVRPSCreatorsSubsystem::ImportPart(const TSharedPtr<FWVRPSImportJob>& Job, int32 Index)
{
	Job->CurrentPart = Index;
	Job->State = EWVRPSJobState::Importing;
	FWVRPSImportPart& Part = Job->Parts[Index];
	Job->Message = FString::Printf(TEXT("Importing %s %d/%d"), *Part.Role, Index + 1, Job->Parts.Num());
	Job->Progress = FMath::Max(Job->Progress, ((float)Index + 0.75f) / FMath::Max(1, Job->Parts.Num()));
	Bump();

	UInterchangeManager& IM = UInterchangeManager::GetInterchangeManager();
	UInterchangeSourceData* Src = UInterchangeManager::CreateSourceData(Part.CacheFile);
	if (!Src || !IM.CanTranslateSourceData(Src))
	{
		FailJob(Job, TEXT("Interchange has no translator for ") + Part.CacheFile);
		return;
	}
	Job->Source.Reset(Src);
	Job->KeepPipelines.Reset();

	FImportAssetParameters Params;
	Params.bIsAutomated = true;       // no modal dialog
	Params.bReplaceExisting = true;
	if (Part.Role == TEXT("gear"))
	{
		USkeleton* Skel = Job->BodySkeleton.Get();
		if (Skel)
		{
			Params.OverridePipelines = BuildPipelines(Src, Skel, *Job);
			Log(FString::Printf(TEXT("%s: gear %d binds to %s"), *Job->ItemId, Index, *Skel->GetPathName()));
		}
		else
		{
			Log(FString::Printf(TEXT("Warning %s: the body produced no skeleton; gear %d imports with its own."), *Job->ItemId, Index));
		}
	}
	Job->Result = IM.ImportAssetAsync(Part.DestPath, Src, Params);
	Log(FString::Printf(TEXT("%s: Interchange import %s -> %s"), *Job->ItemId, *FPaths::GetCleanFilename(Part.CacheFile), *Part.DestPath));
}

bool UWVRPSCreatorsSubsystem::TickImports(float)
{
	if (ActiveJob.IsValid() && ActiveJob->State == EWVRPSJobState::Importing && ActiveJob->Result.IsValid())
	{
		if (ActiveJob->Result->GetStatus() == UE::Interchange::FImportResult::EStatus::Done)
		{
			const TSharedPtr<FWVRPSImportJob> Job = ActiveJob;
			FinishPart(Job, Job->CurrentPart);
		}
	}
	return true;
}

void UWVRPSCreatorsSubsystem::FinishPart(const TSharedPtr<FWVRPSImportJob>& Job, int32 Index)
{
	FWVRPSImportPart& Part = Job->Parts[Index];
	TArray<UPackage*> ToSave;
	USkeleton* FirstSkeleton = nullptr;
	for (UObject* Obj : Job->Result->GetImportedObjects())
	{
		if (!Obj)
		{
			continue;
		}
		Part.Imported.Add(Obj->GetPathName());
		ToSave.AddUnique(Obj->GetOutermost());
		if (USkeletalMesh* SM = Cast<USkeletalMesh>(Obj))
		{
			++Part.SkeletalMeshes;
			if (SM->GetSkeleton())
			{
				Part.BoundSkeleton = SM->GetSkeleton()->GetPathName();
				if (!FirstSkeleton)
				{
					FirstSkeleton = SM->GetSkeleton();
				}
				ToSave.AddUnique(SM->GetSkeleton()->GetOutermost());
			}
		}
		else if (Cast<UStaticMesh>(Obj))
		{
			++Part.StaticMeshes;
		}
		else if (USkeleton* Sk = Cast<USkeleton>(Obj))
		{
			++Part.Skeletons;
			FirstSkeleton = Sk;
		}
	}
	if (UInterchangeResultsContainer* Results = Job->Result->GetResults())
	{
		for (UInterchangeResult* R : Results->GetResults())
		{
			if (R && R->GetResultType() == EInterchangeResultType::Error)
			{
				Log(FString::Printf(TEXT("  Interchange error: %s"), *R->GetText().ToString()));
			}
		}
	}
	Job->Result.Reset();
	Job->Source.Reset();
	Job->KeepPipelines.Reset();

	if (Part.Imported.Num() == 0)
	{
		FailJob(Job, FString::Printf(TEXT("Interchange imported nothing from %s (see the Output Log)."), *FPaths::GetCleanFilename(Part.CacheFile)));
		return;
	}
	ToSave.RemoveAll([](UPackage* P) { return P == nullptr || P == GetTransientPackage(); });
	UEditorLoadingAndSavingUtils::SavePackages(ToSave, true);

	if (Part.Role == TEXT("body"))
	{
		Job->BodySkeleton = FirstSkeleton;
	}
	FString Note;
	if (Part.Role == TEXT("gear") && Job->BodySkeleton.IsValid())
	{
		const bool bBound = Part.BoundSkeleton == Job->BodySkeleton->GetPathName();
		Note = bBound ? TEXT(" (bound to the body skeleton)") : FString::Printf(TEXT(" (WARNING: references %s, not the body skeleton)"), *Part.BoundSkeleton);
	}
	Log(FString::Printf(TEXT("%s: %s imported %d object(s): %d skeletal mesh, %d static mesh, %d new skeleton%s"),
		*Job->ItemId, *Part.Role, Part.Imported.Num(), Part.SkeletalMeshes, Part.StaticMeshes, Part.Skeletons, *Note));

	StepAfterPart(Job, Index);
}

// ------------------------------------------------------------------------------------------------------------------
// Status JSON, panel helpers

FString UWVRPSCreatorsSubsystem::GetStatusJson() const
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("version"), PluginVersion());
	Root->SetStringField(TEXT("base_url"), BaseUrl);
	Root->SetStringField(TEXT("pair_state"), PairState);
	Root->SetStringField(TEXT("user_code"), UserCode);
	Root->SetBoolField(TEXT("signed_in"), IsSignedIn());
	Root->SetStringField(TEXT("who_label"), WhoLabel);
	Root->SetStringField(TEXT("who_tier"), WhoTier);
	Root->SetStringField(TEXT("catalog_state"), CatalogState);
	Root->SetNumberField(TEXT("catalog_count"), Items.Num());
	Root->SetStringField(TEXT("manifest_generated"), ManifestGenerated);
	Root->SetStringField(TEXT("cache_dir"), CacheDir);
	Root->SetStringField(TEXT("license"), GetLicense());
	Root->SetNumberField(TEXT("quota_limit_bytes"), (double)QuotaLimit);
	Root->SetNumberField(TEXT("quota_used_bytes"), (double)QuotaUsed);
	Root->SetNumberField(TEXT("quota_remaining_bytes"), (double)QuotaRemaining);
	Root->SetNumberField(TEXT("catalog_load_seconds"), CatalogLoadSeconds);
	Root->SetNumberField(TEXT("catalog_parse_seconds"), CatalogParseSeconds);
	{
		TSharedRef<FJsonObject> T = MakeShared<FJsonObject>();
		T->SetNumberField(TEXT("http_ok"), ThumbHttpOk);
		T->SetNumberField(TEXT("disk_hit"), ThumbDiskHits);
		T->SetNumberField(TEXT("unsupported"), ThumbUnsupported);
		T->SetNumberField(TEXT("none"), ThumbNone);
		T->SetNumberField(TEXT("failed"), ThumbFailed);
		Root->SetObjectField(TEXT("thumbs"), T);
	}
	TArray<TSharedPtr<FJsonValue>> JobsJson;
	for (const TSharedPtr<FWVRPSImportJob>& J : Jobs)
	{
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("id"), J->ItemId);
		O->SetStringField(TEXT("state"), StateName(J->State));
		O->SetStringField(TEXT("message"), J->Message);
		O->SetStringField(TEXT("dest"), J->DestRoot);
		O->SetStringField(TEXT("body_skeleton"), J->BodySkeleton.IsValid() ? J->BodySkeleton->GetPathName() : FString());
		TArray<TSharedPtr<FJsonValue>> PartsJson;
		for (const FWVRPSImportPart& P : J->Parts)
		{
			TSharedRef<FJsonObject> PO = MakeShared<FJsonObject>();
			PO->SetStringField(TEXT("role"), P.Role);
			PO->SetStringField(TEXT("url"), P.Url);
			PO->SetStringField(TEXT("cache_file"), P.CacheFile);
			PO->SetStringField(TEXT("dest"), P.DestPath);
			PO->SetNumberField(TEXT("skeletal_meshes"), P.SkeletalMeshes);
			PO->SetNumberField(TEXT("static_meshes"), P.StaticMeshes);
			PO->SetNumberField(TEXT("new_skeletons"), P.Skeletons);
			PO->SetNumberField(TEXT("sound_waves"), P.SoundWaves);
			PO->SetStringField(TEXT("api_item"), P.ApiItemId);
			PO->SetStringField(TEXT("api_route"), P.ApiRoute);
			PO->SetStringField(TEXT("bound_skeleton"), P.BoundSkeleton);
			PO->SetNumberField(TEXT("imported_objects"), P.Imported.Num());
			PartsJson.Add(MakeShared<FJsonValueObject>(PO));
		}
		O->SetArrayField(TEXT("parts"), PartsJson);
		JobsJson.Add(MakeShared<FJsonValueObject>(O));
	}
	Root->SetArrayField(TEXT("jobs"), JobsJson);
	FString Out;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> W = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
	FJsonSerializer::Serialize(Root, W);
	return Out;
}

TSharedRef<IHttpRequest, ESPMode::ThreadSafe> UWVRPSCreatorsSubsystem::MakeThumbRequest(const FString& Id, int32 Size) const
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = MakeRequest(TEXT("GET"),
		ApiUrl(TEXT("/api/creator/thumb.php?id=")) + FGenericPlatformHttp::UrlEncode(Id) + FString::Printf(TEXT("&s=%d"), Size), true);
	Req->SetHeader(TEXT("Accept"), TEXT("image/png"));
	Req->SetTimeout(30.0f);
	return Req;
}

FString UWVRPSCreatorsSubsystem::GetThumbCacheDir() const
{
	return FPaths::Combine(CacheDir, TEXT("_thumbs"));
}

void UWVRPSCreatorsSubsystem::NoteThumb(const FString& Outcome)
{
	if (Outcome == TEXT("http_ok")) ++ThumbHttpOk;
	else if (Outcome == TEXT("disk_hit")) ++ThumbDiskHits;
	else if (Outcome == TEXT("unsupported")) ++ThumbUnsupported;
	else if (Outcome == TEXT("none")) ++ThumbNone;
	else ++ThumbFailed;
	const int32 Shown = ThumbHttpOk + ThumbDiskHits;
	const bool bShownMilestone = (Outcome == TEXT("http_ok") || Outcome == TEXT("disk_hit")) && (Shown == 1 || Shown == 25 || Shown == 100 || Shown % 500 == 0);
	const bool bFirstProblem = (Outcome == TEXT("unsupported") && ThumbUnsupported == 1) || (Outcome == TEXT("failed") && ThumbFailed == 1)
		|| (Outcome == TEXT("none") && ThumbNone == 25 && Shown == 0);   // e.g. a server without thumb.php (404 for every tile)
	if (bShownMilestone || bFirstProblem)
	{
		Log(FString::Printf(TEXT("Thumbnails: %d shown (%d from thumb.php, %d from the disk cache), %d unsupported by the server, %d failed."),
			Shown, ThumbHttpOk, ThumbDiskHits, ThumbUnsupported, ThumbFailed));
	}
}

void UWVRPSCreatorsSubsystem::OpenPanel(int32 TabIndex)
{
	RequestedTab = TabIndex;
	FGlobalTabmanager::Get()->TryInvokeTab(FTabId(WVRPSCreatorsTab::TabName));
	Bump();
}

void UWVRPSCreatorsSubsystem::ClosePanel()
{
	if (TSharedPtr<SDockTab> Tab = FGlobalTabmanager::Get()->FindExistingLiveTab(FTabId(WVRPSCreatorsTab::TabName)))
	{
		Tab->RequestCloseTab();
	}
}

void UWVRPSCreatorsSubsystem::SetPanelEnvironmentState(int32 BrowseMode, const FString& Biome, const FString& Group, const FString& Category)
{
	RequestedBrowseMode = BrowseMode;
	RequestedBiome = Biome;
	RequestedGroup = Group;
	RequestedCategory = Category;
	Bump();
}

bool UWVRPSCreatorsSubsystem::CapturePanelScreenshot(const FString& PngPath)
{
	const TSharedPtr<SWidget> Panel = WVRPSCreatorsTab::GetLivePanel();
	if (!Panel.IsValid() || !FSlateApplication::IsInitialized())
	{
		Log(TEXT("Screenshot: the panel is not open."));
		return false;
	}
	TArray<FColor> Pixels;
	FIntVector Size;
	if (!FSlateApplication::Get().TakeScreenshot(Panel.ToSharedRef(), Pixels, Size) || Size.X <= 0 || Size.Y <= 0)
	{
		Log(TEXT("Screenshot: Slate could not capture the panel."));
		return false;
	}
	for (FColor& C : Pixels)
	{
		C.A = 255;
	}
	IImageWrapperModule& IW = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	TSharedPtr<IImageWrapper> Png = IW.CreateImageWrapper(EImageFormat::PNG);
	if (!Png.IsValid() || !Png->SetRaw(Pixels.GetData(), Pixels.Num() * sizeof(FColor), Size.X, Size.Y, ERGBFormat::BGRA, 8))
	{
		return false;
	}
	const TArray64<uint8> Data = Png->GetCompressed();
	const bool bOk = FFileHelper::SaveArrayToFile(Data, *PngPath);
	Log(FString::Printf(TEXT("Screenshot %dx%d -> %s (%s)"), Size.X, Size.Y, *PngPath, bOk ? TEXT("ok") : TEXT("write failed")));
	return bOk;
}
