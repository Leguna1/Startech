#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Controls/ControlsHelpData.h"
#include "Controls/ControlsList.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "ShopQuantity.h"
#include "UObject/UnrealType.h"

namespace StartechControlsTests
{
	/**
	 * A transient mapping context (W and S on IA_TestThrottle, Tab on IA_TestLock, F on IA_TestInteract) and a
	 * DA_ControlsHelp naming only the first two: Throttle under Flight, Lock under Combat.
	 */
	struct FFixture
	{
		FFixture()
		{
			Throttle = NewObject<UInputAction>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UInputAction::StaticClass(), TEXT("IA_TestThrottle")));
			Lock = NewObject<UInputAction>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UInputAction::StaticClass(), TEXT("IA_TestLock")));
			Interact = NewObject<UInputAction>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UInputAction::StaticClass(), TEXT("IA_TestInteract")));

			Context = NewObject<UInputMappingContext>(GetTransientPackage());
			Context->MapKey(Throttle, EKeys::W);
			Context->MapKey(Throttle, EKeys::S);
			Context->MapKey(Lock, EKeys::Tab);
			Context->MapKey(Interact, EKeys::F);

			Data = NewObject<UControlsHelpData>(GetTransientPackage());
			Data->MappingContexts.Add(Context);
			Data->OtherGroupName = FText::FromString(TEXT("Other"));
			Data->UnboundKeyText = FText::FromString(TEXT("Unbound"));
			AddGroup(TEXT("Flight"));
			AddGroup(TEXT("Combat"));
			AddRow(TEXT("Flight"), TEXT("Throttle"), { Throttle });
			AddRow(TEXT("Combat"), TEXT("Lock target"), { Lock });

			for (UObject* const Object : TArray<UObject*>{ Throttle, Lock, Interact, Context, Data })
			{
				Object->AddToRoot();
			}
		}

		~FFixture()
		{
			for (UObject* const Object : TArray<UObject*>{ Throttle, Lock, Interact, Context, Data })
			{
				Object->RemoveFromRoot();
			}
		}

		void AddGroup(const TCHAR* const Name) const
		{
			FControlsHelpGroup Group;
			Group.GroupId = Name;
			Group.DisplayName = FText::FromString(Name);
			Data->Groups.Add(Group);
		}

		FControlsHelpRow& AddRow(const TCHAR* const Group, const TCHAR* const Name, const TArray<UInputAction*>& Actions) const
		{
			FControlsHelpRow Row;
			Row.GroupId = Group;
			Row.DisplayName = FText::FromString(Name);
			Row.Description = FText::FromString(FString::Printf(TEXT("What %s does."), Name));
			for (UInputAction* const Action : Actions)
			{
				Row.Actions.Add(Action);
			}
			return Data->Rows.Add_GetRef(Row);
		}

		UInputAction* Throttle = nullptr;
		UInputAction* Lock = nullptr;
		UInputAction* Interact = nullptr;
		UInputMappingContext* Context = nullptr;
		UControlsHelpData* Data = nullptr;
	};

	/** The row named Name, or null. */
	static const FControlsListRow* FindRow(const TArray<FControlsListGroup>& Groups, const FString& Name, FString* OutGroup = nullptr)
	{
		for (const FControlsListGroup& Group : Groups)
		{
			for (const FControlsListRow& Row : Group.Rows)
			{
				if (Row.DisplayName.ToString() == Name)
				{
					if (OutGroup)
					{
						*OutGroup = Group.DisplayName.ToString();
					}
					return &Row;
				}
			}
		}
		return nullptr;
	}

	/** True if some row lists Action. */
	static bool IsListed(const TArray<FControlsListGroup>& Groups, const UInputAction* const Action)
	{
		for (const FControlsListGroup& Group : Groups)
		{
			for (const FControlsListRow& Row : Group.Rows)
			{
				if (Row.Actions.Contains(Action))
				{
					return true;
				}
			}
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStartechControlsEveryActionTest, "Startech.Controls.ListsEveryMappedAction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FStartechControlsEveryActionTest::RunTest(const FString& Parameters)
{
	using namespace StartechControlsTests;
	AddExpectedMessagePlain(TEXT("has no row in"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);

	const FFixture Fixture;
	const TArray<FControlsListGroup> Groups = FControlsList::Build(*Fixture.Data);

	const TArray<const UInputAction*> Mapped = FControlsList::GetMappedActions({ Fixture.Context });
	TestEqual(TEXT("three actions are mapped"), Mapped.Num(), 3);
	for (const UInputAction* const Action : Mapped)
	{
		TestTrue(FString::Printf(TEXT("%s is listed"), *Action->GetName()), IsListed(Groups, Action));
	}

	TestEqual(TEXT("groups: Flight, Combat, Other"), Groups.Num(), 3);
	if (Groups.Num() == 3)
	{
		TestEqual(TEXT("first group"), Groups[0].DisplayName.ToString(), FString(TEXT("Flight")));
		TestEqual(TEXT("second group"), Groups[1].DisplayName.ToString(), FString(TEXT("Combat")));
		TestEqual(TEXT("last group"), Groups[2].DisplayName.ToString(), FString(TEXT("Other")));
	}

	const FControlsListRow* const Throttle = FindRow(Groups, TEXT("Throttle"));
	if (TestNotNull(TEXT("Throttle row"), Throttle))
	{
		TestEqual(TEXT("both of the action's keys, in mapping order"), FControlsList::DescribeKeys(*Throttle), FString(TEXT("W / S")));
		TestEqual(TEXT("description from the data"), Throttle->Description.ToString(), FString(TEXT("What Throttle does.")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStartechControlsUndescribedTest, "Startech.Controls.UndescribedActionStillShows",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FStartechControlsUndescribedTest::RunTest(const FString& Parameters)
{
	using namespace StartechControlsTests;
	AddExpectedMessagePlain(TEXT("has no row in"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);

	const FFixture Fixture;
	TArray<const UInputAction*> Undescribed;
	const TArray<FControlsListGroup> Groups = FControlsList::Build(*Fixture.Data, &Undescribed);

	TestEqual(TEXT("one undescribed action"), Undescribed.Num(), 1);
	TestTrue(TEXT("it is IA_TestInteract"), Undescribed.Contains(Fixture.Interact));

	const FString Name = FControlsList::GetActionFallbackName(*Fixture.Interact).ToString();
	TestTrue(TEXT("its name is the asset name without IA_"), Name.StartsWith(TEXT("TestInteract")));

	FString Group;
	const FControlsListRow* const Row = FindRow(Groups, Name, &Group);
	if (TestNotNull(TEXT("the undescribed action has a row"), Row))
	{
		TestEqual(TEXT("under Other"), Group, FString(TEXT("Other")));
		TestTrue(TEXT("marked undescribed"), Row->bUndescribed);
		TestEqual(TEXT("with its key"), FControlsList::DescribeKeys(*Row), FString(TEXT("F")));
		TestTrue(TEXT("and no description"), Row->Description.IsEmpty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStartechControlsKeyChangeTest, "Startech.Controls.KeyChangeShows",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FStartechControlsKeyChangeTest::RunTest(const FString& Parameters)
{
	using namespace StartechControlsTests;
	AddExpectedMessagePlain(TEXT("has no row in"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 2);

	const FFixture Fixture;
	const TArray<FControlsListGroup> BeforeGroups = FControlsList::Build(*Fixture.Data);
	const FControlsListRow* const Before = FindRow(BeforeGroups, TEXT("Throttle"));
	TestEqual(TEXT("before"), Before ? FControlsList::DescribeKeys(*Before) : FString(), FString(TEXT("W / S")));

	// Rebind forward from W to Up in the mapping context: the next build shows it, with the engine's key name.
	Fixture.Context->UnmapKey(Fixture.Throttle, EKeys::W);
	Fixture.Context->MapKey(Fixture.Throttle, EKeys::Up);
	const TArray<FControlsListGroup> After = FControlsList::Build(*Fixture.Data);
	const FControlsListRow* const Row = FindRow(After, TEXT("Throttle"));
	const FString Expected = FString::Printf(TEXT("S / %s"), *EKeys::Up.GetDisplayName(false).ToString());
	TestEqual(TEXT("after the key change"), Row ? FControlsList::DescribeKeys(*Row) : FString(), Expected);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStartechControlsKeyLabelsTest, "Startech.Controls.KeyLabelsAndNotes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FStartechControlsKeyLabelsTest::RunTest(const FString& Parameters)
{
	using namespace StartechControlsTests;
	AddExpectedMessagePlain(TEXT("has no row in"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);

	const FFixture Fixture;
	FControlsKeyLabel Short;
	Short.Key = EKeys::W;
	Short.Label = FText::FromString(TEXT("Fwd"));
	Fixture.Data->KeyLabels.Add(Short);

	FControlsHelpRow& Combo = Fixture.AddRow(TEXT("Combat"), TEXT("Trade max"), {});
	Combo.FixedKeys = { EKeys::LeftShift, EKeys::LeftMouseButton };
	Combo.Join = EControlsKeyJoin::Combination;
	FControlsKeyLabel Click;
	Click.Key = EKeys::LeftMouseButton;
	Click.Label = FText::FromString(TEXT("click"));
	Combo.RowKeyLabels.Add(Click);

	Fixture.AddRow(TEXT("Flight"), TEXT("Dock"), {}).KeyNote = FText::FromString(TEXT("Fly in"));

	UInputAction* const Unmapped = NewObject<UInputAction>(GetTransientPackage(),
		MakeUniqueObjectName(GetTransientPackage(), UInputAction::StaticClass(), TEXT("IA_TestUnmapped")));
	Fixture.AddRow(TEXT("Flight"), TEXT("Nothing"), { Unmapped });

	const TArray<FControlsListGroup> Groups = FControlsList::Build(*Fixture.Data);
	const auto Keys = [&Groups](const TCHAR* const Name)
	{
		const FControlsListRow* const Row = FindRow(Groups, Name);
		return Row ? FControlsList::DescribeKeys(*Row) : FString(TEXT("(missing)"));
	};

	TestEqual(TEXT("a short label replaces the engine name"), Keys(TEXT("Throttle")), FString(TEXT("Fwd / S")));
	TestEqual(TEXT("engine names are the fallback; row labels win; joined with +"), Keys(TEXT("Trade max")),
		FString::Printf(TEXT("%s + click"), *EKeys::LeftShift.GetDisplayName(false).ToString()));
	TestEqual(TEXT("a keyless row shows its note, dimmed"), Keys(TEXT("Dock")), FString(TEXT("[Fly in]")));
	TestEqual(TEXT("an action with no key shows Unbound, dimmed"), Keys(TEXT("Nothing")), FString(TEXT("[Unbound]")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStartechControlsPlaceholderTest, "Startech.Controls.DescriptionPlaceholders",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FStartechControlsPlaceholderTest::RunTest(const FString& Parameters)
{
	using namespace StartechControlsTests;
	AddExpectedMessagePlain(TEXT("has no row in"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 2);
	AddExpectedMessagePlain(TEXT("{NoSuchAction} in"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);

	const FFixture Fixture;
	const FString Lock = FControlsList::GetActionFallbackName(*Fixture.Lock).ToString();
	const FString Throttle = FControlsList::GetActionFallbackName(*Fixture.Throttle).ToString();
	FControlsHelpRow& Row = Fixture.AddRow(TEXT("Combat"), TEXT("Placeholders"), {});
	Row.KeyNote = FText::FromString(TEXT("Auto"));
	Row.Description = FText::FromString(FString::Printf(TEXT("Press {%s} to lock; {%s} moves."), *Lock, *Throttle));

	const auto Description = [&Fixture]()
	{
		const TArray<FControlsListGroup> Groups = FControlsList::Build(*Fixture.Data);
		const FControlsListRow* const Found = FindRow(Groups, TEXT("Placeholders"));
		return Found ? Found->Description.ToString() : FString(TEXT("(missing)"));
	};

	TestEqual(TEXT("each placeholder shows its action's keys"), Description(),
		FString::Printf(TEXT("Press %s to lock; W / S moves."), *EKeys::Tab.GetDisplayName(false).ToString()));

	// Rebinding the lock changes the text on the next build.
	Fixture.Context->UnmapKey(Fixture.Lock, EKeys::Tab);
	Fixture.Context->MapKey(Fixture.Lock, EKeys::Q);
	TestEqual(TEXT("after a rebind"), Description(), FString(TEXT("Press Q to lock; W / S moves.")));

	// An unknown name stays as written, with a warning.
	const FText Unknown = FControlsList::FormatWithKeys(*Fixture.Data, FText::FromString(TEXT("Press {NoSuchAction}.")));
	TestEqual(TEXT("unknown placeholder kept"), Unknown.ToString(), FString(TEXT("Press {NoSuchAction}.")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStartechControlsRealDataTest, "Startech.Controls.RealDataCoversEveryInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FStartechControlsRealDataTest::RunTest(const FString& Parameters)
{
	const UControlsHelpData* const Data = UControlsHelpData::Load();
	if (!TestNotNull(TEXT("UStartechSettings::ControlsHelp loads"), Data))
	{
		return false;
	}

	// The context the ship actually adds must be one the screen reads.
	const UClass* const ShipClass = LoadClass<UObject>(nullptr, TEXT("/Game/Blueprints/BP_ShipPawn.BP_ShipPawn_C"));
	const FObjectProperty* const ContextProperty = ShipClass ? FindFProperty<FObjectProperty>(ShipClass, TEXT("MappingContext")) : nullptr;
	const UObject* const ShipContext = ContextProperty ? ContextProperty->GetObjectPropertyValue_InContainer(ShipClass->GetDefaultObject()) : nullptr;
	if (TestNotNull(TEXT("BP_ShipPawn's MappingContext"), ShipContext))
	{
		TestTrue(TEXT("DA_ControlsHelp reads BP_ShipPawn's mapping context"),
			Data->MappingContexts.ContainsByPredicate([ShipContext](const TObjectPtr<UInputMappingContext>& Each) { return Each == ShipContext; }));
	}

	TArray<const UInputAction*> Undescribed;
	const TArray<FControlsListGroup> Groups = FControlsList::Build(*Data, &Undescribed);
	for (const UInputAction* const Action : Undescribed)
	{
		AddError(FString::Printf(TEXT("%s is mapped but has no row in DA_ControlsHelp"), *Action->GetName()));
	}

	const TArray<FString> ExpectedGroups = { TEXT("Flight"), TEXT("Combat"), TEXT("Action bar"), TEXT("Station and interface"), TEXT("Camera") };
	TestEqual(TEXT("five groups"), Groups.Num(), ExpectedGroups.Num());
	for (int32 Index = 0; Index < FMath::Min(Groups.Num(), ExpectedGroups.Num()); ++Index)
	{
		TestEqual(FString::Printf(TEXT("group %d"), Index), Groups[Index].DisplayName.ToString(), ExpectedGroups[Index]);
	}
	for (const FControlsListGroup& Group : Groups)
	{
		for (const FControlsListRow& Row : Group.Rows)
		{
			const FString Name = Row.DisplayName.ToString();
			TestFalse(FString::Printf(TEXT("'%s' has a name"), *Name), Name.IsEmpty());
			TestFalse(FString::Printf(TEXT("'%s' has a description"), *Name), Row.Description.IsEmpty());
			TestFalse(FString::Printf(TEXT("'%s' has a key"), *Name), FControlsList::DescribeKeys(Row).Contains(Data->UnboundKeyText.ToString()));
			TestFalse(FString::Printf(TEXT("'%s' has no unreplaced placeholder"), *Name), Row.Description.ToString().Contains(TEXT("{")));
		}
	}
	TestEqual(TEXT("four tips"), Data->Tips.Num(), 4);
	for (const FText& Tip : Data->Tips)
	{
		TestFalse(TEXT("tip has no unreplaced placeholder"), FControlsList::FormatWithKeys(*Data, Tip).ToString().Contains(TEXT("{")));
	}

	// The station hub row names the Interact key through {Interact}, never a typed-in letter.
	const FControlsHelpRow* const Hub = Data->Rows.FindByPredicate([](const FControlsHelpRow& Each)
	{
		return Each.Description.ToString().Contains(TEXT("{Interact}"));
	});
	TestNotNull(TEXT("a row uses {Interact}"), Hub);

	// The shop's quantity keys are handled in C++ (FShopQuantity), so the data's copy must match it, in order.
	const TArray<EShopQuantity>& Quantities = FShopQuantity::All();
	const bool bShopRowMatches = Data->Rows.ContainsByPredicate([&Quantities](const FControlsHelpRow& Row)
	{
		if (!Row.Actions.IsEmpty() || Row.FixedKeys.Num() != Quantities.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < Quantities.Num(); ++Index)
		{
			EShopQuantity Quantity;
			if (!FShopQuantity::FromKey(Row.FixedKeys[Index], Quantity) || Quantity != Quantities[Index])
			{
				return false;
			}
		}
		return true;
	});
	TestTrue(TEXT("a row lists the shop quantity keys exactly as FShopQuantity reads them"), bShopRowMatches);
	return true;
}

#endif
