// Copyright Epic Games, Inc. All Rights Reserved.

#include "DevNotes.h"
#include "DevNotesStyle.h"
#include "DevNotesCommands.h"
#include "DevNoteSubsystem.h"
#include "Misc/MessageDialog.h"
#include "ToolMenus.h"

static const FName DevNotesTabName("DevNotes");

#define LOCTEXT_NAMESPACE "FDevNotesModule"

void FDevNotesModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	FDevNotesStyle::Initialize();
	FDevNotesStyle::ReloadTextures();

	FDevNotesCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FDevNotesCommands::Get().PluginAction,
		FExecuteAction::CreateRaw(this, &FDevNotesModule::PluginButtonClicked),
		FCanExecuteAction());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FDevNotesModule::RegisterMenus));
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

void FDevNotesModule::PluginButtonClicked()
{
	auto ss = GEngine->GetEngineSubsystem<UDevNoteSubsystem>();
	FDevNote note;
	note.Title = "HEHE";
	note.Id = "000.000.0";
	ss->PushNote(note);
}

void FDevNotesModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
			Section.AddMenuEntryWithCommandList(FDevNotesCommands::Get().PluginAction, PluginCommands);
		}
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("PluginTools");
			{
				FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FDevNotesCommands::Get().PluginAction));
				Entry.SetCommandList(PluginCommands);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FDevNotesModule, DevNotes)