// WVRPS Creators look: WVRPSTools v3-beautify tokens (spacing 4/8/12/16, type tiers, brush-with-fallback) plus the WVRPS
// house palette from simple/13122025/src/styles/house-tokens.css (G1.1: every colour names a MEANING).
#pragma once

#include "CoreMinimal.h"
#include "Math/Color.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateColor.h"
#include "Fonts/SlateFontInfo.h"
#include "Layout/Margin.h"

namespace WVRPSCreatorsUi
{
	// ---- v3 spacing / type (DESIGN_panel_v3_beautify.md S1.1-1.2) -------------------------------------------------
	inline constexpr float S1 = 4.0f;
	inline constexpr float S2 = 8.0f;
	inline constexpr float S3 = 12.0f;
	inline constexpr float S4 = 16.0f;
	inline FMargin BtnPadFilled() { return FMargin(0.0f, 4.0f); }
	inline FMargin BtnPadSimple() { return FMargin(4.0f, 2.0f); }

	inline FSlateFontInfo FontBody()   { return FAppStyle::Get().GetFontStyle("NormalFont"); }
	inline FSlateFontInfo FontMeta()   { return FAppStyle::Get().GetFontStyle("SmallFont"); }
	inline FSlateFontInfo FontStrong() { return FCoreStyle::GetDefaultFontStyle("Bold", 10); }
	inline FSlateFontInfo FontPill()   { return FCoreStyle::GetDefaultFontStyle("Bold", 8); }
	inline FSlateFontInfo FontName()   { return FCoreStyle::GetDefaultFontStyle("Bold", 12); }
	inline FSlateFontInfo FontMono()   { return FCoreStyle::GetDefaultFontStyle("Mono", 9); }
	inline FSlateFontInfo FontCode()   { return FCoreStyle::GetDefaultFontStyle("Mono", 26); }
	inline FSlateFontInfo FontBrand()  { return FCoreStyle::GetDefaultFontStyle("Bold", 15); }

	/** A registered brush, or the fallback (never Slate's missing-brush checkerboard). */
	inline const FSlateBrush* Brush(const char* Name, const char* Fallback = "Icons.Info")
	{
		return FAppStyle::Get().GetOptionalBrush(FName(Name), nullptr, FAppStyle::GetBrush(FName(Fallback)));
	}

	// ---- WVRPS house palette (house-tokens.css :root) -------------------------------------------------------------
	inline FLinearColor Hex(const TCHAR* H) { return FLinearColor(FColor::FromHex(H)); }
	inline FLinearColor Accent()    { return Hex(TEXT("25D9E0")); } // --h-accent: ACT (press this)
	inline FLinearColor AccentDim() { return Hex(TEXT("1B8A8E")); } // --h-accent-dim
	inline FLinearColor Value()     { return Hex(TEXT("FF2F9D")); } // --h-value: OWNED / holder / rarity, never a CTA
	inline FLinearColor ValueSoft() { return Hex(TEXT("FF62B7")); } // --h-value-soft
	inline FLinearColor Rec()       { return Hex(TEXT("FF3B30")); } // --h-rec: errors / irreversible
	inline FLinearColor Amber()     { return Hex(TEXT("E8A33D")); } // --h-amber
	inline FLinearColor Base()      { return Hex(TEXT("08090C")); } // --h-base: masthead band
	inline FLinearColor Panel()     { return Hex(TEXT("101319")); } // --h-panel: cards
	inline FLinearColor Panel2()    { return Hex(TEXT("161A22")); } // --h-panel2: raised / selected
	inline FLinearColor Border()    { return Hex(TEXT("232833")); } // --h-border: 1px hairline
	inline FLinearColor BorderSoft(){ return Hex(TEXT("2E3540")); }
	inline FLinearColor TextHi()    { return Hex(TEXT("FFFFFF")); }
	inline FLinearColor TextMid()   { return Hex(TEXT("9AA3AF")); }
	inline FLinearColor TextLo()    { return Hex(TEXT("5B636E")); }

	// The trio (site character colours: Nayomi pink, Gnar Heart teal, DJ Dragoon amber).
	inline FLinearColor Nayomi()    { return Hex(TEXT("FF62B7")); }
	inline FLinearColor GnarHeart() { return Hex(TEXT("25D9E0")); }
	inline FLinearColor DjDragoon() { return Hex(TEXT("E8A33D")); }
}
