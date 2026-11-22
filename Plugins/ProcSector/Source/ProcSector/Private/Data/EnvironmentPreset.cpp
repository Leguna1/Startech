#include "Data/EnvironmentPreset.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"

namespace
{
	/** Adds an error and returns false when Value lies outside 0 to 1. Owner and Name label the message. */
	bool CheckFraction(FDataValidationContext& Context, const FString& Owner, const TCHAR* Name, const float Value)
	{
		if (Value >= 0.0f && Value <= 1.0f)
		{
			return true;
		}
		Context.AddError(FText::FromString(FString::Printf(TEXT("%s: %s is %.3f; it must be within 0 to 1."), *Owner, Name, Value)));
		return false;
	}

	/** Checks the three fractions of a zone or asteroid field and that OuterFraction is not below InnerFraction. Returns false on any error. */
	bool CheckFractions(FDataValidationContext& Context, const FString& Owner, const float InnerFraction, const float OuterFraction, const float HeightFraction)
	{
		bool bValid = CheckFraction(Context, Owner, TEXT("InnerFraction"), InnerFraction);
		bValid &= CheckFraction(Context, Owner, TEXT("OuterFraction"), OuterFraction);
		bValid &= CheckFraction(Context, Owner, TEXT("HeightFraction"), HeightFraction);

		if (OuterFraction < InnerFraction)
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("%s: OuterFraction is smaller than InnerFraction."), *Owner)));
			bValid = false;
		}
		return bValid;
	}
}

EDataValidationResult UEnvironmentPreset::IsDataValid(FDataValidationContext& Context) const
{
	bool bValid = Super::IsDataValid(Context) != EDataValidationResult::Invalid;

	const FAsteroidFieldParams& Asteroids = BaseParams.Asteroids;
	if (!CheckFractions(Context, TEXT("Asteroids"), Asteroids.InnerFraction, Asteroids.OuterFraction, Asteroids.HeightFraction))
	{
		bValid = false;
	}
	if (Asteroids.MaxScale < Asteroids.MinScale)
	{
		Context.AddError(FText::FromString(TEXT("Asteroids: MaxScale is smaller than MinScale.")));
		bValid = false;
	}

	for (int32 TypeIndex = 0; TypeIndex < Asteroids.Types.Num(); ++TypeIndex)
	{
		const FAsteroidTypeParams& Type = Asteroids.Types[TypeIndex];
		if (Type.Mesh.IsNull())
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("Asteroids: type %d has no Mesh."), TypeIndex)));
			bValid = false;
		}
		if (Type.Weight <= 0.0f)
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("Asteroids: type %d has a Weight of %.3f; it must be above 0."), TypeIndex, Type.Weight)));
			bValid = false;
		}
		if (Type.MaxScale < Type.MinScale)
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("Asteroids: type %d has MaxScale smaller than MinScale."), TypeIndex)));
			bValid = false;
		}
	}

	for (const FZoneSpawnTable& Zone : BaseParams.Zones)
	{
		const FString ZoneLabel = FString::Printf(TEXT("Zone '%s'"), *Zone.ZoneName.ToString());
		if (!CheckFractions(Context, ZoneLabel, Zone.InnerFraction, Zone.OuterFraction, Zone.HeightFraction))
		{
			bValid = false;
		}

		for (const FZoneSpawnEntry& Entry : Zone.Entries)
		{
			if (Entry.ActorClass.IsNull())
			{
				Context.AddError(FText::FromString(FString::Printf(
					TEXT("Zone '%s': a spawn entry has no ActorClass."), *Zone.ZoneName.ToString())));
				bValid = false;
			}
			if (Entry.MaxCount < Entry.MinCount)
			{
				Context.AddError(FText::FromString(FString::Printf(
					TEXT("Zone '%s': a spawn entry has MaxCount below MinCount."), *Zone.ZoneName.ToString())));
				bValid = false;
			}
			if (Entry.RespawnDelay > 0.0f && Entry.bRememberWhenConsumed)
			{
				Context.AddError(FText::FromString(FString::Printf(
					TEXT("Zone '%s': a spawn entry has both RespawnDelay and Remember When Consumed. Choose one; respawning entries are not remembered."),
					*Zone.ZoneName.ToString())));
				bValid = false;
			}
			if (Entry.RespawnOffsetMax < Entry.RespawnOffsetMin)
			{
				Context.AddError(FText::FromString(FString::Printf(
					TEXT("Zone '%s': a spawn entry has RespawnOffsetMax below RespawnOffsetMin."), *Zone.ZoneName.ToString())));
				bValid = false;
			}
			if (Entry.RespawnDelay < 0.0f || Entry.RespawnOffsetMin < 0.0f || Entry.RespawnOffsetMax < 0.0f
				|| Entry.MinDistanceFromPlayer < 0.0f || Entry.MinSpacing < 0.0f)
			{
				Context.AddError(FText::FromString(FString::Printf(
					TEXT("Zone '%s': a spawn entry has a negative RespawnDelay, RespawnOffsetMin, RespawnOffsetMax, MinDistanceFromPlayer or MinSpacing."),
					*Zone.ZoneName.ToString())));
				bValid = false;
			}
		}
	}

	return bValid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif
