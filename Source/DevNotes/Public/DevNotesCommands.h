// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"
#include "DevNotesStyle.h"

class FDevNotesCommands : public TCommands<FDevNotesCommands>
{
public:

	FDevNotesCommands()
		: TCommands<FDevNotesCommands>(TEXT("DevNotes"), NSLOCTEXT("Contexts", "DevNotes", "DevNotes Plugin"), NAME_None, FDevNotesStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > PluginAction;
};
