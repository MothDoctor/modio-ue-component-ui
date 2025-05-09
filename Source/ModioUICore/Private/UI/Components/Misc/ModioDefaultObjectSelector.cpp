/*
 *  Copyright (C) 2024 mod.io Pty Ltd. <https://mod.io>
 *
 *  This file is part of the mod.io UE Plugin.
 *
 *  Distributed under the MIT License. (See accompanying file LICENSE or
 *   view online at <https://github.com/modio/modio-ue/blob/main/LICENSE>)
 *
 */

#include "UI/Components/Misc/ModioDefaultObjectSelector.h"

#include "TimerManager.h"
#include "UI/Components/Slate/SModioDataSourceAwareTableRow.h"
#include "UI/Interfaces/IModioModTagUIDetails.h"
#include "UI/Interfaces/IModioUIClickableWidget.h"
#include "UI/Interfaces/IModioUISelectableWidget.h"
#if WITH_EDITOR
	#include "Editor/WidgetCompilerLog.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModioDefaultObjectSelector)

UUserWidget* UModioDefaultObjectSelector::GetEntryWidgetFromItem(const UObject* Item) const
{
	return Item ? ITypedUMGListView<UObject*>::GetEntryWidgetFromItem<UUserWidget>(const_cast<UObject*>(Item)) : nullptr;
}

void UModioDefaultObjectSelector::RemoveSelectionChangedHandler_Implementation(
	const FModioOnObjectSelectionChanged& Handler)
{
	OnSelectedValueChanged.Remove(Handler);
}

void UModioDefaultObjectSelector::AddSelectionChangedHandler_Implementation(
	const FModioOnObjectSelectionChanged& Handler)
{
	OnSelectedValueChanged.AddUnique(Handler);
}

void UModioDefaultObjectSelector::SetSingleSelectionByValue_Implementation(UObject* Value, bool bEmitSelectionEvent)
{
	EmitSelectionEvents.Push(bEmitSelectionEvent);
	SetSelectedItem(Value);
	NotifySelectionChanged(Value);
	EmitSelectionEvents.Pop();
}

void UModioDefaultObjectSelector::SetSingleSelectionByIndex_Implementation(int32 Index, bool bEmitSelectionEvent)
{
	EmitSelectionEvents.Push(bEmitSelectionEvent);
	SetSelectedIndex(Index);
	NotifySelectionChanged(GetItemAt(Index));
	EmitSelectionEvents.Pop();
}

void UModioDefaultObjectSelector::ClearSelectedValues_Implementation()
{
	ClearSelection();
	if (GetSelectionMode() == ESelectionMode::Single)
	{
		// Sometimes the selection is not fully cleared by ClearSelection for the displayed entries
		// so we need to manually clear the selection state for each displayed entry
		for (UUserWidget* CurrentWidget : GetDisplayedEntryWidgets())
		{
			IUserListEntry::UpdateItemSelection(*CurrentWidget, false);
		}
	}
	NotifySelectionChanged(nullptr);
}

UObject* UModioDefaultObjectSelector::GetSingleSelectedValue_Implementation()
{
	return GetSelectedItem();
}

int32 UModioDefaultObjectSelector::GetSingleSelectionIndex_Implementation()
{
	return GetIndexForItem(GetSelectedItem());
}

void UModioDefaultObjectSelector::SetValues_Implementation(const TArray<UObject*>& InValues)
{
	ClearListItems();
	ClearSelection();
	if (IModioUIObjectSelector::Execute_GetMultiSelectionAllowed(this) && MultipleSelectionListItemClass)
	{
		IModioUIObjectSelector::Execute_SetListEntryWidgetClass(this, MultipleSelectionListItemClass);
	}
	SetListItems(InValues);
	RegenerateAllEntries();
}
int32 UModioDefaultObjectSelector::GetNumEntries_Implementation()
{
	return GetNumItems();
}

#if WITH_EDITOR
void UModioDefaultObjectSelector::ValidateCompiledDefaults(IWidgetCompilerLog& CompileLog) const
{
	Super::ValidateCompiledDefaults(CompileLog);
	if (EntryWidgetClass)
	{
		if (!EntryWidgetClass->ImplementsInterface(UModioUIClickableWidget::StaticClass()))
		{
			CompileLog.Error(FText::Format(
				FTextFormat::FromString(
					"'{0}' has EntryWidgetClass property set to'{1}' which does not implement ModioUIClickableWidget."),
				FText::FromString(GetName()), FText::FromString(EntryWidgetClass->GetName())));
		}
		if (!EntryWidgetClass->ImplementsInterface(UModioUISelectableWidget::StaticClass()))
		{
			CompileLog.Error(FText::Format(FTextFormat::FromString("'{0}' has EntryWidgetClass property set to'{1}' "
																   "which does not implement ModioUISelectableWidget."),
										   FText::FromString(GetName()),
										   FText::FromString(EntryWidgetClass->GetName())));
		}
	}
}

#endif

void UModioDefaultObjectSelector::OnEntryWidgetClicked(UObject* Widget)
{
	// Possibly need to check to make sure the base impl of this is called?
	NativeOnEntryWidgetClicked(Widget);
}

void UModioDefaultObjectSelector::NativeOnEntryWidgetClicked(UObject* Widget)
{
	if (UUserWidget* ConcreteWidget = Cast<UUserWidget>(Widget))
	{
		if (UObject* ListItem = UUserObjectListEntryLibrary::GetListItemObject(ConcreteWidget))
		{
			if(!GetMultiSelectionAllowed_Implementation())
			{
				SetSelectedItem(ListItem);
				NotifySelectionChanged(ListItem);
			}
			else
			{
				if(GetSelectedValues_Implementation().Contains(ListItem))
				{
					SetSelectedStateForValue_Implementation(ListItem, false, true);
				}
				else
				{
					SetSelectedStateForValue_Implementation(ListItem, true, true);
				}
			}
		}
	}
}

UUserWidget& UModioDefaultObjectSelector::OnGenerateEntryWidgetInternal(UObject* Item,
																		TSubclassOf<UUserWidget> DesiredEntryClass,
																		const TSharedRef<STableViewBase>& OwnerTable)
{
	UUserWidget& GeneratedEntryWidget = [this, DesiredEntryClass, OwnerTable, Item]() -> UUserWidget&
	{
		if (DesiredEntryClass->ImplementsInterface(UModioUIDataSourceWidget::StaticClass()))
		{
			return GenerateTypedEntry<UUserWidget, SModioDataSourceAwareTableRow<UObject*>>(DesiredEntryClass, OwnerTable);
		}
		return Super::OnGenerateEntryWidgetInternal(Item, DesiredEntryClass, OwnerTable);
	}();
	
	// Ensure the entry widget's underlying Slate structure has been initialized
	GeneratedEntryWidget.TakeWidget();
	if (GeneratedEntryWidget.GetClass()->ImplementsInterface(UModioUIClickableWidget::StaticClass()))
	{
		// Enable click events on the entry widget and bind a handler for click events
		IModioUIClickableWidget::Execute_EnableClick(&GeneratedEntryWidget);
		FModioClickableOnClicked ClickedDelegate;
		ClickedDelegate.BindUFunction(this, FName("OnEntryWidgetClicked"));
		IModioUIClickableWidget::Execute_AddClickedHandler(&GeneratedEntryWidget, ClickedDelegate);
	}

	if (GeneratedEntryWidget.GetClass()->ImplementsInterface(UModioUISelectableWidget::StaticClass()))
	{
		// Ensure that the entry widget is selectable so that it can potentially display selection feedback etc
		IModioUISelectableWidget::Execute_SetSelectable(&GeneratedEntryWidget, true);
	}

	// There's a bug in the engine where, when regenerating the entries of a list view while the outer widget is cached via UCommonActivatableWidgetContainerBase
	// (such as when the outer widget is added using the "Push Widget" function), the selection state of the previously selected item is reset without appropriate necessity.
	// This happens even though the entry widget itself maintains the correct selected state.
	// This issue has only been reproduced with the tag entries in the filter panel.
	if (Item && Item->Implements<UModioModTagUIDetails>() && IModioModTagUIDetails::Execute_GetSelectionState(Item))
	{
		if (UWorld* World = GetWorld())
		{
			// Defer the deselection of the item to the next tick so the correct state can be applied 
			// after STableViewBase::Tick improperly updates the selection state.
			World->GetTimerManager().SetTimerForNextTick([this, Item]()
			{
				IModioUIObjectSelector::Execute_SetSelectedStateForValue(this, Item, false, true);
			});
		}
	}

	OnWidgetCreated.Broadcast(&GeneratedEntryWidget, Item);
	return GeneratedEntryWidget;
}

void UModioDefaultObjectSelector::OnSelectionChangedInternal(UObject* FirstSelectedItem)
{
	Super::OnSelectionChangedInternal(FirstSelectedItem);
}

void UModioDefaultObjectSelector::OnItemClickedInternal(UObject* Item)
{
	Super::OnItemClickedInternal(Item);

	// Broadcast the selection event from our interface if necessary
	// Must be on clicked as controllers will use this to trigger selection
	NotifySelectionChanged(Item);
}

void UModioDefaultObjectSelector::NotifySelectionChanged(UObject* SelectedItem)
{
	// If we had an existing selection, make sure we notify it that it is no longer selected
	if (!GetMultiSelectionAllowed_Implementation())
	{
		if (PreviouslySelectedWidget != GetEntryWidgetFromItem(SelectedItem))
		{
			if (PreviouslySelectedWidget.IsValid() &&
				PreviouslySelectedWidget->GetClass()->ImplementsInterface(UModioUISelectableWidget::StaticClass()))
			{
				IModioUISelectableWidget::Execute_SetSelectedState(PreviouslySelectedWidget.Get(), false);
			}
			PreviouslySelectedWidget = GetEntryWidgetFromItem(SelectedItem);
		}
	}

	if (EmitSelectionEvents.Peek(true))
	{
		OnSelectedValueChanged.Broadcast(SelectedItem);
	}

	LastSelectedIndex = GetIndexForItem(SelectedItem);
}

void UModioDefaultObjectSelector::NativeSetObjects(const TArray<UObject*>& InObjects)
{
	SetListItems(InObjects);
	RegenerateAllEntries();
}

TArray<UObject*> UModioDefaultObjectSelector::NativeGetObjects()
{
	return GetListItems();
}

UObject* UModioDefaultObjectSelector::NativeGetObjectAt(int32 Index) const
{
	return GetItemAt(Index);
}

void UModioDefaultObjectSelector::NativeAddObjectWidgetCreatedHandler(
	const FModioObjectListOnObjectWidgetCreated& Handler)
{
	OnWidgetCreated.AddUnique(Handler);
}

void UModioDefaultObjectSelector::NativeRemoveObjectWidgetCreatedHandler(
	const FModioObjectListOnObjectWidgetCreated& Handler)
{
	OnWidgetCreated.Remove(Handler);
}

UWidget* UModioDefaultObjectSelector::NativeGetWidgetToFocus(EUINavigation NavigationType) const
{
	if (UObject* SelectedItem = GetSelectedItem())
	{
		if (UUserWidget* SelectedWidget = GetEntryWidgetFromItem(SelectedItem))
		{
			if (SelectedWidget->GetClass()->ImplementsInterface(UModioFocusableWidget::StaticClass()))
			{
				return IModioFocusableWidget::Execute_GetWidgetToFocus(SelectedWidget, NavigationType);
			}
			return SelectedWidget;
		}
	}
	return nullptr;
}

bool UModioDefaultObjectSelector::GetMultiSelectionAllowed_Implementation()
{
	return SelectionMode == ESelectionMode::Multi;
}

void UModioDefaultObjectSelector::SetMultiSelectionAllowed_Implementation(bool bMultiSelectionAllowed)
{
	SetSelectionMode(bMultiSelectionAllowed ? ESelectionMode::Multi : ESelectionMode::Single);
}

TArray<UObject*> UModioDefaultObjectSelector::GetSelectedValues_Implementation()
{
	TArray<UObject*> OutArray;
	GetSelectedItems(OutArray);
	return OutArray;
}

int32 UModioDefaultObjectSelector::GetLastSelectionIndex_Implementation()
{
	return LastSelectedIndex;
}

void UModioDefaultObjectSelector::SetSelectedStateForValue_Implementation(UObject* Value, bool bNewSelectionState,
                                                                          bool bEmitSelectionEvent)
{
	// override the default event emission setting so that our handler for selection changed knows if it should emit
	// events
	EmitSelectionEvents.Push(bEmitSelectionEvent);
	SetItemSelection(Value, bNewSelectionState, ESelectInfo::Direct);
	NotifySelectionChanged(Value);
	EmitSelectionEvents.Pop();
}

void UModioDefaultObjectSelector::SetSelectedStateForIndex_Implementation(int32 Index, bool bNewSelectionState,
																		  bool bEmitSelectionEvent)
{
	IModioUIObjectSelector::Execute_SetSelectedStateForValue(this, GetItemAt(Index), bNewSelectionState,
															 bEmitSelectionEvent);
}

void UModioDefaultObjectSelector::SetListEntryWidgetClass_Implementation(TSubclassOf<UWidget> InNewEntryClass)
{
	EntryWidgetClass = InNewEntryClass;
	//some kind of rebuild here maybe?
}

int32 UModioDefaultObjectSelector::GetIndexForValue_Implementation(UObject* Value) const
{
	return GetIndexForItem(Value);
}

UUserWidget* UModioDefaultObjectSelector::GetWidgetForValue_Implementation(UObject* Value) const
{
	return GetEntryWidgetFromItem(Value);
}

void UModioDefaultObjectSelector::ScrollToTop_Implementation()
{
	Cast<UListViewBase>(this)->ScrollToTop();
}

void UModioDefaultObjectSelector::ScrollToBottom_Implementation()
{
	Cast<UListViewBase>(this)->ScrollToBottom();
}

void UModioDefaultObjectSelector::SetScrollOffset_Implementation(float Offset)
{
	Cast<UListViewBase>(this)->SetScrollOffset(Offset);
}

float UModioDefaultObjectSelector::GetScrollOffset_Implementation() const
{
	return Cast<UListViewBase>(this)->GetScrollOffset();
}
