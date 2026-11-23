#include "Controls/ControlsList.h"

#include "Controls/ControlsHelpData.h"
#include "InputAction.h"
#include "InputMappingContext.h"

namespace ControlsList
{
	static TArray<const UInputMappingContext*> GetContexts(const UControlsHelpData& Data)
	{
		TArray<const UInputMappingContext*> Contexts;
		for (const TObjectPtr<UInputMappingContext>& Context : Data.MappingContexts)
		{
			if (Context)
			{
				Contexts.AddUnique(Context.Get());
			}
		}
		return Contexts;
	}

	static FControlsKeyCap MakeCap(const FText& Text, const bool bDimmed)
	{
		FControlsKeyCap Cap;
		Cap.Text = Text;
		Cap.bDimmed = bDimmed;
		return Cap;
	}

	/** The row's keycaps: its actions' mapped keys, else its fixed keys, else its note, else "Unbound". */
	static TArray<FControlsKeyCap> MakeCaps(const UControlsHelpData& Data, const FControlsHelpRow& Row,
		const TArray<const UInputMappingContext*>& Contexts)
	{
		TArray<FKey> Keys;
		for (const TObjectPtr<UInputAction>& Action : Row.Actions)
		{
			if (Action)
			{
				for (const FKey& Key : FControlsList::GetMappedKeys(Contexts, *Action))
				{
					Keys.AddUnique(Key);
				}
			}
		}
		if (Row.Actions.IsEmpty())
		{
			Keys = Row.FixedKeys;
		}

		TArray<FControlsKeyCap> Caps;
		for (const FKey& Key : Keys)
		{
			Caps.Add(MakeCap(Data.GetKeyLabel(Key, &Row), false));
		}
		if (Caps.IsEmpty())
		{
			const bool bHasNote = Row.Actions.IsEmpty() && !Row.KeyNote.IsEmpty();
			Caps.Add(MakeCap(bHasNote ? Row.KeyNote : Data.UnboundKeyText, true));
		}
		return Caps;
	}
}

TArray<FControlsListGroup> FControlsList::Build(const UControlsHelpData& Data, TArray<const UInputAction*>* const OutUndescribed)
{
	const TArray<const UInputMappingContext*> Contexts = ControlsList::GetContexts(Data);

	TArray<FControlsListGroup> Groups;
	TArray<const UInputAction*> Described;
	for (const FControlsHelpGroup& HelpGroup : Data.Groups)
	{
		FControlsListGroup Group;
		Group.DisplayName = HelpGroup.DisplayName;
		for (const FControlsHelpRow& HelpRow : Data.Rows)
		{
			if (HelpRow.GroupId != HelpGroup.GroupId)
			{
				continue;
			}
			FControlsListRow Row;
			Row.DisplayName = HelpRow.DisplayName;
			Row.Description = FormatWithKeys(Data, HelpRow.Description);
			Row.bCombination = HelpRow.Join == EControlsKeyJoin::Combination;
			Row.Keys = ControlsList::MakeCaps(Data, HelpRow, Contexts);
			for (const TObjectPtr<UInputAction>& Action : HelpRow.Actions)
			{
				if (Action)
				{
					Row.Actions.AddUnique(Action.Get());
					Described.AddUnique(Action.Get());
				}
			}
			Group.Rows.Add(MoveTemp(Row));
		}
		if (!Group.Rows.IsEmpty())
		{
			Groups.Add(MoveTemp(Group));
		}
	}

	// A row naming a group that doesn't exist would vanish silently: say so.
	for (const FControlsHelpRow& HelpRow : Data.Rows)
	{
		if (!Data.Groups.ContainsByPredicate([&HelpRow](const FControlsHelpGroup& Each) { return Each.GroupId == HelpRow.GroupId; }))
		{
			UE_LOG(LogStartechControls, Warning, TEXT("[Controls] Row '%s' in %s names group '%s', which isn't in Groups: not shown."),
				*HelpRow.DisplayName.ToString(), *Data.GetName(), *HelpRow.GroupId.ToString());
		}
	}

	// Every mapped action shows, described or not.
	FControlsListGroup Other;
	Other.DisplayName = Data.OtherGroupName;
	for (const UInputAction* const Action : GetMappedActions(Contexts))
	{
		if (Described.Contains(Action))
		{
			continue;
		}
		FControlsListRow Row;
		Row.DisplayName = GetActionFallbackName(*Action);
		Row.bUndescribed = true;
		Row.Actions.Add(Action);
		for (const FKey& Key : GetMappedKeys(Contexts, *Action))
		{
			Row.Keys.Add(ControlsList::MakeCap(Data.GetKeyLabel(Key, nullptr), false));
		}
		UE_LOG(LogStartechControls, Warning, TEXT("[Controls] %s (%s) has no row in %s: listed under \"%s\" with no description."),
			*Action->GetName(), *DescribeKeys(Row), *Data.GetName(), *Data.OtherGroupName.ToString());
		if (OutUndescribed)
		{
			OutUndescribed->Add(Action);
		}
		Other.Rows.Add(MoveTemp(Row));
	}
	if (!Other.Rows.IsEmpty())
	{
		Groups.Add(MoveTemp(Other));
	}
	return Groups;
}

TArray<const UInputAction*> FControlsList::GetMappedActions(const TArray<const UInputMappingContext*>& Contexts)
{
	TArray<const UInputAction*> Actions;
	for (const UInputMappingContext* const Context : Contexts)
	{
		for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
		{
			if (Mapping.Action && Mapping.Key.IsValid())
			{
				Actions.AddUnique(Mapping.Action.Get());
			}
		}
	}
	return Actions;
}

TArray<FKey> FControlsList::GetMappedKeys(const TArray<const UInputMappingContext*>& Contexts, const UInputAction& Action)
{
	TArray<FKey> Keys;
	for (const UInputMappingContext* const Context : Contexts)
	{
		for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
		{
			if (Mapping.Action == &Action && Mapping.Key.IsValid())
			{
				Keys.AddUnique(Mapping.Key);
			}
		}
	}
	return Keys;
}

FText FControlsList::FormatWithKeys(const UControlsHelpData& Data, const FText& Text)
{
	TArray<FString> Names;
	FText::GetFormatPatternParameters(FTextFormat(Text), Names);
	if (Names.IsEmpty())
	{
		return Text;
	}

	// Every action the placeholders can name: the mapped ones, then any a row names (which may have no key).
	const TArray<const UInputMappingContext*> Contexts = ControlsList::GetContexts(Data);
	TArray<const UInputAction*> Actions = GetMappedActions(Contexts);
	for (const FControlsHelpRow& Row : Data.Rows)
	{
		for (const TObjectPtr<UInputAction>& Action : Row.Actions)
		{
			if (Action)
			{
				Actions.AddUnique(Action.Get());
			}
		}
	}

	FFormatNamedArguments Arguments;
	for (const FString& Name : Names)
	{
		const UInputAction* const* const Found = Actions.FindByPredicate([&Name](const UInputAction* const Each)
		{
			return GetActionFallbackName(*Each).ToString().Equals(Name, ESearchCase::IgnoreCase);
		});
		if (!Found)
		{
			UE_LOG(LogStartechControls, Warning, TEXT("[Controls] {%s} in \"%s\" names no Input Action in %s: left as written."),
				*Name, *Text.ToString(), *Data.GetName());
			Arguments.Add(Name, FText::FromString(FString::Printf(TEXT("{%s}"), *Name)));
			continue;
		}

		TArray<FString> Labels;
		for (const FKey& Key : GetMappedKeys(Contexts, **Found))
		{
			Labels.Add(Data.GetKeyLabel(Key, nullptr).ToString());
		}
		Arguments.Add(Name, Labels.IsEmpty() ? Data.UnboundKeyText : FText::FromString(FString::Join(Labels, TEXT(" / "))));
	}
	return FText::Format(FTextFormat(Text), Arguments);
}

FText FControlsList::GetActionFallbackName(const UInputAction& Action)
{
	FString Name = Action.GetName();
	Name.RemoveFromStart(TEXT("IA_"));
	return FText::FromString(Name);
}

FString FControlsList::DescribeKeys(const FControlsListRow& Row)
{
	TArray<FString> Parts;
	for (const FControlsKeyCap& Cap : Row.Keys)
	{
		Parts.Add(Cap.bDimmed ? FString::Printf(TEXT("[%s]"), *Cap.Text.ToString()) : Cap.Text.ToString());
	}
	return FString::Join(Parts, Row.bCombination ? TEXT(" + ") : TEXT(" / "));
}
