// Copyright Epic Games, Inc. All Rights Reserved.

#include "DevNotes.h"

#include "DevNoteActor.h"
#include "DevNoteSubsystem.h"
#include "Misc/MessageDialog.h"
#include "ToolMenus.h"
#include "EditorCustomization/DevNoteActorCustomization.h"
#include "Widgets/SDevNotesDropdownWidget.h"

#define LOCTEXT_NAMESPACE "FDevNotesModule"

void FDevNotesModule::OnMapOpened(const FString& String, bool bArg)
{
	if (UDevNoteSubsystem* Subsystem = GEditor->GetEditorSubsystem<UDevNoteSubsystem>())
	{
		if (Subsystem->IsLoggedIn())
		{
			Subsystem->RequestNotesFromServer();
		}
	}
}

void FDevNotesModule::StartupModule()
{
	// Register toolbar menu when ready
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(
		this, &FDevNotesModule::RegisterMenus));

	// Custom details panel for ADevNoteActor 
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout(
		ADevNoteActor::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FDevNoteActorCustomization::MakeInstance));

	// Refresh notes whenever a new map is opened
	FEditorDelegates::OnMapOpened.AddRaw(this, &FDevNotesModule::OnMapOpened);
}


void FDevNotesModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	// Add a combo button dropdown to toolbar for custom DevNotes menu
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
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	FEditorDelegates::OnMapOpened.RemoveAll(this);
}


TSharedRef<SWidget> FDevNotesModule::GenerateNotesDropdown()
{
	if (UDevNoteSubsystem* Subsystem = GEditor->GetEditorSubsystem<UDevNoteSubsystem>())
	{
		// Refresh notes whenever opened
		Subsystem->RequestNotesFromServer();
		const auto& notes = Subsystem->GetNotes();

		// Cache and reuse a single notes widget
		if (NotesWidget == nullptr)
		{
			NotesWidget = SNew(SDevNotesDropdownWidget);
		}

		NotesWidget->SetNotesSource(notes);
	
		return SNew(SBox)
		.WidthOverride(600)
		.HeightOverride(400)
		[
			NotesWidget.ToSharedRef()
		];
	}
	
	return SNullWidget::NullWidget;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FDevNotesModule, DevNotes)