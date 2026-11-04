// WVRPS Creators panel. Look = WVRPSTools v3-beautify base (spacing/type tokens, brush-with-fallback, PrimaryButton
// once per region, pills with an opaque dark fill and a 1 px coloured outline) recoloured in the WVRPS house palette.
// Behaviour lives in UWVRPSCreatorsSubsystem; this file only draws state and forwards clicks.
// Nothing here blocks the game thread: thumbnails arrive over async HTTP and decode on a background task.
#include "SWVRPSCreatorsPanel.h"

#include "WVRPSCreatorsStyle.h"

#include "Async/Async.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Framework/Notifications/NotificationManager.h"
#include "InputCoreTypes.h"
#include "Misc/FileHelper.h"
#include "Misc/SecureHash.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "ContentBrowserModule.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"
#include "HttpModule.h"
#include "IContentBrowserSingleton.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STileView.h"

#define LOCTEXT_NAMESPACE "SWVRPSCreatorsPanel"

using namespace WVRPSCreatorsUi;

namespace
{
	const TCHAR* GClasses[] = { TEXT("vault_nft"), TEXT("body"), TEXT("house_gear"), TEXT("ship_room"), TEXT("chess"), TEXT("printables"), TEXT("voxel_lego"), TEXT("music") };
}

SWVRPSCreatorsPanel::~SWVRPSCreatorsPanel() = default;

FText SWVRPSCreatorsPanel::ClassLabel(const FString& Class)
{
	if (Class.IsEmpty()) return LOCTEXT("ClassAll", "All classes");
	if (Class == TEXT("vault_nft")) return LOCTEXT("ClassVault", "Vault NFTs");
	if (Class == TEXT("body")) return LOCTEXT("ClassBody", "Bodies (trio)");
	if (Class == TEXT("house_gear")) return LOCTEXT("ClassGear", "House gear");
	if (Class == TEXT("ship_room")) return LOCTEXT("ClassShip", "Ship room");
	if (Class == TEXT("chess")) return LOCTEXT("ClassChess", "Chess");
	if (Class == TEXT("printables")) return LOCTEXT("ClassPrint", "Printables");
	if (Class == TEXT("voxel_lego")) return LOCTEXT("ClassVoxel", "Voxel / brick");
	if (Class == TEXT("music")) return LOCTEXT("ClassMusic", "Music");
	return FText::FromString(Class);
}

FString SWVRPSCreatorsPanel::HumanBytes(int64 Bytes)
{
	if (Bytes <= 0) return TEXT("size n/a");
	if (Bytes >= 1024 * 1024) return FString::Printf(TEXT("%.1f MB"), Bytes / (1024.0 * 1024.0));
	return FString::Printf(TEXT("%.0f KB"), FMath::Max(1.0, Bytes / 1024.0));
}

void SWVRPSCreatorsPanel::Construct(const FArguments&)
{
	ImageWrapper = FModuleManager::LoadModulePtr<IImageWrapperModule>(TEXT("ImageWrapper"));

	CardBrush = MakeUnique<FSlateRoundedBoxBrush>(Panel(), 6.0f, Border(), 1.0f);
	MastBrush = MakeUnique<FSlateRoundedBoxBrush>(Base(), 0.0f);
	TileBrush = MakeUnique<FSlateRoundedBoxBrush>(Panel(), 6.0f, Border(), 1.0f);
	WellBrush = MakeUnique<FSlateRoundedBoxBrush>(Base(), 4.0f);
	CodeBrush = MakeUnique<FSlateRoundedBoxBrush>(Panel2(), 6.0f, Accent(), 1.0f);
	PillAccent = MakeUnique<FSlateRoundedBoxBrush>(Base(), 8.0f, Accent(), 1.0f);
	PillValue = MakeUnique<FSlateRoundedBoxBrush>(Base(), 8.0f, Value(), 1.0f);
	PillAmber = MakeUnique<FSlateRoundedBoxBrush>(Base(), 8.0f, Amber(), 1.0f);
	PillGray = MakeUnique<FSlateRoundedBoxBrush>(Base(), 8.0f, BorderSoft(), 1.0f);
	PillRec = MakeUnique<FSlateRoundedBoxBrush>(Base(), 8.0f, Rec(), 1.0f);
	LicenceBrush = MakeUnique<FSlateRoundedBoxBrush>(Panel2(), 6.0f, AccentDim(), 1.0f);

	// The ACT colour is teal on the site; Unreal's PrimaryButton is blue. Same metrics, house colours.
	AccentButton = FAppStyle::Get().GetWidgetStyle<FButtonStyle>("PrimaryButton");
	AccentButton.SetNormal(FSlateRoundedBoxBrush(Accent(), 4.0f))
		.SetHovered(FSlateRoundedBoxBrush(Hex(TEXT("5CE6EB")), 4.0f))
		.SetPressed(FSlateRoundedBoxBrush(AccentDim(), 4.0f))
		.SetDisabled(FSlateRoundedBoxBrush(BorderSoft(), 4.0f))
		.SetNormalForeground(FSlateColor(Base()))
		.SetHoveredForeground(FSlateColor(Base()))
		.SetPressedForeground(FSlateColor(Base()))
		.SetDisabledForeground(FSlateColor(TextLo()));

	ProgressStyle = FAppStyle::Get().GetWidgetStyle<FProgressBarStyle>("ProgressBar");
	ProgressStyle.SetBackgroundImage(FSlateRoundedBoxBrush(Base(), 2.0f))
		.SetFillImage(FSlateRoundedBoxBrush(FLinearColor::White, 2.0f))
		.SetMarqueeImage(FSlateRoundedBoxBrush(FLinearColor::White, 2.0f));

	ClassOptions.Add(MakeShared<FString>(TEXT("")));
	for (const TCHAR* C : GClasses)
	{
		ClassOptions.Add(MakeShared<FString>(C));
	}

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Brushes.Panel"))
		.Padding(0.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[ BuildMasthead() ]
			+ SVerticalBox::Slot().AutoHeight().Padding(S3, S2, S3, S2)
			[
				SNew(SSegmentedControl<int32>)
				.Value_Lambda([this]() { return ActiveTab; })
				.OnValueChanged_Lambda([this](int32 V) { ActiveTab = V; })
				+ SSegmentedControl<int32>::Slot(0).Text(LOCTEXT("TabConnect", "Connect")).Icon(Brush("Icons.Link", "Icons.Info"))
				+ SSegmentedControl<int32>::Slot(1).Text(LOCTEXT("TabBrowse", "Browse")).Icon(Brush("Icons.Search"))
				+ SSegmentedControl<int32>::Slot(2).Text(LOCTEXT("TabImport", "Import")).Icon(Brush("Icons.Import"))
				+ SSegmentedControl<int32>::Slot(3).Text(LOCTEXT("TabAbout", "About")).Icon(Brush("Icons.Info"))
			]
			+ SVerticalBox::Slot().FillHeight(1.0f).Padding(S3, 0.0f, S3, S2)
			[
				SNew(SWidgetSwitcher)
				.WidgetIndex_Lambda([this]() { return ActiveTab; })
				+ SWidgetSwitcher::Slot()[ BuildConnect() ]
				+ SWidgetSwitcher::Slot()[ BuildBrowse() ]
				+ SWidgetSwitcher::Slot()[ BuildImport() ]
				+ SWidgetSwitcher::Slot()[ BuildAbout() ]
			]
			+ SVerticalBox::Slot().AutoHeight()[ BuildFooter() ]
		]
	];
	ActiveTab = (Sub() && Sub()->IsSignedIn()) ? 1 : 0;
	SyncFromSubsystem();   // after the default tab, so a requested tab (OpenPanel(n)) wins
	if (Sub() && Sub()->IsSignedIn() && Sub()->GetCatalogCount() == 0 && Sub()->GetCatalogState() != TEXT("loading"))
	{
		Sub()->RefreshWhoAmI();
		Sub()->RefreshCatalog();
	}
}

// ------------------------------------------------------------------------------------------------------------------
// Building blocks

TSharedRef<SWidget> SWVRPSCreatorsPanel::Pill(const TAttribute<FText>& Text, const TAttribute<const FSlateBrush*>& PillBrush, const TAttribute<FSlateColor>& Color)
{
	return SNew(SBorder)
		.BorderImage(PillBrush)
		.Padding(FMargin(6.0f, 1.0f))
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock).Font(FontPill()).ColorAndOpacity(Color).Text(Text)
		];
}

TSharedRef<SWidget> SWVRPSCreatorsPanel::IconLabel(const char* Icon, const FText& Label, const FSlateFontInfo& Font)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(16.0f).HeightOverride(16.0f)[ SNew(SImage).Image(Brush(Icon)).ColorAndOpacity(FSlateColor::UseForeground()) ]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(S1, 0.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock).Font(Font).Text(Label)
		];
}

TSharedRef<SWidget> SWVRPSCreatorsPanel::Card(const FText& Step, const FText& Title, TSharedRef<SWidget> Body)
{
	return SNew(SBorder)
		.BorderImage(CardBrush.Get())
		.Padding(S3)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, S2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(20.0f).HeightOverride(20.0f).Visibility(Step.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
					[
						SNew(SBorder).BorderImage(PillAccent.Get()).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0.0f)
						[
							SNew(STextBlock).Font(FontPill()).ColorAndOpacity(Accent()).Text(Step)
						]
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(Step.IsEmpty() ? 0.0f : S2, 0.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock).Font(FontStrong()).ColorAndOpacity(TextHi()).Text(Title)
				]
			]
			+ SVerticalBox::Slot().AutoHeight()[ Body ]
		];
}

const FSlateBrush* SWVRPSCreatorsPanel::TierBrush(const FString& Tier) const
{
	return Tier == TEXT("holder") ? PillValue.Get() : PillAccent.Get();
}

FSlateColor SWVRPSCreatorsPanel::TierColor(const FString& Tier) const
{
	return Tier == TEXT("holder") ? FSlateColor(ValueSoft()) : FSlateColor(Accent());
}

FText SWVRPSCreatorsPanel::StatusText() const
{
	const UWVRPSCreatorsSubsystem* S = Sub();
	if (!S) return LOCTEXT("NoSub", "Editor subsystem missing");
	if (S->IsSignedIn())
	{
		return FText::Format(LOCTEXT("SignedInPill", "CONNECTED · {0}"), FText::FromString(S->GetWhoTier().IsEmpty() ? FString(TEXT("KEY")) : S->GetWhoTier().ToUpper()));
	}
	if (S->GetPairState() == TEXT("awaiting_approval")) return LOCTEXT("AwaitPill", "WAITING FOR APPROVAL");
	return LOCTEXT("OffPill", "NOT CONNECTED");
}

// ------------------------------------------------------------------------------------------------------------------
// Masthead: WVRPS wordmark, the trio, connection pill

TSharedRef<SWidget> SWVRPSCreatorsPanel::BuildMasthead()
{
	auto Trio = [this](const FText& Name, FLinearColor C, const FSlateBrush* B)
	{
		return SNew(SBorder).BorderImage(B).Padding(FMargin(6.0f, 1.0f)).VAlign(VAlign_Center)
			[ SNew(STextBlock).Font(FontPill()).ColorAndOpacity(C).Text(Name) ];
	};
	return SNew(SBorder)
		.BorderImage(MastBrush.Get())
		.Padding(FMargin(S3, S2))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock).Font(FontBrand()).ColorAndOpacity(TextHi()).Text(LOCTEXT("Wordmark", "WVRPS"))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(S1, 0.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock).Font(FontBrand()).ColorAndOpacity(Accent()).Text(LOCTEXT("WordmarkCreators", "CREATORS"))
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f)[ SNullWidget::NullWidget ]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					Pill(TAttribute<FText>::CreateLambda([this]() { return StatusText(); }),
						TAttribute<const FSlateBrush*>::CreateLambda([this]() -> const FSlateBrush* { return (Sub() && Sub()->IsSignedIn()) ? PillAccent.Get() : PillGray.Get(); }),
						TAttribute<FSlateColor>::CreateLambda([this]() { return (Sub() && Sub()->IsSignedIn()) ? FSlateColor(Accent()) : FSlateColor(TextMid()); }))
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, S1, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock).Font(FontMeta()).ColorAndOpacity(TextMid()).Text(LOCTEXT("Tagline", "The free WVRPS creators pack, straight into Unreal."))
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f)[ SNullWidget::NullWidget ]
				+ SHorizontalBox::Slot().AutoWidth().Padding(S1, 0.0f, 0.0f, 0.0f)[ Trio(LOCTEXT("Nayomi", "NAYOMI"), Nayomi(), PillValue.Get()) ]
				+ SHorizontalBox::Slot().AutoWidth().Padding(S1, 0.0f, 0.0f, 0.0f)[ Trio(LOCTEXT("Gnar", "GNAR HEART"), GnarHeart(), PillAccent.Get()) ]
				+ SHorizontalBox::Slot().AutoWidth().Padding(S1, 0.0f, 0.0f, 0.0f)[ Trio(LOCTEXT("Dragoon", "DJ DRAGOON"), DjDragoon(), PillAmber.Get()) ]
			]
		];
}

// ------------------------------------------------------------------------------------------------------------------
// Connect

TSharedRef<SWidget> SWVRPSCreatorsPanel::BuildConnect()
{
	auto IsAwaiting = [this]() { return Sub() && Sub()->GetPairState() == TEXT("awaiting_approval"); };
	return SNew(SScrollBox)
		+ SScrollBox::Slot().Padding(0.0f, 0.0f, 0.0f, S3)
		[
			Card(LOCTEXT("Step1", "1"), LOCTEXT("ServerTitle", "Creator API"),
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SEditableTextBox)
					.Font(FontBody())
					.Text_Lambda([this]() { return FText::FromString(Sub() ? Sub()->GetBaseUrl() : FString()); })
					.OnTextCommitted_Lambda([this](const FText& T, ETextCommit::Type) { if (Sub()) { Sub()->SetBaseUrl(T.ToString()); } })
					.ToolTipText(LOCTEXT("BaseTip", "Default https://wvrps.com. Change only for a staging or local test server."))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, S1, 0.0f, 0.0f)
				[
					SNew(STextBlock).Font(FontMeta()).ColorAndOpacity(TextMid()).AutoWrapText(true)
					.Text(LOCTEXT("ServerHint", "Your key is stored in this project's editor user settings (Saved/Config), never in Content."))
				])
		]
		+ SScrollBox::Slot().Padding(0.0f, 0.0f, 0.0f, S3)
		[
			Card(LOCTEXT("Step2", "2"), LOCTEXT("PairTitle", "Pair this editor"),
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Font(FontBody()).ColorAndOpacity(TextMid()).AutoWrapText(true)
					.Text(LOCTEXT("PairHint", "Press Pair. Your browser opens wvrps.com/creators: sign in (wallet or email) and approve the 8-digit code shown here. Holders and non-holders both get a free key."))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, S2, 0.0f, 0.0f).HAlign(HAlign_Left)
				[
					// Signed in: Pair is replaced by Sign out (a second Pair only minted a second key toward the 5-key cap).
					SNew(SWidgetSwitcher)
					.WidgetIndex_Lambda([this]() { return (Sub() && Sub()->IsSignedIn()) ? 1 : 0; })
					+ SWidgetSwitcher::Slot()
					[
						SNew(SButton)
						.ButtonStyle(&AccentButton)
						.ContentPadding(BtnPadFilled())
						.IsEnabled_Lambda([this, IsAwaiting]() { return Sub() && !Sub()->IsSignedIn() && !IsAwaiting() && Sub()->GetPairState() != TEXT("starting"); })
						.OnClicked_Lambda([this]() { if (Sub() && !Sub()->IsSignedIn()) { Sub()->StartPairing(true); } return FReply::Handled(); })
						[
							IconLabel("Icons.Link", LOCTEXT("PairBtn", "Pair with wvrps.com"), FontBody())
						]
					]
					+ SWidgetSwitcher::Slot()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SButton).ButtonStyle(FAppStyle::Get(), "Button").ContentPadding(BtnPadFilled())
							.ToolTipText(LOCTEXT("SignOutTip", "Revokes this editor's creator key on wvrps.com and removes it here. Pair again afterwards if you need a new key."))
							.OnClicked_Lambda([this]() { if (Sub()) { Sub()->SignOut(); } return FReply::Handled(); })
							[ IconLabel("Icons.XCircle", LOCTEXT("SignOutPair", "Sign out"), FontBody()) ]
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(S2, 0.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock).Font(FontMeta()).ColorAndOpacity(TextMid())
							.Text(LOCTEXT("PairedNote", "This editor is paired. Sign out to pair again."))
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, S3, 0.0f, 0.0f)
				[
					SNew(SBorder)
					.Visibility_Lambda([IsAwaiting]() { return IsAwaiting() ? EVisibility::Visible : EVisibility::Collapsed; })
					.BorderImage(CodeBrush.Get())
					.Padding(S3)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock).Font(FontMeta()).ColorAndOpacity(TextMid()).Text(LOCTEXT("CodeLabel", "YOUR CODE"))
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, S1)
						[
							SNew(STextBlock).Font(FontCode()).ColorAndOpacity(Accent())
							.Text_Lambda([this]()
							{
								const FString C = Sub() ? Sub()->GetUserCode() : FString();
								return FText::FromString(C.Len() == 8 ? C.Left(4) + TEXT(" ") + C.Right(4) : C);
							})
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock).Font(FontMeta()).ColorAndOpacity(TextMid())
							.Text_Lambda([this]()
							{
								const double Left = Sub() ? FMath::Max(0.0, Sub()->GetPairExpiresAt() - FPlatformTime::Seconds()) : 0.0;
								return FText::Format(LOCTEXT("Waiting", "Waiting for approval in your browser... expires in {0}:{1}"),
									FText::AsNumber((int32)Left / 60), FText::FromString(FString::Printf(TEXT("%02d"), (int32)Left % 60)));
							})
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, S2, 0.0f, 0.0f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth()
							[
								SNew(SButton).ButtonStyle(FAppStyle::Get(), "Button").ContentPadding(BtnPadFilled())
								.OnClicked_Lambda([this]() { if (Sub()) { FPlatformProcess::LaunchURL(*Sub()->GetApproveUrl(), nullptr, nullptr); } return FReply::Handled(); })
								[ IconLabel("Icons.Link", LOCTEXT("OpenApprove", "Open approve page"), FontBody()) ]
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(S2, 0.0f, 0.0f, 0.0f)
							[
								SNew(SButton).ButtonStyle(FAppStyle::Get(), "SimpleButton").ContentPadding(BtnPadSimple())
								.OnClicked_Lambda([this]() { if (Sub()) { FPlatformApplicationMisc::ClipboardCopy(*Sub()->GetUserCode()); } return FReply::Handled(); })
								[ IconLabel("Icons.Clipboard", LOCTEXT("CopyCode", "Copy code"), FontBody()) ]
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(S2, 0.0f, 0.0f, 0.0f)
							[
								SNew(SButton).ButtonStyle(FAppStyle::Get(), "SimpleButton").ContentPadding(BtnPadSimple())
								.OnClicked_Lambda([this]() { if (Sub()) { Sub()->CancelPairing(); } return FReply::Handled(); })
								[ IconLabel("Icons.XCircle", LOCTEXT("CancelPair", "Cancel"), FontBody()) ]
							]
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, S2, 0.0f, 0.0f)
				[
					SNew(STextBlock).Font(FontMeta()).AutoWrapText(true).ColorAndOpacity(Rec())
					.Visibility_Lambda([this]() { return (Sub() && (Sub()->GetPairState() == TEXT("error") || Sub()->GetPairState() == TEXT("expired"))) ? EVisibility::Visible : EVisibility::Collapsed; })
					.Text_Lambda([this]()
					{
						if (!Sub()) return FText::GetEmpty();
						return Sub()->GetPairState() == TEXT("expired") ? LOCTEXT("Expired", "That code expired. Press Pair for a new one.") : FText::FromString(Sub()->GetLastError());
					})
				])
		]
		+ SScrollBox::Slot()
		[
			SNew(SBox)
			.Visibility_Lambda([this]() { return (Sub() && Sub()->IsSignedIn()) ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				Card(LOCTEXT("Step3", "3"), LOCTEXT("SignedTitle", "Connected"),
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
						[
							SNew(STextBlock).Font(FontName()).ColorAndOpacity(TextHi())
							.Text_Lambda([this]() { return FText::FromString(Sub() && !Sub()->GetWhoLabel().IsEmpty() ? Sub()->GetWhoLabel() : FString(TEXT("WVRPS creator"))); })
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							Pill(TAttribute<FText>::CreateLambda([this]() { return FText::FromString(Sub() && !Sub()->GetWhoTier().IsEmpty() ? Sub()->GetWhoTier().ToUpper() : FString(TEXT("..."))); }),
								TAttribute<const FSlateBrush*>::CreateLambda([this]() { return TierBrush(Sub() ? Sub()->GetWhoTier() : FString()); }),
								TAttribute<FSlateColor>::CreateLambda([this]() { return TierColor(Sub() ? Sub()->GetWhoTier() : FString()); }))
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, S1, 0.0f, 0.0f)
					[
						SNew(STextBlock).Font(FontMeta()).ColorAndOpacity(TextMid())
						.Text_Lambda([this]()
						{
							if (!Sub()) return FText::GetEmpty();
							const FString W = Sub()->GetWhoWallet();
							const FString Short = W.Len() > 12 ? W.Left(6) + TEXT("...") + W.Right(4) : W;
							const FString Exp = Sub()->GetTokenExp() > 0 ? FDateTime::FromUnixTimestamp((int64)Sub()->GetTokenExp()).ToString(TEXT("%Y-%m-%d")) : FString(TEXT("?"));
							return FText::Format(LOCTEXT("WhoLine", "Wallet {0}  ·  key valid until {1}"), FText::FromString(Short.IsEmpty() ? FString(TEXT("email sign-in")) : Short), FText::FromString(Exp));
						})
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, S2, 0.0f, 0.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SButton).ButtonStyle(FAppStyle::Get(), "Button").ContentPadding(BtnPadFilled())
							.OnClicked_Lambda([this]() { ActiveTab = 1; return FReply::Handled(); })
							[ IconLabel("Icons.Search", LOCTEXT("GoBrowse", "Browse the pack"), FontBody()) ]
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(S2, 0.0f, 0.0f, 0.0f)
						[
							SNew(SButton).ButtonStyle(FAppStyle::Get(), "SimpleButton").ContentPadding(BtnPadSimple())
							.OnClicked_Lambda([this]() { if (Sub()) { Sub()->SignOut(); } return FReply::Handled(); })
							[ IconLabel("Icons.XCircle", LOCTEXT("SignOut", "Sign out"), FontBody()) ]
						]
					])
			]
		];
}

// ------------------------------------------------------------------------------------------------------------------
// Browse

TSharedRef<SWidget> SWVRPSCreatorsPanel::BuildBrowse()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, S2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).MinDesiredWidth(150.0f)
				[
					SNew(SComboBox<TSharedPtr<FString>>)
					.OptionsSource(&ClassOptions)
					.OnGenerateWidget_Lambda([](TSharedPtr<FString> O) { return SNew(STextBlock).Font(FontBody()).Text(ClassLabel(*O)); })
					.OnSelectionChanged_Lambda([this](TSharedPtr<FString> O, ESelectInfo::Type) { ClassFilter = O.IsValid() ? *O : FString(); RebuildFilter(); })
					[
						SNew(STextBlock).Font(FontBody()).Text_Lambda([this]() { return ClassLabel(ClassFilter); })
					]
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(S2, 0.0f, 0.0f, 0.0f)
			[
				SNew(SSearchBox)
				.HintText(LOCTEXT("SearchHint", "Search the pack"))
				.OnTextChanged_Lambda([this](const FText& T) { SearchText = T.ToString(); RebuildFilter(); })
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(S2, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton).ButtonStyle(FAppStyle::Get(), "SimpleButton").ContentPadding(BtnPadSimple())
				.ToolTipText(LOCTEXT("RefreshTip", "Reload the catalog (ETag: unchanged manifests cost nothing)"))
				.IsEnabled_Lambda([this]() { return Sub() && Sub()->IsSignedIn(); })
				.OnClicked_Lambda([this]() { if (Sub()) { Sub()->RefreshCatalog(); } return FReply::Handled(); })
				[ IconLabel("Icons.Refresh", LOCTEXT("Refresh", "Refresh"), FontBody()) ]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, S1)
		[
			SNew(STextBlock).Font(FontMeta()).ColorAndOpacity(TextMid())
			.Text_Lambda([this]()
			{
				if (!Sub()) return FText::GetEmpty();
				if (!Sub()->IsSignedIn()) return LOCTEXT("BrowseNoKey", "Pair this editor first (Connect tab) to load the pack.");
				if (Sub()->GetCatalogState() == TEXT("loading")) return LOCTEXT("BrowseLoading", "Loading the catalog...");
				if (Sub()->GetCatalogState() == TEXT("error")) return FText::FromString(Sub()->GetLastError());
				return FText::Format(LOCTEXT("BrowseCount", "{0} of {1} assets  ·  manifest {2}"), FText::AsNumber(Filtered.Num()), FText::AsNumber(Sub()->GetCatalogCount()), FText::FromString(Sub()->GetManifestGenerated()));
			})
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, S2)
		[
			// Selection bar: the tile body selects, this primary action (or double-click / Enter) imports.
			SNew(SBorder).BorderImage(CardBrush.Get()).Padding(FMargin(S2, S1))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Font(FontBody()).ColorAndOpacity(TextHi()).OverflowPolicy(ETextOverflowPolicy::Ellipsis)
					.Text_Lambda([this]()
					{
						const FItemPtr S = SelectedItem();
						if (!S.IsValid()) return LOCTEXT("SelHint", "Click a tile to select it. Double-click it, press Enter or use Import to bring it into the project.");
						return S->IsImportable()
							? FText::Format(LOCTEXT("SelOk", "Selected: {0}"), FText::FromString(S->Name.IsEmpty() ? S->Id : S->Name))
							: FText::Format(LOCTEXT("SelNo", "Selected: {0}  -  not importable: {1}"), FText::FromString(S->Name.IsEmpty() ? S->Id : S->Name), FText::FromString(S->NotImportableReason()));
					})
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(S2, 0.0f, 0.0f, 0.0f)
				[
					SNew(SBox).HeightOverride(32.0f).MinDesiredWidth(120.0f)
					.ToolTipText_Lambda([this]()
					{
						const FItemPtr S = SelectedItem();
						return !S.IsValid() ? LOCTEXT("SelNone", "Select a tile first") : S->IsImportable() ? LOCTEXT("SelImportTip", "Import the selected asset (Enter / double-click do the same)") : FText::FromString(S->NotImportableReason());
					})
					[
						SNew(SButton)
						.ButtonStyle(&AccentButton)
						.ContentPadding(FMargin(12.0f, 0.0f))
						.HAlign(HAlign_Center).VAlign(VAlign_Center)
						.IsEnabled_Lambda([this]() { const FItemPtr S = SelectedItem(); return S.IsValid() && S->IsImportable() && Sub() && Sub()->IsSignedIn(); })
						.OnClicked_Lambda([this]() { QueueImport(SelectedItem()); return FReply::Handled(); })
						[
							IconLabel("Icons.Import", LOCTEXT("ImportSelected", "IMPORT"), FontStrong())
						]
					]
				]
			]
		]
		+ SVerticalBox::Slot().FillHeight(1.0f)
		[
			SAssignNew(TileView, STileView<FItemPtr>)
			.ListItemsSource(&Filtered)
			.ItemWidth(188.0f)
			.ItemHeight(272.0f)
			.SelectionMode(ESelectionMode::Single)
			.OnGenerateTile(this, &SWVRPSCreatorsPanel::OnGenerateTile)
			.OnMouseButtonDoubleClick_Lambda([this](FItemPtr Item) { QueueImport(Item); })
		];
}

TSharedRef<ITableRow> SWVRPSCreatorsPanel::OnGenerateTile(FItemPtr Item, const TSharedRef<STableViewBase>& Owner)
{
	RequestThumb(Item);
	const FString Id = Item->Id;
	const bool bImportable = Item->IsImportable();
	const bool bGearOnBody = Item->Class == TEXT("house_gear") && !Item->Character.IsEmpty() && !Item->Character.StartsWith(TEXT("rig:"));
	const FText KindText = Item->UrlKind == TEXT("mml") ? LOCTEXT("KindMml", "CHARACTER")
		: Item->IsAudio() ? FText::Format(LOCTEXT("KindAudio", "AUDIO {0}"), FText::FromString(Item->UrlKind.ToUpper()))
		: bGearOnBody ? FText::Format(LOCTEXT("KindGear", "GEAR on {0}"), FText::FromString(Item->Character.ToUpper()))
		: Item->UrlKind == TEXT("json") ? LOCTEXT("KindJson", "BRICK KIT (JSON)")
		: LOCTEXT("KindGlb", "GLB");
	const FText ImportTip = Item->IsAudio() ? LOCTEXT("AudioTip", "Download and import as a SoundWave (stock audio importer). Double-click the tile or press Enter does the same.")
		: Item->UrlKind == TEXT("mml") ? LOCTEXT("MmlTip", "Import body first, then every gear GLB bound to the body's skeleton")
		: bGearOnBody ? LOCTEXT("GearTip", "Imports the matching body first (or reuses it), then binds this gear to the body's skeleton")
		: !bImportable ? FText::FromString(Item->NotImportableReason())
		: LOCTEXT("GlbTip", "Download and import through Interchange. Double-click the tile or press Enter does the same.");
	FItemPtr Captured = Item;
	return SNew(STableRow<FItemPtr>, Owner)
		.Padding(S1)
		.ToolTipText(ImportTip)
		[
			SNew(SBorder)
			.BorderImage(TileBrush.Get())
			.Padding(S2)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SBorder).BorderImage(WellBrush.Get()).Padding(0.0f).HAlign(HAlign_Center).VAlign(VAlign_Center)
					[
						SNew(SBox).WidthOverride(150.0f).HeightOverride(120.0f).HAlign(HAlign_Center).VAlign(VAlign_Center)
						[
							SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
							[
								SNew(SImage).Image_Lambda([this, Id]() { const FSlateBrush* B = ThumbFor(Id); return B ? B : Brush("ClassIcon.StaticMesh"); })
							]
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, S2, 0.0f, 0.0f)
				[
					SNew(STextBlock).Font(FontStrong()).ColorAndOpacity(TextHi()).Text(FText::FromString(Item->Name.IsEmpty() ? Item->Id : Item->Name))
					.OverflowPolicy(ETextOverflowPolicy::Ellipsis).ToolTipText(FText::FromString(Item->Name + TEXT("\n") + Item->Id + TEXT("\nsource: ") + Item->Source))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, S1, 0.0f, 0.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Font(FontMeta()).ColorAndOpacity(TextMid()).Text(ClassLabel(Item->Class))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						Pill(FText::FromString(Item->Tier.IsEmpty() ? FString(TEXT("PUBLIC")) : Item->Tier.ToUpper()), TierBrush(Item->Tier), TierColor(Item->Tier))
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, S1, 0.0f, 0.0f)
				[
					SNew(STextBlock).Font(FontMeta()).ColorAndOpacity(bImportable ? TextLo() : Amber()).OverflowPolicy(ETextOverflowPolicy::Ellipsis)
					.Text(bImportable ? FText::Format(LOCTEXT("TileMeta", "{0} · {1}"), KindText, FText::FromString(HumanBytes(Item->Bytes)))
						: FText::Format(LOCTEXT("TileMetaNo", "{0} · not a mesh, not importable"), KindText))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, S2, 0.0f, 0.0f)
				[
					// High-contrast primary action, 32 px tall. Greyed out with the reason as tooltip for non-importables.
					SNew(SBox).HeightOverride(32.0f).ToolTipText(ImportTip)
					[
						SNew(SButton)
						.ButtonStyle(&AccentButton)
						.ContentPadding(FMargin(8.0f, 0.0f))
						.HAlign(HAlign_Center).VAlign(VAlign_Center)
						.IsEnabled(bImportable)
						.OnClicked_Lambda([this, Captured]() { QueueImport(Captured); return FReply::Handled(); })
						[
							SNew(STextBlock).Font(FontStrong())
							.Text(bImportable ? LOCTEXT("Import", "IMPORT") : LOCTEXT("NoImport", "NOT IMPORTABLE"))
						]
					]
				]
			]
		];
}

SWVRPSCreatorsPanel::FItemPtr SWVRPSCreatorsPanel::SelectedItem() const
{
	if (!TileView.IsValid())
	{
		return nullptr;
	}
	const TArray<FItemPtr> Sel = TileView->GetSelectedItems();
	return Sel.Num() > 0 ? Sel[0] : nullptr;
}

void SWVRPSCreatorsPanel::Toast(const FText& Text, bool bSuccess)
{
	FNotificationInfo Info(Text);
	Info.ExpireDuration = bSuccess ? 3.0f : 6.0f;
	Info.bUseLargeFont = false;
	Info.bFireAndForget = true;
	if (TSharedPtr<SNotificationItem> N = FSlateNotificationManager::Get().AddNotification(Info))
	{
		N->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
	}
}

void SWVRPSCreatorsPanel::QueueImport(const FItemPtr& Item)
{
	UWVRPSCreatorsSubsystem* S = Sub();
	if (!S || !Item.IsValid())
	{
		return;
	}
	const FString Name = Item->Name.IsEmpty() ? Item->Id : Item->Name;
	if (!Item->IsImportable())
	{
		// Not silent any more: say why (brick kits are JSON build files, not meshes).
		S->Log(FString::Printf(TEXT("Not importable: %s. %s"), *Name, *Item->NotImportableReason()));
		Toast(FText::Format(LOCTEXT("ToastNo", "{0} is not importable: {1}"), FText::FromString(Name), FText::FromString(Item->NotImportableReason())), false);
		return;
	}
	if (!S->IsSignedIn())
	{
		S->Log(TEXT("Import: pair this editor first (Connect tab)."));
		Toast(LOCTEXT("ToastPair", "Pair this editor first (Connect tab)."), false);
		SetActiveTab(0);
		return;
	}
	if (S->ImportItem(Item->Id, DestRoot))   // logs "Queued <name> (<id>) -> <dest>" immediately
	{
		Toast(FText::Format(LOCTEXT("ToastQueued", "Queued {0}"), FText::FromString(Name)), true);
		ActiveTab = 2;
	}
	else
	{
		Toast(FText::Format(LOCTEXT("ToastFail", "Could not queue {0}: see the Import tab log."), FText::FromString(Name)), false);
	}
}

FReply SWVRPSCreatorsPanel::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (ActiveTab == 1 && (InKeyEvent.GetKey() == EKeys::Enter || InKeyEvent.GetKey() == EKeys::Virtual_Accept))
	{
		if (const FItemPtr S = SelectedItem())
		{
			QueueImport(S);
			return FReply::Handled();
		}
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

void SWVRPSCreatorsPanel::RebuildFilter()
{
	Filtered.Reset();
	if (const UWVRPSCreatorsSubsystem* S = Sub())
	{
		for (const FItemPtr& It : S->GetItems())
		{
			if (!ClassFilter.IsEmpty() && It->Class != ClassFilter)
			{
				continue;
			}
			if (!SearchText.IsEmpty() && !It->Name.Contains(SearchText) && !It->Id.Contains(SearchText) && !It->Class.Contains(SearchText))
			{
				continue;
			}
			Filtered.Add(It);
		}
	}
	if (TileView.IsValid())
	{
		TileView->RequestListRefresh();
	}
}

// ------------------------------------------------------------------------------------------------------------------
// Thumbnails: async HTTP, background decode, brush on the game thread

namespace
{
	/** PNG/JPEG bytes -> BGRA downscaled to <= 256 px (background thread). thumb.php always sends PNG. */
	bool DecodeThumb(IImageWrapperModule* Module, const TArray<uint8>& File, int32& OW, int32& OH, TArray<uint8>& Out)
	{
		const EImageFormat Format = Module->DetectImageFormat(File.GetData(), File.Num());
		TSharedPtr<IImageWrapper> W = (Format == EImageFormat::PNG || Format == EImageFormat::JPEG) ? Module->CreateImageWrapper(Format) : nullptr;
		TArray64<uint8> Raw;
		if (!W.IsValid() || !W->SetCompressed(File.GetData(), File.Num()) || !W->GetRaw(ERGBFormat::BGRA, 8, Raw))
		{
			return false;
		}
		const int32 SrcW = (int32)W->GetWidth(), SrcH = (int32)W->GetHeight();
		const int32 Factor = FMath::Max(1, FMath::DivideAndRoundUp(FMath::Max(SrcW, SrcH), 256));
		OW = FMath::Max(1, SrcW / Factor);
		OH = FMath::Max(1, SrcH / Factor);
		Out.SetNumUninitialized(OW * OH * 4);
		for (int32 Y = 0; Y < OH; ++Y)
		{
			for (int32 X = 0; X < OW; ++X)
			{
				const int64 I = ((int64)(Y * Factor) * SrcW + X * Factor) * 4;
				FMemory::Memcpy(&Out[(Y * OW + X) * 4], &Raw[I], 4);
			}
		}
		return true;
	}
}

FString SWVRPSCreatorsPanel::ThumbCacheFile(const FItemPtr& Item) const
{
	// Keyed on the id AND its source URLs, so a manifest that changes a thumbnail gets a fresh file.
	const FString Key = FMD5::HashAnsiString(*(Item->Id + TEXT("|") + Item->ThumbUrl + TEXT("|") + Item->ThumbHqUrl + TEXT("|") + Item->TokenId));
	return FPaths::Combine(Sub() ? Sub()->GetThumbCacheDir() : FString(TEXT(".")), FString::Printf(TEXT("%s_%d.png"), *Key, ThumbSize));
}

void SWVRPSCreatorsPanel::RequestThumb(const FItemPtr& Item)
{
	if (!ImageWrapper || !Sub() || Thumbs.Contains(Item->Id) || PendingThumbs.Contains(Item->Id) || FailedThumbs.Contains(Item->Id))
	{
		return;
	}
	if (!Item->HasThumbSource())
	{
		FailedThumbs.Add(Item->Id);   // bodies / rooms / chess: nothing to convert, the class icon stays
		Sub()->NoteThumb(TEXT("none"));
		return;
	}
	PendingThumbs.Add(Item->Id);
	// Disk cache first (read + decode off the game thread); a miss joins the HTTP queue.
	const FString File = ThumbCacheFile(Item);
	const TWeakPtr<SWVRPSCreatorsPanel> Weak = StaticCastSharedRef<SWVRPSCreatorsPanel>(AsShared());
	IImageWrapperModule* Module = ImageWrapper;
	FItemPtr Captured = Item;
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [Weak, Module, Captured, File]()
	{
		TArray<uint8> Bytes;
		int32 OW = 0, OH = 0;
		TArray<uint8> Out;
		const bool bHit = FFileHelper::LoadFileToArray(Bytes, *File, FILEREAD_Silent) && DecodeThumb(Module, Bytes, OW, OH, Out);
		AsyncTask(ENamedThreads::GameThread, [Weak, Captured, File, bHit, OW, OH, Out = MoveTemp(Out)]() mutable
		{
			const TSharedPtr<SWVRPSCreatorsPanel> P = Weak.Pin();
			if (!P.IsValid())
			{
				return;
			}
			if (bHit)
			{
				P->OnThumbDecoded(Captured->Id, OW, OH, MoveTemp(Out));
				if (P->Sub()) { P->Sub()->NoteThumb(TEXT("disk_hit")); }
				return;
			}
			P->ThumbQueue.Emplace(Captured, File);
			P->PumpThumbs();
		});
	});
}

void SWVRPSCreatorsPanel::PumpThumbs()
{
	if (!Sub() || !Sub()->IsSignedIn() || FPlatformTime::Seconds() < ThumbPauseUntil)
	{
		return;
	}
	while (ThumbsInFlight < MaxThumbsInFlight && ThumbQueue.Num() > 0)
	{
		const TPair<FItemPtr, FString> Next = ThumbQueue.Last();   // newest first: the tiles on screen now
		ThumbQueue.RemoveAt(ThumbQueue.Num() - 1);
		StartThumbHttp(Next.Key, Next.Value);
	}
}

void SWVRPSCreatorsPanel::StartThumbHttp(const FItemPtr& Item, const FString& CacheFile)
{
	++ThumbsInFlight;
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = Sub()->MakeThumbRequest(Item->Id, ThumbSize);
	const TWeakPtr<SWVRPSCreatorsPanel> Weak = StaticCastSharedRef<SWVRPSCreatorsPanel>(AsShared());
	IImageWrapperModule* Module = ImageWrapper;
	FItemPtr Captured = Item;
	Req->OnProcessRequestComplete().BindLambda([Weak, Module, Captured, CacheFile](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
	{
		const TSharedPtr<SWVRPSCreatorsPanel> P = Weak.Pin();
		if (!P.IsValid())
		{
			return;
		}
		P->ThumbsInFlight = FMath::Max(0, P->ThumbsInFlight - 1);
		const int32 Code = (bOk && Resp.IsValid()) ? Resp->GetResponseCode() : 0;
		if (Code == 429)
		{
			// Thumb bucket full: back off 10 s and retry this one (the grid keeps its placeholders meanwhile).
			P->ThumbQueue.Insert(TPair<FItemPtr, FString>(Captured, CacheFile), 0);
			P->ThumbPauseUntil = FPlatformTime::Seconds() + 10.0;
			return;
		}
		if (Code != 200)
		{
			P->OnThumbFailed(Captured->Id, Code == 415 ? TEXT("unsupported") : (Code == 404 ? TEXT("none") : TEXT("failed")));
			P->PumpThumbs();
			return;
		}
		TArray<uint8> File = Resp->GetContent();
		AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [Weak, Module, Captured, CacheFile, File = MoveTemp(File)]()
		{
			int32 OW = 0, OH = 0;
			TArray<uint8> Out;
			const bool bDecoded = DecodeThumb(Module, File, OW, OH, Out);
			if (bDecoded)
			{
				FFileHelper::SaveArrayToFile(File, *CacheFile);   // only a PNG that decodes is cached
			}
			AsyncTask(ENamedThreads::GameThread, [Weak, Captured, bDecoded, OW, OH, Out = MoveTemp(Out)]() mutable
			{
				if (const TSharedPtr<SWVRPSCreatorsPanel> P2 = Weak.Pin())
				{
					if (bDecoded)
					{
						P2->OnThumbDecoded(Captured->Id, OW, OH, MoveTemp(Out));
						if (P2->Sub()) { P2->Sub()->NoteThumb(TEXT("http_ok")); }
					}
					else
					{
						P2->OnThumbFailed(Captured->Id, TEXT("failed"));
					}
					P2->PumpThumbs();
				}
			});
		});
	});
	Req->ProcessRequest();
}

void SWVRPSCreatorsPanel::OnThumbFailed(const FString& Id, const FString& Outcome)
{
	PendingThumbs.Remove(Id);
	FailedThumbs.Add(Id);
	if (Sub())
	{
		Sub()->NoteThumb(Outcome);
	}
}

void SWVRPSCreatorsPanel::OnThumbDecoded(const FString& Id, int32 W, int32 H, TArray<uint8>&& BGRA)
{
	PendingThumbs.Remove(Id);
	const FName Name(*FString::Printf(TEXT("WVRPSCreatorsThumb_%u"), ++BrushCounter));
	TSharedPtr<FSlateDynamicImageBrush> B = FSlateDynamicImageBrush::CreateWithImageData(Name, FVector2D(W, H), BGRA);
	if (B.IsValid())
	{
		Thumbs.Add(Id, B);
	}
}

const FSlateBrush* SWVRPSCreatorsPanel::ThumbFor(const FString& Id) const
{
	const TSharedPtr<FSlateDynamicImageBrush>* B = Thumbs.Find(Id);
	return (B && B->IsValid()) ? B->Get() : nullptr;
}

// ------------------------------------------------------------------------------------------------------------------
// Import

TSharedRef<SWidget> SWVRPSCreatorsPanel::BuildImport()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, S2)
		[
			Card(FText::GetEmpty(), LOCTEXT("DestTitle", "Destination"),
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SEditableTextBox).Font(FontBody())
					.Text_Lambda([this]() { return FText::FromString(DestRoot); })
					.OnTextCommitted_Lambda([this](const FText& T, ETextCommit::Type) { DestRoot = T.ToString().TrimStartAndEnd(); if (DestRoot.IsEmpty()) { DestRoot = TEXT("/Game/WVRPS"); } })
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, S1, 0.0f, 0.0f)
				[
					SNew(STextBlock).Font(FontMeta()).ColorAndOpacity(TextMid()).AutoWrapText(true)
					.Text(LOCTEXT("DestHint", "Each asset lands in <root>/<class>/<id>/. Characters: the body imports first and makes the one skeleton; every gear GLB then binds to it (Gear_NN subfolders). Files are checked for the glTF header before import."))
				])
		]
		+ SVerticalBox::Slot().FillHeight(1.0f)
		[
			SNew(SBorder).BorderImage(CardBrush.Get()).Padding(S2)
			[
				SAssignNew(JobList, SListView<FJobPtr>)
				.ListItemsSource(&JobRows)
				.SelectionMode(ESelectionMode::None)
				.OnGenerateRow(this, &SWVRPSCreatorsPanel::OnGenerateJobRow)
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, S2, 0.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton).ButtonStyle(FAppStyle::Get(), "SimpleButton").ContentPadding(BtnPadSimple())
				.OnClicked_Lambda([this]() { if (Sub()) { IFileManager::Get().MakeDirectory(*Sub()->GetCacheDir(), true); FPlatformProcess::ExploreFolder(*Sub()->GetCacheDir()); } return FReply::Handled(); })
				[ IconLabel("Icons.FolderOpen", LOCTEXT("OpenCache", "Open download cache"), FontBody()) ]
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(S2, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton).ButtonStyle(FAppStyle::Get(), "SimpleButton").ContentPadding(BtnPadSimple())
				.OnClicked_Lambda([this]()
				{
					FContentBrowserModule& CB = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
					CB.Get().SyncBrowserToFolders(TArray<FString>{ DestRoot });
					return FReply::Handled();
				})
				[ IconLabel("Icons.BrowseContent", LOCTEXT("ShowCB", "Show in Content Browser"), FontBody()) ]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, S2, 0.0f, 0.0f)
		[
			SNew(SBox).HeightOverride(120.0f)
			[
				SNew(SMultiLineEditableTextBox)
				.IsReadOnly(true)
				.Font(FontMono())
				.AlwaysShowScrollbars(true)
				.Text_Lambda([this]() { return FText::FromString(Sub() ? Sub()->GetLogText() : FString()); })
			]
		];
}

TSharedRef<ITableRow> SWVRPSCreatorsPanel::OnGenerateJobRow(FJobPtr Job, const TSharedRef<STableViewBase>& Owner)
{
	return SNew(STableRow<FJobPtr>, Owner)
		.Padding(FMargin(0.0f, S1))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Font(FontStrong()).ColorAndOpacity(TextHi()).Text(FText::FromString(Job->Item.Name.IsEmpty() ? Job->ItemId : Job->Item.Name))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					Pill(TAttribute<FText>::CreateLambda([Job]()
						{
							switch (Job->State)
							{
							case EWVRPSJobState::Queued: return LOCTEXT("SQ", "QUEUED");
							case EWVRPSJobState::Resolving: return LOCTEXT("SR", "RESOLVING");
							case EWVRPSJobState::Downloading: return LOCTEXT("SD", "DOWNLOADING");
							case EWVRPSJobState::Importing: return LOCTEXT("SI", "IMPORTING");
							case EWVRPSJobState::Done: return LOCTEXT("SOK", "DONE");
							default: return LOCTEXT("SF", "FAILED");
							}
						}),
						TAttribute<const FSlateBrush*>::CreateLambda([this, Job]() -> const FSlateBrush*
						{
							return Job->State == EWVRPSJobState::Done ? PillAccent.Get() : Job->State == EWVRPSJobState::Failed ? PillRec.Get() : PillAmber.Get();
						}),
						TAttribute<FSlateColor>::CreateLambda([Job]()
						{
							return Job->State == EWVRPSJobState::Done ? FSlateColor(Accent()) : Job->State == EWVRPSJobState::Failed ? FSlateColor(Rec()) : FSlateColor(Amber());
						}))
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, S1, 0.0f, 0.0f)
			[
				SNew(SBox).HeightOverride(6.0f)
				[
				SNew(SProgressBar)
				.Style(&ProgressStyle)
				.Percent_Lambda([Job]() { return TOptional<float>(Job->Progress); })
				.FillColorAndOpacity_Lambda([Job]() { return Job->State == EWVRPSJobState::Failed ? FSlateColor(Rec()) : FSlateColor(Accent()); })
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, S1, 0.0f, 0.0f)
			[
				SNew(STextBlock).Font(FontMeta()).ColorAndOpacity(TextMid()).AutoWrapText(true)
				.Text_Lambda([Job]() { return FText::FromString(Job->Message); })
			]
		];
}

// ------------------------------------------------------------------------------------------------------------------
// About + footer

TSharedRef<SWidget> SWVRPSCreatorsPanel::BuildAbout()
{
	return SNew(SScrollBox)
		+ SScrollBox::Slot().Padding(0.0f, 0.0f, 0.0f, S3)
		[
			Card(FText::GetEmpty(), LOCTEXT("LicenceTitle", "Licence"),
				SNew(SBorder).BorderImage(LicenceBrush.Get()).Padding(S3)
				[
					SNew(STextBlock).Font(FontBody()).ColorAndOpacity(TextHi()).AutoWrapText(true)
					.Text_Lambda([this]() { return FText::FromString(Sub() ? Sub()->GetLicense() : FString(UWVRPSCreatorsSubsystem::LicenseLine())); })
				])
		]
		+ SScrollBox::Slot().Padding(0.0f, 0.0f, 0.0f, S3)
		[
			Card(FText::GetEmpty(), LOCTEXT("AboutTitle", "About"),
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Font(FontBody()).ColorAndOpacity(TextMid())
					.Text_Lambda([this]()
					{
						return FText::Format(LOCTEXT("AboutLines", "WVRPS Creators {0}\nManifest generated: {1}\nCreator API: {2}\nDownload cache: {3}\nDaily download quota: 200 MB per key"),
							FText::FromString(UWVRPSCreatorsSubsystem::PluginVersion()),
							FText::FromString(Sub() && !Sub()->GetManifestGenerated().IsEmpty() ? Sub()->GetManifestGenerated() : FString(TEXT("not loaded yet"))),
							FText::FromString(Sub() ? Sub()->GetBaseUrl() : FString()),
							FText::FromString(Sub() ? Sub()->GetCacheDir() : FString()));
					})
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, S2, 0.0f, 0.0f)
				[
					SNew(STextBlock).Font(FontBody()).ColorAndOpacity(TextMid()).AutoWrapText(true)
					.Text(LOCTEXT("AboutTrio", "Every WVRPS character is Nayomi, Gnar Heart or DJ Dragoon: one body GLB plus gear GLBs on the same skeleton, imported exactly as published."))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, S2, 0.0f, 0.0f).HAlign(HAlign_Left)
				[
					SNew(SButton).ButtonStyle(FAppStyle::Get(), "Button").ContentPadding(BtnPadFilled())
					.OnClicked_Lambda([]() { FPlatformProcess::LaunchURL(TEXT("https://wvrps.com/creators"), nullptr, nullptr); return FReply::Handled(); })
					[ IconLabel("Icons.Link", LOCTEXT("OpenHub", "wvrps.com/creators"), FontBody()) ]
				])
		];
}

TSharedRef<SWidget> SWVRPSCreatorsPanel::BuildFooter()
{
	return SNew(SBorder)
		.BorderImage(MastBrush.Get())
		.Padding(FMargin(S3, S1 + 2.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Font(FontMeta()).ColorAndOpacity(TextMid()).AutoWrapText(true)
				.Text(FText::FromString(UWVRPSCreatorsSubsystem::LicenseLine()))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(S2, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock).Font(FontPill()).ColorAndOpacity(Accent()).Text(LOCTEXT("Site", "WVRPS.COM"))
			]
		];
}

// ------------------------------------------------------------------------------------------------------------------
// Sync

void SWVRPSCreatorsPanel::SyncFromSubsystem()
{
	UWVRPSCreatorsSubsystem* S = Sub();
	if (!S)
	{
		return;
	}
	SeenRevision = S->GetRevision();
	if (S->GetCatalogCount() != SeenItemCount || S->GetManifestGenerated() != SeenGenerated)
	{
		SeenItemCount = S->GetCatalogCount();
		SeenGenerated = S->GetManifestGenerated();
		RebuildFilter();
	}
	if (JobRows.Num() != S->GetJobs().Num())
	{
		JobRows = S->GetJobs();
		if (JobList.IsValid())
		{
			JobList->RequestListRefresh();
		}
	}
	if (S->GetRequestedTab() >= 0)
	{
		SetActiveTab(S->GetRequestedTab());
		S->ClearRequestedTab();
	}
}

void SWVRPSCreatorsPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	if (ThumbQueue.Num() > 0 && ThumbsInFlight < MaxThumbsInFlight)
	{
		PumpThumbs();   // resumes after a 429 back-off, or after signing in
	}
	if (const UWVRPSCreatorsSubsystem* S = Sub())
	{
		if (S->GetRevision() != SeenRevision)
		{
			SyncFromSubsystem();
		}
	}
}

#undef LOCTEXT_NAMESPACE
