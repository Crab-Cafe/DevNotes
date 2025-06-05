// Copyright Epic Games, Inc. All Rights Reserved.

#include "DevNotes.h"
#include "DevNotesStyle.h"
#include "DevNotesCommands.h"
#include "DevNoteSubsystem.h"
#include "LevelEditor.h"
#include "Misc/MessageDialog.h"
#include "ToolMenus.h"
#include "Widgets/SDevNotesDropdownWidget.h"

static const FName DevNotesTabName("DevNotes");

#define LOCTEXT_NAMESPACE "FDevNotesModule"

void FDevNotesModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	FDevNotesStyle::Initialize();
	FDevNotesStyle::ReloadTextures();

	// Register a function to be called when menu system is initialized
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(
		this, &FDevNotesModule::RegisterMenus));
}


void FDevNotesModule::RegisterMenus()
{

	FToolMenuOwnerScoped OwnerScoped(this);

	UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.User");
	FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("DevNotes");
	Section.AddEntry(FToolMenuEntry::InitWidget(
			"DevNotesButton",
			SNew(SComboButton)
			.OnGetMenuContent(FOnGetContent::CreateRaw(this, &FDevNotesModule::GenerateNotesDropdown))
			.ButtonContent()
			[
				SNew(STextBlock).Text(FText::FromString("Notes"))
			],
			FText::FromString("Developer Notes")
		));
}


void FDevNotesModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	FDevNotesStyle::Shutdown();
	FDevNotesCommands::Unregister();
}

TSharedRef<SWidget> FDevNotesModule::GenerateNotesDropdown()
{
	UDevNoteSubsystem* Subsystem = GEditor->GetEditorSubsystem<UDevNoteSubsystem>();
	Subsystem->RequestNotesFromServer();
	const auto& notes = Subsystem->GetNotes();
	TSharedRef<SDevNotesDropdownWidget> Widget = SNew(SDevNotesDropdownWidget);
	Widget->SetNotesSource(notes);
	
	return SNew(SBox)
	.WidthOverride(600)
	.HeightOverride(400)
	[
		Widget
	];
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FDevNotesModule, DevNotes)