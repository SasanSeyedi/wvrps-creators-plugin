// WVRPS Creators: the editor subsystem that owns pairing, the creator token, the catalog and the import queue.
// The Slate panel and editor Python (golden test) drive this same object, so the test exercises the real code.
// Law (inherited from WVRPSTools): nothing blocks the game thread. HTTP is async (callbacks on the game thread),
// thumbnail decode and cache writes run on background tasks, imports go through UInterchangeManager::ImportAssetAsync
// and are polled from a core ticker.
#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Containers/Ticker.h"
#include "Interfaces/IHttpRequest.h"
#include "UObject/StrongObjectPtr.h"
#include "WVRPSCreatorsSubsystem.generated.h"

class USkeleton;
class UInterchangeSourceData;
class UInterchangePipelineBase;
class FJsonObject;
namespace UE { namespace Interchange { class FImportResult; } }

/** One row of the creators manifest (catalog.php items[]). */
struct FWVRPSCatalogItem
{
	FString Id;
	FString Class;      // vault_nft | house_gear | ship_room | chess | printables | voxel_lego | music | body
	FString Name;
	FString Url;        // the first of glb_url | mml_url | voxel_raw_url | mp3_url | wav_url that is set, else `url`
	FString UrlKind;    // "glb" | "mml" | "voxel" | "mp3" | "wav" | "json" (brick kit, not importable) | ""
	FString Character;  // house_gear: the body this gear was fitted/bound to ("dragoon" | "gnar" | "nayomi" | "rig:<id>")
	FString Variant;
	FString ThumbUrl;
	FString ThumbHqUrl;  // vault rows: the large PNG (thumb.php falls back to it server-side)
	FString TokenId;     // token rows (vault / brick kit / song): thumb.php can use the token's own art
	int64 Bytes = -1;
	FString Tier;       // public | holder
	FString License;
	FString Source;

	bool IsImportable() const { return UrlKind == TEXT("glb") || UrlKind == TEXT("mml") || UrlKind == TEXT("voxel") || IsAudio(); }
	bool IsAudio() const { return UrlKind == TEXT("mp3") || UrlKind == TEXT("wav"); }
	/** thumb.php has something to convert (thumb_url, thumb_hq_url or a token id). Bodies / rooms / chess have none. */
	bool HasThumbSource() const { return !ThumbUrl.IsEmpty() || !ThumbHqUrl.IsEmpty() || !TokenId.IsEmpty(); }
	/** Why the IMPORT action is greyed out (empty when importable). Shown as the tooltip and in the toast. */
	FString NotImportableReason() const
	{
		if (IsImportable()) return FString();
		if (UrlKind == TEXT("json")) return TEXT("Brick kit: a JSON build file (brick-by-brick instructions), not a GLB mesh. Unreal cannot import it; use the 3D model of the same token (Vault NFTs) instead.");
		if (UrlKind.IsEmpty()) return TEXT("This item has no downloadable file yet.");
		return FString::Printf(TEXT("A .%s file: listed for reference, this plugin imports GLB models and wav/mp3 audio only."), *UrlKind);
	}
};

/** One downloadable file of an import job: the item's GLB, or an MML body / gear child. */
struct FWVRPSImportPart
{
	FString Url;
	FString Role;       // "asset" | "body" | "gear" | "audio"
	FString ApiItemId;  // set: fetch through creator/download.php?id= (charged to the quota), then follow its 302
	                    // empty (MML body / gear children): fetched through download.php?url=<Url> (also charged)
	FString ApiRoute;   // "id" | "url": how this part was charged (status JSON)
	int32 SoundWaves = 0;
	FString CacheFile;  // absolute path once written
	FString DestPath;   // /Game/... package path for this part
	int32 StaticMeshes = 0;
	int32 SkeletalMeshes = 0;
	int32 Skeletons = 0;
	FString BoundSkeleton; // gear: the skeleton the imported SkeletalMesh actually references
	TArray<FString> Imported;
};

enum class EWVRPSJobState : uint8 { Queued, Resolving, Downloading, Importing, Done, Failed };

struct FWVRPSImportJob
{
	FString ItemId;
	FWVRPSCatalogItem Item;
	FString DestRoot;   // /Game/WVRPS/<class>/<id>
	EWVRPSJobState State = EWVRPSJobState::Queued;
	FString Message;
	float Progress = 0.0f;
	TArray<FWVRPSImportPart> Parts;
	int32 CurrentPart = -1;
	TWeakObjectPtr<USkeleton> BodySkeleton;
	double StartedAt = 0.0;

	// In-flight import state (game thread only)
	TSharedPtr<UE::Interchange::FImportResult, ESPMode::ThreadSafe> Result;
	// UObject-typed so this header never needs the complete Interchange types.
	TStrongObjectPtr<UObject> Source;
	TArray<TStrongObjectPtr<UObject>> KeepPipelines;
};

DECLARE_MULTICAST_DELEGATE(FOnWVRPSCreatorsChanged);

UCLASS()
class WVRPSCREATORS_API UWVRPSCreatorsSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	static UWVRPSCreatorsSubsystem* Get();

	// ---- Connect ------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "WVRPS Creators")
	void SetBaseUrl(const FString& InBaseUrl);
	UFUNCTION(BlueprintPure, Category = "WVRPS Creators")
	FString GetBaseUrl() const { return BaseUrl; }

	/** grant_start -> shows the 8-digit code; opens <base>/creators?code=<code> when bOpenBrowser; then polls. */
	UFUNCTION(BlueprintCallable, Category = "WVRPS Creators")
	void StartPairing(bool bOpenBrowser = true);
	UFUNCTION(BlueprintCallable, Category = "WVRPS Creators")
	void CancelPairing();
	/** idle | starting | awaiting_approval | approved | expired | error */
	UFUNCTION(BlueprintPure, Category = "WVRPS Creators")
	FString GetPairState() const { return PairState; }
	UFUNCTION(BlueprintPure, Category = "WVRPS Creators")
	FString GetUserCode() const { return UserCode; }
	UFUNCTION(BlueprintPure, Category = "WVRPS Creators")
	FString GetApproveUrl() const;
	UFUNCTION(BlueprintPure, Category = "WVRPS Creators")
	bool IsSignedIn() const { return !Token.IsEmpty(); }
	UFUNCTION(BlueprintCallable, Category = "WVRPS Creators")
	void SignOut();
	UFUNCTION(BlueprintCallable, Category = "WVRPS Creators")
	void RefreshWhoAmI();

	// ---- Browse -------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "WVRPS Creators")
	void RefreshCatalog();
	/** idle | loading | ready | error */
	UFUNCTION(BlueprintPure, Category = "WVRPS Creators")
	FString GetCatalogState() const { return CatalogState; }
	UFUNCTION(BlueprintPure, Category = "WVRPS Creators")
	int32 GetCatalogCount() const { return Items.Num(); }
	UFUNCTION(BlueprintPure, Category = "WVRPS Creators")
	FString GetManifestGenerated() const { return ManifestGenerated; }
	/** Seconds the last catalog request took (request sent -> items parsed), and the parse share of it. */
	UFUNCTION(BlueprintPure, Category = "WVRPS Creators")
	float GetCatalogLoadSeconds() const { return CatalogLoadSeconds; }
	UFUNCTION(BlueprintPure, Category = "WVRPS Creators")
	float GetCatalogParseSeconds() const { return CatalogParseSeconds; }

	// ---- Import -------------------------------------------------------------------------------------------
	/** Queue an import of a catalog item. DestRoot defaults to /Game/WVRPS. Returns false if unknown / not importable. */
	UFUNCTION(BlueprintCallable, Category = "WVRPS Creators")
	bool ImportItem(const FString& Id, const FString& DestRoot = TEXT(""));
	/** queued | resolving | downloading | importing | done | failed | none */
	UFUNCTION(BlueprintPure, Category = "WVRPS Creators")
	FString GetImportState(const FString& Id) const;
	UFUNCTION(BlueprintPure, Category = "WVRPS Creators")
	bool IsBusy() const;
	UFUNCTION(BlueprintCallable, Category = "WVRPS Creators")
	void SetCacheDir(const FString& Dir);
	UFUNCTION(BlueprintPure, Category = "WVRPS Creators")
	FString GetCacheDir() const { return CacheDir; }

	/** Everything above as one JSON document (pair state, whoami, catalog summary, every job with per-part counts). */
	UFUNCTION(BlueprintCallable, Category = "WVRPS Creators")
	FString GetStatusJson() const;
	UFUNCTION(BlueprintCallable, Category = "WVRPS Creators")
	FString GetLogText() const;

	// ---- Panel helpers ------------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "WVRPS Creators")
	void OpenPanel(int32 TabIndex = -1);
	UFUNCTION(BlueprintCallable, Category = "WVRPS Creators")
	void ClosePanel();
	/** Window capture of the panel's host window to PNG (Slate TakeScreenshot). */
	UFUNCTION(BlueprintCallable, Category = "WVRPS Creators")
	bool CapturePanelScreenshot(const FString& PngPath);

	// ---- Thumbnails (api/creator/thumb.php: server-side webp/png/jpg -> PNG, not charged) ------------------
	/** GET <base>/api/creator/thumb.php?id=<id>&s=<size> with the bearer. The panel owns concurrency + disk cache. */
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> MakeThumbRequest(const FString& Id, int32 Size) const;
	/** <cache dir>/_thumbs: decoded-ready PNGs from thumb.php, one per item (keyed by id + source URLs). */
	FString GetThumbCacheDir() const;
	/** Panel -> subsystem counters for the status JSON: "http_ok" | "disk_hit" | "unsupported" | "none" | "failed". */
	void NoteThumb(const FString& Outcome);
	UFUNCTION(BlueprintPure, Category = "WVRPS Creators")
	int32 GetThumbsShown() const { return ThumbHttpOk + ThumbDiskHits; }

	// C++-side accessors for the panel
	const TArray<TSharedPtr<FWVRPSCatalogItem>>& GetItems() const { return Items; }
	const TArray<TSharedPtr<FWVRPSImportJob>>& GetJobs() const { return Jobs; }
	FString GetWhoLabel() const { return WhoLabel; }
	FString GetWhoWallet() const { return WhoWallet; }
	FString GetWhoTier() const { return WhoTier; }
	FString GetLicense() const;
	double GetTokenExp() const { return TokenExp; }
	double GetPairExpiresAt() const { return PairExpiresAt; }
	FString GetLastError() const { return LastError; }
	uint32 GetRevision() const { return Revision; }
	int32 GetRequestedTab() const { return RequestedTab; }
	void ClearRequestedTab() { RequestedTab = -1; }
	FOnWVRPSCreatorsChanged& OnChanged() { return Changed; }
	void Log(const FString& Line);

	static FString PluginVersion();
	static const TCHAR* LicenseLine();

private:
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> MakeRequest(const FString& Verb, const FString& Url, bool bAuth) const;
	FString ApiUrl(const TCHAR* Path) const;
	void SaveSettings();
	void LoadSettings();
	void Bump();

	void PollGrant();
	bool TickPoll(float Dt);
	void HandleCatalogJson(const TSharedPtr<FJsonObject>& Root);

	// import pipeline
	void PumpQueue();
	void StartJob(const TSharedPtr<FWVRPSImportJob>& Job);
	void FailJob(const TSharedPtr<FWVRPSImportJob>& Job, const FString& Why);
	void OnResolved(const TSharedPtr<FWVRPSImportJob>& Job, FHttpResponsePtr Resp);
	void DownloadPart(const TSharedPtr<FWVRPSImportJob>& Job, int32 Index);
	void ImportPart(const TSharedPtr<FWVRPSImportJob>& Job, int32 Index);
	void FinishPart(const TSharedPtr<FWVRPSImportJob>& Job, int32 Index);
	/** 429/403/401/404/5xx -> FailJob with the API's own words. Returns true when the job was failed. */
	bool FailOnApiError(const TSharedPtr<FWVRPSImportJob>& Job, int32 Code, const FString& Body);
	/** wav/mp3 -> USoundWave through the stock audio factory (AssetTools import task, automated). */
	void ImportAudioPart(const TSharedPtr<FWVRPSImportJob>& Job, int32 Index);
	/** After a part's bytes are on disk: GLB -> Interchange, audio -> sound factory. */
	void OnPartBytes(const TSharedPtr<FWVRPSImportJob>& Job, int32 Index, TArray<uint8> Bytes, const FString& ContentType);
	void StepAfterPart(const TSharedPtr<FWVRPSImportJob>& Job, int32 Index);
	bool TickImports(float Dt);
	TArray<FSoftObjectPath> BuildPipelines(const UInterchangeSourceData* Source, USkeleton* Skeleton, FWVRPSImportJob& Job);

	FString BaseUrl = TEXT("https://wvrps.com");
	FString Token;
	double TokenExp = 0.0;
	FString CacheDir;

	FString PairState = TEXT("idle");
	FString GrantId, ClaimSecret, UserCode;
	double PairExpiresAt = 0.0;
	double NextPollAt = 0.0;
	bool bPollInFlight = false;
	FTSTicker::FDelegateHandle PollTicker;

	FString WhoLabel, WhoWallet, WhoTier, WhoLicense;
	int64 QuotaLimit = -1, QuotaUsed = -1, QuotaRemaining = -1;
	FString CatalogState = TEXT("idle");
	FString CatalogETag, ManifestGenerated, ManifestLicense;
	double CatalogRequestedAt = 0.0;
	float CatalogLoadSeconds = 0.0f, CatalogParseSeconds = 0.0f;
	FString LastError;
	TArray<TSharedPtr<FWVRPSCatalogItem>> Items;

	TArray<TSharedPtr<FWVRPSImportJob>> Jobs;
	TSharedPtr<FWVRPSImportJob> ActiveJob;
	FTSTicker::FDelegateHandle ImportTicker;

	int32 ThumbHttpOk = 0, ThumbDiskHits = 0, ThumbUnsupported = 0, ThumbNone = 0, ThumbFailed = 0;
	FString LastQuotaLine;

	TArray<FString> LogLines;
	uint32 Revision = 1;
	int32 RequestedTab = -1;
	FOnWVRPSCreatorsChanged Changed;
};
