// Copyright Epic Games, Inc. All Rights Reserved.

#include "DevNotesCommands.h"

#define LOCTEXT_NAMESPACE "FDevNotesModule"

void FDevNotesCommands::RegisterCommands()
{
	UI_COMMAND(PluginAction, "DevNotes", "Execute DevNotes action", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
