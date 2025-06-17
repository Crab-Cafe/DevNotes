// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FDevNote.h"
#include "Modules/ModuleManager.h"

class FToolBarBuilder;
class FMenuBuilder;

class FDevNotesModule : public IModuleInterface
{
public:
	void OnMapOpened(const FString& String, bool bArg);
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;


private:
	TSharedPtr<SButton> ToolButton;
	TSharedRef<SWidget> GenerateNotesDropdown();
	void RegisterMenus();
};
