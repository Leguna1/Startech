#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Flow/CallsignRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStartechCallsignRulesTest, "Startech.Flow.CallsignRules",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

/** 3 to 16 letters, digits and single spaces, after trimming. */
bool FStartechCallsignRulesTest::RunTest(const FString& Parameters)
{
	const auto Valid = [](const TCHAR* const Raw) { return FCallsignRules::IsValid(FCallsignRules::Normalize(Raw)); };

	TestTrue(TEXT("Plain name"), Valid(TEXT("Ace")));
	TestTrue(TEXT("Digits and a space"), Valid(TEXT("Red Fox 7")));
	TestTrue(TEXT("Trimmed"), Valid(TEXT("   Nova   ")));
	TestTrue(TEXT("Exactly 16"), Valid(TEXT("ABCDEFGHIJKLMNOP")));
	TestTrue(TEXT("Accented letter"), Valid(TEXT("Zoë")));

	TestFalse(TEXT("Empty"), Valid(TEXT("")));
	TestFalse(TEXT("Too short"), Valid(TEXT("Al")));
	TestFalse(TEXT("Too short once trimmed"), Valid(TEXT("  Al  ")));
	TestFalse(TEXT("17 characters"), Valid(TEXT("ABCDEFGHIJKLMNOPQ")));
	TestFalse(TEXT("Double space"), Valid(TEXT("Red  Fox")));
	TestFalse(TEXT("Punctuation"), Valid(TEXT("Ace!")));
	TestFalse(TEXT("Underscore"), Valid(TEXT("Red_Fox")));
	TestFalse(TEXT("Tab"), Valid(TEXT("Red\tFox")));

	TestEqual(TEXT("Normalize trims"), FCallsignRules::Normalize(TEXT("  Red Fox ")), FString(TEXT("Red Fox")));
	return true;
}

#endif
