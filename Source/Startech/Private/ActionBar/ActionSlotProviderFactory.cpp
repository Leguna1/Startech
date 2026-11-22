#include "ActionBar/ActionSlotProviderFactory.h"

#include "ActionBar/AmmoSlotProvider.h"

UActionSlotProvider* FActionSlotProviderFactory::Create(const EActionSlotKind Kind, UObject& Outer)
{
	switch (Kind)
	{
	case EActionSlotKind::Ammo:
		return NewObject<UAmmoSlotProvider>(&Outer);
	}
	return nullptr;
}
