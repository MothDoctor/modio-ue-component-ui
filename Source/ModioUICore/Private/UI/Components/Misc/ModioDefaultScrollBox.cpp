/*
 *  Copyright (C) 2024 mod.io Pty Ltd. <https://mod.io>
 *
 *  This file is part of the mod.io UE Plugin.
 *
 *  Distributed under the MIT License. (See accompanying file LICENSE or
 *   view online at <https://github.com/modio/modio-ue/blob/main/LICENSE>)
 *
 */

#include "UI/Components/Misc/ModioDefaultScrollBox.h"

#include "Misc/EngineVersionComparison.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModioDefaultScrollBox)

void UModioDefaultScrollBox::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	UpdateNavigationData(Execute_GetScrollOffset(this));
}

TSharedRef<SWidget> UModioDefaultScrollBox::RebuildWidget()
{
	UpdateNavigationData(Execute_GetScrollOffset(this));
	OnUserScrolled.RemoveDynamic(this, &UModioDefaultScrollBox::OnUserScrolledHandle);
	OnUserScrolled.AddDynamic(this, &UModioDefaultScrollBox::OnUserScrolledHandle);

	// TODO: This might be a bit hacky, but it's the only way I found to make sure that the scroll box GetScrollOffsetOfEnd() is correct (it is set up in SScrollBox sometime during the tick, not the first though). Revise this if you find a better way.
	ON_SCOPE_EXIT
	{
		if (NavigationDataTickHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(NavigationDataTickHandle);
			NavigationDataTickHandle.Reset();
		}

		NavigationDataTickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float Time) {
												  UpdateNavigationData(Execute_GetScrollOffset(this));

			// Run only once
			return false;
		}), 0.25f); // This amount of time is somewhat arbitrary, but it seems to work well enough 
	};

	return Super::RebuildWidget();
}

void UModioDefaultScrollBox::OnUserScrolledHandle_Implementation(float CurrentOffset)
{
	UpdateNavigationData(CurrentOffset);
}

void UModioDefaultScrollBox::UpdateNavigationData_Implementation(int32 PendingScrollOffset)
{
	if (!IsValid(Navigation))
	{
		return;
	}

	if (IsDesignTime() || !IsValid(UserDefinedNavigation))
	{
		UserDefinedNavigation = NewObject<UWidgetNavigation>(this);
		UserDefinedNavigation->Up = Navigation->Up;
		UserDefinedNavigation->Down = Navigation->Down;
		UserDefinedNavigation->Left = Navigation->Left;
		UserDefinedNavigation->Right = Navigation->Right;
	}

	FCustomWidgetNavigationDelegate NavigationDelegate;
	NavigationDelegate.BindUFunction(this, "HandleCustomBoundaryNavigation");


	#if UE_VERSION_OLDER_THAN(5, 2, 0)
	if (Orientation == EOrientation::Orient_Vertical)
		#else
	if (GetOrientation() == EOrientation::Orient_Vertical)
		#endif
	{
		SetNavigationRuleCustomBoundary(EUINavigation::Down, NavigationDelegate);
		SetNavigationRuleCustomBoundary(EUINavigation::Up, NavigationDelegate);
	}
	#if UE_VERSION_OLDER_THAN(5, 2, 0)
	else if (Orientation == EOrientation::Orient_Horizontal)
		#else
	else if (GetOrientation() == EOrientation::Orient_Horizontal)
		#endif
	{
		SetNavigationRuleCustomBoundary(EUINavigation::Right, NavigationDelegate);
		SetNavigationRuleCustomBoundary(EUINavigation::Left, NavigationDelegate);
	}
		
	if (!bScrollByNavigationInput || IsDesignTime())
	{
		Navigation = UserDefinedNavigation;
		BuildNavigation();
	}
	else if (PendingScrollOffset != INDEX_NONE && (PendingScrollOffset <= 0 || float(PendingScrollOffset) >= GetScrollOffsetOfEnd() || GetScrollOffsetOfEnd() == 0.0f))
	{
		if (PendingScrollOffset <= 0)
		{
			Navigation->Up = UserDefinedNavigation->Up;
			Navigation->Left = UserDefinedNavigation->Left;
		}
		if (GetScrollOffsetOfEnd() == 0.0f || (GetScrollOffsetOfEnd() > 0.0f && float(PendingScrollOffset) >= GetScrollOffsetOfEnd()))
		{
			Navigation->Right = UserDefinedNavigation->Right;
			Navigation->Down = UserDefinedNavigation->Down;
		}
		BuildNavigation();
	}
}

UWidget* UModioDefaultScrollBox::HandleCustomBoundaryNavigation_Implementation(EUINavigation InNavigation)
{
	const float NewOffset = [this, InNavigation]() {
		if (InNavigation == EUINavigation::Down || InNavigation == EUINavigation::Right)
		{
			return Execute_GetScrollOffset(this) + float(NavigationScrollOffsetStep);
		}
		if (InNavigation == EUINavigation::Up || InNavigation == EUINavigation::Left)
		{
			return Execute_GetScrollOffset(this) - float(NavigationScrollOffsetStep);
		}
		return static_cast<float>(INDEX_NONE);
	}();
	Execute_SetScrollOffset(this, NewOffset);
	UpdateNavigationData(NewOffset);
	return nullptr;
}

void UModioDefaultScrollBox::ScrollToTop_Implementation()
{
	UScrollBox::SetScrollOffset(0.0f);
}

void UModioDefaultScrollBox::ScrollToBottom_Implementation()
{
	UScrollBox::SetScrollOffset(GetScrollOffsetOfEnd());
}

void UModioDefaultScrollBox::SetScrollOffset_Implementation(float Offset)
{
	Offset = FMath::Clamp(Offset, 0.0f, GetScrollOffsetOfEnd());
	UScrollBox::SetScrollOffset(Offset);
}

float UModioDefaultScrollBox::GetScrollOffset_Implementation() const
{
	return UScrollBox::GetScrollOffset();
}

bool UModioDefaultScrollBox::CanScrollInDirection_Implementation(bool bBackward) const 
{
	if (bBackward)
	{
		return Execute_GetScrollOffset(this) > 0.0f;
	}
	else
	{
		return Execute_GetScrollOffset(this) < GetScrollOffsetOfEnd();
	}
}
