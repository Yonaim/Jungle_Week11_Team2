#pragma once
#include "Core/CoreTypes.h"

class FInputModifier;
class FInputTrigger;
struct FInputAction;

enum class EInputModifierMatch : uint8
{
	DontCare,
	Required,
	Blocked,
};

struct FInputModifierRequirements
{
	EInputModifierMatch Ctrl = EInputModifierMatch::DontCare;
	EInputModifierMatch Alt = EInputModifierMatch::DontCare;
	EInputModifierMatch Shift = EInputModifierMatch::DontCare;
};

// Struct : ActionKey Mapping
// Fuction : Store Mapping information of action, key , Trigger, Modifier 
struct FActionKeyMapping
{
	FInputAction* Action = nullptr;
	int32 Key = 0;
	TArray<FInputTrigger*> Triggers;  
	TArray<FInputModifier*> Modifiers;
	FInputModifierRequirements ModifierRequirements;

	FActionKeyMapping& RequireCtrl() { ModifierRequirements.Ctrl = EInputModifierMatch::Required; return *this; }
	FActionKeyMapping& RequireAlt() { ModifierRequirements.Alt = EInputModifierMatch::Required; return *this; }
	FActionKeyMapping& RequireShift() { ModifierRequirements.Shift = EInputModifierMatch::Required; return *this; }
	FActionKeyMapping& BlockCtrl() { ModifierRequirements.Ctrl = EInputModifierMatch::Blocked; return *this; }
	FActionKeyMapping& BlockAlt() { ModifierRequirements.Alt = EInputModifierMatch::Blocked; return *this; }
	FActionKeyMapping& BlockShift() { ModifierRequirements.Shift = EInputModifierMatch::Blocked; return *this; }
};
// Struct : Input Mapping Context
// Fuction : Store Mapping of Action and Key, Trigger, Modifier
struct FInputMappingContext
{
	FString ContextName;
	TArray<FActionKeyMapping> Mappings;

	FActionKeyMapping& AddMapping(FInputAction* Action, int32 Key);

};
