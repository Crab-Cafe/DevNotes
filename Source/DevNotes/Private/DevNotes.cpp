// Copyright Epic Games, Inc. All Rights Reserved.

#include "DevNotes.h"

#include "DevNoteActor.h"
#include "DevNotesStyle.h"
#include "DevNotesCommands.h"
#include "DevNoteSubsystem.h"
#include "LevelEditor.h"
#include "Misc/MessageDialog.h"
#include "ToolMenus.h"
#include "EditorCustomization/DevNoteActorCustomization.h"
#include "Widgets/SDevNotesDropdownWidget.h"
#include "DevNoteSubsystem.h"

static const FName DevNotesTabName("DevNotes");

#define LOCTEXT_NAMESPACE "FDevNotesModule"

void FDevNotesModule::OnMapOpened(const FString& String, bool bArg)
{
	if (UDevNoteSubsystem* Subsystem = GEditor->GetEditorSubsystem<UDevNoteSubsystem>())
	{
		Subsystem->RequestNotesFromServer();
	}
}

void FDevNotesModule::StartupModule()
{
	FDevNotesStyle::Initialize();
	FDevNotesStyle::ReloadTextures();

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(
		this, &FDevNotesModule::RegisterMenus));

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout(
		ADevNoteActor::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FDevNoteActorCustomization::MakeInstance));

	FEditorDelegates::OnMapOpened.AddRaw(this, &FDevNotesModule::OnMapOpened);
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
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	FDevNotesStyle::Shutdown();
	FDevNotesCommands::Unregister();

	FEditorDelegates::OnMapOpened.RemoveAll(this);
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