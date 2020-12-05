// Fill out your copyright notice in the Description page of Project Settings.

#include "Controllers/AI/UnitGroupAIController.h"
#include "CommanderBlackboardDataKeys.h"
#include "DelegateHelpers.h"
#include "AIPrototype/AIPrototype.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Controllers/AI/UnitAIController.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense_Sight.h"
#include "Units/UnitBase.h"
#include "VisualLogger/VisualLogger.h"

AUnitGroupAIController::AUnitGroupAIController()
{
	PerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("PerceptionComponent"));
	PerceptionComponent->SetDominantSense(UAISense_Sight::StaticClass());
}

void AUnitGroupAIController::InitializeControlledUnits(const TArray<AUnitBase*>& Units)
{
	for (const auto unit : Units)
	{
		m_ControlledUnits.AddUnique(unit);
	}

	InitBlackboardAlliesNum();
}

void AUnitGroupAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateBBEnemyGroupCenterLocation();
}

void AUnitGroupAIController::InitBlackboardAlliesNum()
{
	GetBlackboardComponent()->SetValueAsInt(CBBKeys::AlliesNum, m_ControlledUnits.Num());
}

void AUnitGroupAIController::BeginPlay()
{
	Super::BeginPlay();

	SubscribeOnPerceptionUpdates();
	RunBehaviorTree(BehaviorTreeAsset);
}

void AUnitGroupAIController::SubscribeOnPerceptionUpdates()
{
	PerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(this, &AUnitGroupAIController::OnTargetPerceptionUpdated);
}

void AUnitGroupAIController::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (Stimulus.IsActive())
	{
		if (!m_SensedEnemies.Contains(Actor))
		{
			m_SensedEnemies.AddUnique(Actor);
		}
	}
	else if (Stimulus.IsExpired())
	{
		if (m_SensedEnemies.Contains(Actor))
		{
			m_SensedEnemies.Remove(Actor);
		}
	}

	UpdateBlackboardEnemiesNum();
}

void AUnitGroupAIController::UpdateBBEnemyGroupCenterLocation()
{
	if (m_SensedEnemies.Num() > 0)
	{
		GetBlackboardComponent()->SetValueAsVector(CBBKeys::EnemyGroupCenterLocation, CalcEnemyGroupCenterLocation());
	}
	else
	{
		GetBlackboardComponent()->ClearValue(CBBKeys::EnemyGroupCenterLocation);
	}
}

void AUnitGroupAIController::UpdateBlackboardEnemiesNum()
{
	GetBlackboardComponent()->SetValueAsInt(CBBKeys::EnemiesNum, m_SensedEnemies.Num());
}

FVector AUnitGroupAIController::CalcEnemyGroupCenterLocation() const
{
	FVector min_location = FVector(BIG_NUMBER);
	FVector max_location = FVector(-BIG_NUMBER);
	FVector unit_location = FVector::ZeroVector;

	for (const auto unit : m_SensedEnemies)
	{
		if (unit->IsValidLowLevelFast())
		{
			unit_location = unit->GetActorLocation();

			min_location.X = FMath::Min<float>(unit_location.X, min_location.X);
			min_location.Y = FMath::Min<float>(unit_location.Y, min_location.Y);
			min_location.Z = FMath::Min<float>(unit_location.Z, min_location.Z);

			max_location.X = FMath::Max<float>(unit_location.X, max_location.X);
			max_location.Y = FMath::Max<float>(unit_location.Y, max_location.Y);
			max_location.Z = FMath::Max<float>(unit_location.Z, max_location.Z);
		}
	}

	const auto center_location = (min_location + max_location) * 0.5f;

	UE_VLOG_BOX(this, LogAIPrototype, Log, FBox(min_location, max_location), FColor::Green, TEXT(""));
	UE_VLOG_SEGMENT(this, LogAIPrototype, Log, min_location, max_location, FColor::Blue, TEXT(""));
	UE_VLOG_LOCATION(this, LogAIPrototype, Log, center_location, 10, FColor::Red, TEXT("group center location"));

	return center_location;
}

void AUnitGroupAIController::MoveGroupToLocation(const FVector& Location, const float AcceptanceRadius)
{
	for (const auto unit_weak_ptr : m_ControlledUnits)
	{
		if (const auto unit = unit_weak_ptr.Get())
		{
			const auto unit_ai_controller = Cast<AUnitAIController>(unit->GetController());
			if (unit_ai_controller != nullptr)
			{
				unit_ai_controller->MoveToLocation(Location);
			}
		}
	}

	auto pathFollowingComponent = GetPathFollowingComponent();
	pathFollowingComponent->SetAcceptanceRadius(AcceptanceRadius);
	Chain(this, pathFollowingComponent->OnRequestFinished, OnGroupMovementFinished, this);
}

