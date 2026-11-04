// MML document -> body GLB url + gear GLB urls.
// Body resolution mirrors simple/13122025/src/utils/otherside.js:8-29 (src=".glb" first, then url=".glb", then a bare
// https://....glb), resolved against the .mml's own URL. Gear = every <m-model src="....glb"> child (atalier_mml_save.php
// publishes gear that way). Pure function: no engine state, no I/O.
#pragma once

#include "CoreMinimal.h"
#include "Internationalization/Regex.h"

namespace WVRPSMml
{
	inline FString ResolveRelative(const FString& Url, const FString& BaseUrl)
	{
		if (Url.StartsWith(TEXT("http://"), ESearchCase::IgnoreCase) || Url.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase))
		{
			return Url;
		}
		if (Url.StartsWith(TEXT("//")))
		{
			return (BaseUrl.StartsWith(TEXT("http://"), ESearchCase::IgnoreCase) ? TEXT("http:") : TEXT("https:")) + Url;
		}
		int32 Slash = INDEX_NONE;
		if (!BaseUrl.FindLastChar(TEXT('/'), Slash))
		{
			return Url;
		}
		FString Dir = BaseUrl.Left(Slash + 1);
		FString Rel = Url;
		if (Rel.StartsWith(TEXT("/")))
		{
			// origin-relative: scheme://host + path
			const int32 SchemeEnd = BaseUrl.Find(TEXT("://"));
			const int32 HostEnd = SchemeEnd == INDEX_NONE ? INDEX_NONE : BaseUrl.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromStart, SchemeEnd + 3);
			return (HostEnd == INDEX_NONE ? BaseUrl : BaseUrl.Left(HostEnd)) + Rel;
		}
		while (Rel.StartsWith(TEXT("./")))
		{
			Rel.RightChopInline(2);
		}
		return Dir + Rel;
	}

	/** First capture of Pattern in Text, or empty. */
	inline FString First(const FString& Text, const TCHAR* Pattern)
	{
		FRegexMatcher M(FRegexPattern(Pattern, ERegexPatternFlags::CaseInsensitive), Text);
		return M.FindNext() ? M.GetCaptureGroup(1) : FString();
	}

	/** Returns false when no body GLB is found. */
	inline bool Parse(const FString& MmlText, const FString& MmlUrl, FString& OutBody, TArray<FString>& OutGear)
	{
		OutBody.Reset();
		OutGear.Reset();

		// The <m-character ...> tag's own src wins (that is the body); fall back to otherside.js order on the whole text.
		const FString CharTag = First(MmlText, TEXT("(<m-character[^>]*>)"));
		FString Body = First(CharTag, TEXT("src\\s*=\\s*[\"']([^\"']*\\.glb[^\"']*?)[\"']"));
		if (Body.IsEmpty())
		{
			Body = First(MmlText, TEXT("src\\s*=\\s*[\"']([^\"']*\\.glb[^\"']*?)[\"']"));
		}
		if (Body.IsEmpty())
		{
			Body = First(MmlText, TEXT("url\\s*=\\s*[\"']([^\"']*\\.glb[^\"']*?)[\"']"));
		}
		if (Body.IsEmpty())
		{
			Body = First(MmlText, TEXT("(https?://[^\\s\"'<>]*\\.glb)"));
		}
		if (Body.IsEmpty())
		{
			return false;
		}
		OutBody = ResolveRelative(Body, MmlUrl);

		FRegexMatcher Models(FRegexPattern(TEXT("<m-model[^>]*?src\\s*=\\s*[\"']([^\"']*\\.glb[^\"']*?)[\"']"), ERegexPatternFlags::CaseInsensitive), MmlText);
		while (Models.FindNext())
		{
			const FString Gear = ResolveRelative(Models.GetCaptureGroup(1), MmlUrl);
			if (Gear != OutBody)
			{
				OutGear.AddUnique(Gear);
			}
		}
		return true;
	}
}
