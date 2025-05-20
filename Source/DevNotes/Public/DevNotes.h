// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FDevNote.h"
#include "Modules/ModuleManager.h"

class FToolBarBuilder;
class FMenuBuilder;

class FDevNotesModule : public IModuleInterface
{
public:
	
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;


private:
	TSharedRef<SWidget> GenerateNotesDropdown();
	void RegisterMenus();

};
