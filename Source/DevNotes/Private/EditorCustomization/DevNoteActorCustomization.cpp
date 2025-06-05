#include "DevNoteActorCustomization.h"

#include "DevNoteActor.h"
#include "Widgets/SDevNoteEditor.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<IDetailCustomization> FDevNoteActorCustomization::MakeInstance()
{
	return MakeShareable(new FDevNoteActorCustomization);
}

void FDevNoteActorCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);

	if (Objects.Num() != 1) return;

	ADevNoteActor* DevNoteActor = Cast<ADevNoteActor>(Objects[0].Get());
	if (!DevNoteActor) return;
	
	TSharedPtr<FDevNote> DevNote = DevNoteActor->Note;
    
	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("DevNote");
	DetailBuilder.HideCategory("Replication");
	DetailBuilder.HideCategory("Collision");
	DetailBuilder.HideCategory("HLOD");
	DetailBuilder.HideCategory("Physics");
	DetailBuilder.HideCategory("Networking");
	DetailBuilder.HideCategory("Input");
	DetailBuilder.HideCategory("Actor");
	DetailBuilder.HideCategory("LevelInstance");
	DetailBuilder.HideCategory("Cooking");
	DetailBuilder.HideCategory("WorldPartition");
	DetailBuilder.HideCategory("DataLayers");
	DetailBuilder.HideCategory("Components");

	Category.AddCustomRow(NSLOCTEXT("DevNote", "DevNoteEditor", "Dev Note Editor")).ShouldAutoExpand()
	[
		SNew(SBox)
		.Padding(0, 10, 0, 0)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SDevNoteEditor)
			.SelectedNote(DevNote)
		]
	];
}