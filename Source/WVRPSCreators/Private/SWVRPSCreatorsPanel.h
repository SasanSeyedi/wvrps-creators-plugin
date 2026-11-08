// WVRPS Creators panel: Connect / Browse / Import / About. Slate only; every action calls UWVRPSCreatorsSubsystem.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "WVRPSCreatorsSubsystem.h"

struct FSlateDynamicImageBrush;
struct FSlateRoundedBoxBrush;
class IImageWrapperModule;
template <typename ItemType> class STileView;
template <typename ItemType> class SListView;

class SWVRPSCreatorsPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWVRPSCreatorsPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SWVRPSCreatorsPanel() override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	/** Enter on the Browse grid imports the selected tile (key events bubble up from the focused tile view). */
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	void SetActiveTab(int32 Index) { ActiveTab = FMath::Clamp(Index, 0, 3); }

private:
	using FItemPtr = TSharedPtr<FWVRPSCatalogItem>;
	using FJobPtr = TSharedPtr<FWVRPSImportJob>;

	UWVRPSCreatorsSubsystem* Sub() const { return UWVRPSCreatorsSubsystem::Get(); }

	TSharedRef<SWidget> BuildMasthead();
	TSharedRef<SWidget> BuildConnect();
	TSharedRef<SWidget> BuildBrowse();
	TSharedRef<SWidget> BuildImport();
	TSharedRef<SWidget> BuildAbout();
	TSharedRef<SWidget> BuildFooter();

	// Environments drill-down (Browse mode 1)
	TSharedRef<SWidget> BuildAssetsBrowse();
	TSharedRef<SWidget> BuildEnvironmentsBrowse();
	TSharedRef<SWidget> BuildEnvironmentNavigation();
	TSharedRef<SWidget> BuildBiomeGrid();
	TSharedRef<SWidget> BuildGroupGrid();
	TSharedRef<SWidget> BuildCategoryGrid();

	TSharedRef<SWidget> Card(const FText& Step, const FText& Title, TSharedRef<SWidget> Body);
	TSharedRef<SWidget> Pill(const TAttribute<FText>& Text, const TAttribute<const FSlateBrush*>& PillBrush, const TAttribute<FSlateColor>& Color);
	TSharedRef<SWidget> IconLabel(const char* Icon, const FText& Label, const FSlateFontInfo& Font);

	TSharedRef<ITableRow> OnGenerateTile(FItemPtr Item, const TSharedRef<STableViewBase>& Owner);
	TSharedRef<ITableRow> OnGenerateClipTile(FItemPtr Item, const TSharedRef<STableViewBase>& Owner);
	TSharedRef<ITableRow> OnGenerateBiomeTile(TSharedPtr<FWVRPSBiome> Biome, const TSharedRef<STableViewBase>& Owner);
	TSharedRef<ITableRow> OnGenerateGroupTile(TSharedPtr<FString> Group, const TSharedRef<STableViewBase>& Owner);
	TSharedRef<ITableRow> OnGenerateCategoryTile(TSharedPtr<FString> Slug, const TSharedRef<STableViewBase>& Owner);
	TSharedRef<ITableRow> OnGenerateJobRow(FJobPtr Job, const TSharedRef<STableViewBase>& Owner);
	void RebuildFilter();
	void SyncFromSubsystem();

	void NavigateToBiome(const FString& BiomeId);
	void NavigateToGroup(const FString& Group);
	void NavigateToCategory(const FString& Slug);
	void BackEnvironment();
	void PlayPreview(const FItemPtr& Item);
	const FSlateBrush* CategoryIconFor(const FString& Slug) const;
	const FSlateBrush* GroupIconFor(const FString& Group) const;
	const FSlateBrush* BiomeThumbFor(const FString& BiomeId) const;
	void LoadEnvironmentIcons();
	void RequestBiomeThumb(const FString& BiomeId, const FString& Url);

	/** Tile IMPORT pill, double-click, Enter and the selection bar all land here: queue + "Queued <name>" toast/log at once. */
	void QueueImport(const FItemPtr& Item);
	void Toast(const FText& Text, bool bSuccess);
	FItemPtr SelectedItem() const;

	// Thumbnails: api/creator/thumb.php (server converts webp -> PNG), <= MaxThumbsInFlight requests, PNG disk cache.
	void RequestThumb(const FItemPtr& Item);
	void PumpThumbs();
	void StartThumbHttp(const FItemPtr& Item, const FString& CacheFile);
	FString ThumbCacheFile(const FItemPtr& Item) const;
	void OnThumbDecoded(const FString& Id, int32 W, int32 H, TArray<uint8>&& BGRA);
	void OnThumbFailed(const FString& Id, const FString& Outcome);
	const FSlateBrush* ThumbFor(const FString& Id) const;

	const FSlateBrush* TierBrush(const FString& Tier) const;
	FSlateColor TierColor(const FString& Tier) const;
	static FText ClassLabel(const FString& Class);
	static FString BiomeDisplayName(const TArray<FWVRPSBiome>& Biomes, const FString& Id);
	static FString CategoryDisplayLabel(const TArray<FWVRPSCategory>& Categories, const FString& Slug);
	static FString HumanBytes(int64 Bytes);
	FText StatusText() const;

	int32 ActiveTab = 0;
	uint32 SeenRevision = 0;
	int32 SeenItemCount = -1;
	FString SeenGenerated;
	FString SeenPreviewId;

	FString ClassFilter;      // "" = all
	FString SearchText;
	FString DestRoot = TEXT("/Game/WVRPS");
	TArray<FItemPtr> Filtered;
	TArray<FJobPtr> JobRows;
	TArray<TSharedPtr<FString>> ClassOptions;

	// M10 Environments browse mode (second Browse mode): 3-level drill-down
	int32 BrowseMode = 0;        // 0 = Assets, 1 = Environments
	int32 EnvDepth = 0;          // 0 biome grid, 1 group split, 2 category list, 3 clips
	FString SelectedBiome;       // biome slug
	FString SelectedGroup;       // "music" | "sfx"
	FString SelectedCategory;    // category slug

	TArray<TSharedPtr<FWVRPSBiome>> BiomeRows;   // level 0 grid source
	TArray<TSharedPtr<FString>> GroupOptions;    // level 1 grid source ("music", "sfx")
	TArray<TSharedPtr<FString>> CategoryList;    // level 2 grid source (slugs of the selected group)
	TSharedPtr<STileView<TSharedPtr<FWVRPSBiome>>> BiomeView;
	TSharedPtr<STileView<TSharedPtr<FString>>> GroupView;
	TSharedPtr<STileView<TSharedPtr<FString>>> CategoryView;
	TSharedPtr<STileView<FItemPtr>> ClipView;   // Environments depth-3 clip tiles

	TMap<FString, TSharedPtr<FSlateDynamicImageBrush>> EnvIcons;    // category slug + group name -> decoded PNG brush
	TMap<FString, TSharedPtr<FSlateDynamicImageBrush>> BiomeThumbs;  // biome id -> decoded art (direct URL)
	TSet<FString> PendingBiomeThumbs;

	TSharedPtr<STileView<FItemPtr>> TileView;
	TSharedPtr<SListView<FJobPtr>> JobList;

	TMap<FString, TSharedPtr<FSlateDynamicImageBrush>> Thumbs;
	TSet<FString> PendingThumbs;
	TSet<FString> FailedThumbs;          // 404 / 415 / no source: keep the class placeholder, never re-ask this session
	TArray<TPair<FItemPtr, FString>> ThumbQueue;   // (item, disk cache file); served newest-first (the tiles on screen)
	int32 ThumbsInFlight = 0;
	double ThumbPauseUntil = 0.0;        // after a 429: back off
	static constexpr int32 MaxThumbsInFlight = 6;
	static constexpr int32 ThumbSize = 256;
	uint32 BrushCounter = 0;
	IImageWrapperModule* ImageWrapper = nullptr;

	FButtonStyle AccentButton;
	FProgressBarStyle ProgressStyle;   // WVRPS teal ACT button (house-tokens --h-accent), dark label
	TUniquePtr<FSlateRoundedBoxBrush> CardBrush, MastBrush, TileBrush, CodeBrush, WellBrush;
	TUniquePtr<FSlateRoundedBoxBrush> PillAccent, PillValue, PillAmber, PillGray, PillRec, LicenceBrush;
};
