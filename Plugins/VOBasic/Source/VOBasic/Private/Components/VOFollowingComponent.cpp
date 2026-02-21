#include "Components/VOFollowingComponent.h"

#include "AbstractNavData.h"
#include "Core/VOManager.h"
#include "NavigationSystem.h"
#include "AI/Navigation/NavAreaBase.h"
#include "NavMesh/NavMeshPath.h"
#include "Settings/VOSettings.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavAreas/NavArea.h"
#include "NavMesh/RecastNavMesh.h"

DEFINE_LOG_CATEGORY(LogVOFollowing);

void LogPathPartHelper(AActor* LogOwner, FNavMeshPath* NavMeshPath, int32 StartIdx, int32 EndIdx)
{
#if ENABLE_VISUAL_LOG && WITH_RECAST
	ARecastNavMesh* NavMesh = Cast<ARecastNavMesh>(NavMeshPath->GetNavigationDataUsed());
	FVisualLogger& VisualLogger = FVisualLogger::Get();

	if (NavMesh == NULL ||
		!VisualLogger.IsCategoryLogged(LogNavigation) ||
		!NavMeshPath->PathCorridor.IsValidIndex(StartIdx) ||
		!NavMeshPath->PathCorridor.IsValidIndex(EndIdx))
	{
		return;
	}

	FVisualLogShapeElement CorridorPoly(EVisualLoggerShapeElement::Polygon);
	CorridorPoly.SetColor(FColorList::Cyan.WithAlpha(100));
	CorridorPoly.Category = LogNavigation.GetCategoryName();
	CorridorPoly.Points.Reserve((EndIdx - StartIdx) * 6);

	const FVector CorridorOffset = NavigationDebugDrawing::PathOffset * 1.25f;
	int32 NumAreaMark = 1;

	if (FVisualLogEntry* Snapshot = FVisualLogger::GetEntryToWrite(LogOwner, LogVOFollowing))
	{
		NavMesh->BeginBatchQuery();

		TArray<FVector> Verts;
		for (int32 Idx = StartIdx; Idx <= EndIdx; Idx++)
		{
			const uint8 AreaID = IntCastChecked<uint8>(NavMesh->GetPolyAreaID(NavMeshPath->PathCorridor[Idx]));
			const UClass* AreaClass = NavMesh->GetAreaClass(AreaID);

			Verts.Reset();
			NavMesh->GetPolyVerts(NavMeshPath->PathCorridor[Idx], Verts);

			FVector CenterPt = FVector::ZeroVector;
			for (int32 VIdx = 0; VIdx < Verts.Num(); VIdx++)
			{
				Verts[VIdx].Z += 5.0f;
				CenterPt += Verts[VIdx];
			}
			CenterPt /= Verts.Num();

			const UNavArea* DefArea = AreaClass ? ((UClass*)AreaClass)->GetDefaultObject<UNavArea>() : NULL;
			const FColor PolygonColor = AreaClass != FNavigationSystem::GetDefaultWalkableArea() ? (DefArea ? DefArea->DrawColor : NavMesh->GetConfig().Color) : FColorList::LightSteelBlue;

			CorridorPoly.SetColor(PolygonColor.WithAlpha(100));
			CorridorPoly.Points.Reset();
			CorridorPoly.Points.Append(Verts);
			Snapshot->ElementsToDraw.Add(CorridorPoly);

			if (AreaClass && AreaClass != FNavigationSystem::GetDefaultWalkableArea())
			{
				FVisualLogShapeElement AreaMarkElem(EVisualLoggerShapeElement::Segment);
				AreaMarkElem.SetColor(FColorList::Orange.WithAlpha(100));
				AreaMarkElem.Category = LogNavigation.GetCategoryName();
				AreaMarkElem.Thickness = 2;
				AreaMarkElem.Description = AreaClass->GetName();

				AreaMarkElem.Points.Add(CenterPt + CorridorOffset);
				AreaMarkElem.Points.Add(CenterPt + CorridorOffset + FVector(0, 0, 100.0f + NumAreaMark * 50.0f));
				Snapshot->ElementsToDraw.Add(AreaMarkElem);

				NumAreaMark = (NumAreaMark + 1) % 5;
			}
		}

		NavMesh->FinishBatchQuery();
	}
#endif // ENABLE_VISUAL_LOG && WITH_RECAST
}

UVOFollowingComponent::UVOFollowingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	bWantsInitializeComponent = true;
}

void UVOFollowingComponent::Initialize()
{
	Super::Initialize();

	SimulationState = ECrowdSimulationState::Enabled;
}

void UVOFollowingComponent::SetMoveSegment(int32 SegmentStartIndex)
{
	UE_LOG(LogTemp, Warning, TEXT("1"));
	if (!IsCrowdSimulationEnabled())
	{
		Super::SetMoveSegment(SegmentStartIndex);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("2"));

	PathStartIndex = SegmentStartIndex;
	LastPathPolyIndex = PathStartIndex;
	if (Path.IsValid() == false || Path->IsValid() == false || GetOwner() == NULL)
	{
		return;
	}
	
	FVector CurrentTargetPt = Path->GetPathPoints().Last().Location;

	FNavMeshPath* NavMeshPath = Path->CastPath<FNavMeshPath>();
	FAbstractNavigationPath* DirectPath = Path->CastPath<FAbstractNavigationPath>();
	UE_LOG(LogTemp, Warning, TEXT("3"));
	if (NavMeshPath)
	{
#if WITH_RECAST
		if (NavMeshPath->PathCorridor.Num() == 0)
		{
			UE_VLOG(GetOwner(), LogVOFollowing, Error, TEXT("Can't switch path segments: empty path corridor!"));
			OnPathFinished(FPathFollowingResult(EPathFollowingResult::Aborted, FPathFollowingResultFlags::InvalidPath));
			return;
		}
		else if (NavMeshPath->PathCorridor.IsValidIndex(PathStartIndex) == false)
		{
			// this should never matter, but just in case
			UE_VLOG(GetOwner(), LogVOFollowing, Error, TEXT("SegmentStartIndex in call to UCrowdFollowingComponent::SetMoveSegment is out of path corridor array's bounds (index: %d, array size %d)")
				, PathStartIndex, NavMeshPath->PathCorridor.Num());
			PathStartIndex = FMath::Clamp<int32>(PathStartIndex, 0, NavMeshPath->PathCorridor.Num() - 1);
		}

		// cut paths into parts to avoid problems with crowds getting into local minimum
		// due to using only first 10 steps of A*

		// do NOT use PathPoints here, crowd simulation disables path post processing
		// which means, that PathPoints contains only start and end position 
		// full path is available through PathCorridor array (poly refs)

		UVOManager* VOManager = UVOManager::GetCurrent(GetWorld());
		if (VOManager == nullptr)
		{
			UE_VLOG(GetOwner(), LogVOFollowing, Error, TEXT("Can't switch path segments: missing crowd manager!"));
			OnPathFinished(FPathFollowingResult(EPathFollowingResult::Aborted, FPathFollowingResultFlags::InvalidPath));
			return;
		}

		ARecastNavMesh* RecastNavData = Cast<ARecastNavMesh>(NavMeshPath->GetNavigationDataUsed());
		if (RecastNavData == nullptr)
		{
			UE_VLOG(GetOwner(), LogVOFollowing, Error, TEXT("Invalid navigation data in UCrowdFollowingComponent::SetMoveSegment, expected ARecastNavMesh class, got: %s"), *GetNameSafe(NavMeshPath->GetNavigationDataUsed()));
			OnPathFinished(FPathFollowingResult(EPathFollowingResult::Aborted, FPathFollowingResultFlags::InvalidPath));
			return;
		}
		else if (VOManager->GetNavData(this) != RecastNavData)
		{
			UE_VLOG(GetOwner(), LogVOFollowing, Error, TEXT("Invalid navigation data in UCrowdFollowingComponent::SetMoveSegment, expected 0x%X, got: 0x%X"), VOManager->GetNavData(this), RecastNavData);
			OnPathFinished(FPathFollowingResult(EPathFollowingResult::Aborted, FPathFollowingResultFlags::InvalidPath));
			return;
		}

		const int32 PathPartSize = 15;
		const int32 LastPolyIdx = NavMeshPath->PathCorridor.Num() - 1;
		int32 PathPartEndIdx = FMath::Min(PathStartIndex + PathPartSize, LastPolyIdx);
		bFinalPathPart = (PathPartEndIdx == LastPolyIdx);

		FVector PtA, PtB;
		const bool bStartIsNavLink = RecastNavData->GetLinkEndPoints(NavMeshPath->PathCorridor[PathStartIndex], PtA, PtB);
		if (bStartIsNavLink)
		{
			PathStartIndex = FMath::Max(0, PathStartIndex - 1);
		}

		if (!bFinalPathPart)
		{
			const bool bEndIsNavLink = RecastNavData->GetLinkEndPoints(NavMeshPath->PathCorridor[PathPartEndIdx], PtA, PtB);
			const bool bSwitchIsNavLink = (PathPartEndIdx > 0) ? RecastNavData->GetLinkEndPoints(NavMeshPath->PathCorridor[PathPartEndIdx - 1], PtA, PtB) : false;
			if (bEndIsNavLink)
			{
				PathPartEndIdx = FMath::Max(0, PathPartEndIdx - 1);
			}
			if (bSwitchIsNavLink)
			{
				PathPartEndIdx = FMath::Max(0, PathPartEndIdx - 2);
			}

			RecastNavData->GetPolyCenter(NavMeshPath->PathCorridor[PathPartEndIdx], CurrentTargetPt);
		}
		else if (NavMeshPath->IsPartial())
		{
			RecastNavData->GetClosestPointOnPoly(NavMeshPath->PathCorridor[PathPartEndIdx], Path->GetPathPoints().Last().Location, CurrentTargetPt);
		}

		// not safe to read those directions yet, you have to wait until crowd manager gives you next corner of string pulled path
		CrowdAgentMoveDirection = FVector::ZeroVector;
		MoveSegmentDirection = FVector::ZeroVector;

		CurrentDestination.Set(Path->GetBaseActor(), CurrentTargetPt);

		LogPathPartHelper(GetOwner(), NavMeshPath, PathStartIndex, PathPartEndIdx);
		UE_VLOG_SEGMENT(GetOwner(), LogVOFollowing, Log, NavMovementInterface->GetFeetLocation(), CurrentTargetPt, FColor::Red, TEXT("path part"));
		UE_VLOG(GetOwner(), LogVOFollowing, Log, TEXT("SetMoveSegment, from:%d segments:%d%s"),
			PathStartIndex, (PathPartEndIdx - PathStartIndex)+1, bFinalPathPart ? TEXT(" (final)") : TEXT(""));

		VOManager->SetAgentMovePath(this, NavMeshPath, PathStartIndex, PathPartEndIdx, CurrentTargetPt);
#endif
	}
	/*else if (DirectPath)
	{
		//TODO: Implement
		
		// direct paths are not using any steering or avoidance
		// pathfinding is replaced with simple velocity request 

		const FVector AgentLoc = NavMovementInterface->GetFeetLocation();

		bFinalPathPart = true;
		bCheckMovementAngle = true;
		bUpdateDirectMoveVelocity = true;
		CurrentDestination.Set(Path->GetBaseActor(), CurrentTargetPt);
		CrowdAgentMoveDirection = (CurrentTargetPt - AgentLoc).GetSafeNormal();
		MoveSegmentDirection = CrowdAgentMoveDirection;

		UE_VLOG(GetOwner(), LogVOFollowing, Log, TEXT("SetMoveSegment, direct move"));
		UE_VLOG_SEGMENT(GetOwner(), LogVOFollowing, Log, AgentLoc, CurrentTargetPt, FColor::Red, TEXT("path"));

		UCrowdManager* CrowdManager = UCrowdManager::GetCurrent(GetWorld());
		if (CrowdManager)
		{
			CrowdManager->SetAgentMoveDirection(this, CrowdAgentMoveDirection);
		}
	}*/
	else
	{
		UE_VLOG(GetOwner(), LogVOFollowing, Error, TEXT("SetMoveSegment, unknown path type!"));
	}
}

void UVOFollowingComponent::OnRegister()
{
	Super::OnRegister(); // TODO: Check UCrowdManager::GetCurrent(GetWorld()); in CrowdFollowComp.cpp
	if (UWorld* W = GetWorld())
		if (auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(W))
			if (auto* CM = Cast<UVOManager>(Nav->GetCrowdManager()))
				CM->RegisterAgent(this);
}

void UVOFollowingComponent::OnUnregister()
{
	if (UWorld* W = GetWorld())
		if (auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(W))
			if (auto* CM = Cast<UVOManager>(Nav->GetCrowdManager()))
				CM->UnregisterAgent(this);
	Super::OnUnregister();
}

void UVOFollowingComponent::FollowPathSegment(float DeltaTime)
{
	/*if (Path.IsValid() && IsCrowdSimulationActive())
	{
		UpdatePathSegment();
	}*/
	Super::FollowPathSegment(DeltaTime);
}

void UVOFollowingComponent::SetVOProfile(FName InProfileName)
{
	//TODO: Check for validity
	VOProfile = InProfileName; 
	MarkEffectiveDirty();
}

void UVOFollowingComponent::SetVOOverrides(const FVOParams& InOverrides, bool bEnable)
{
	Overrides     = InOverrides;
	bUseOverrides = bEnable;
	MarkEffectiveDirty();
}

int32 UVOFollowingComponent::AddParamModifier(const FVOParamModifier& InMod)
{
	FVOParamModifier Copy = InMod;
	Copy.Id = NextModId++;
	ActiveMods.Add(Copy);
	MarkEffectiveDirty();
	return Copy.Id;
}

bool UVOFollowingComponent::RemoveParamModifierById(int32 Id)
{
	const int32 Removed = ActiveMods.RemoveAll([&](const FVOParamModifier& M){ return M.Id == Id; });
	if (Removed > 0) MarkEffectiveDirty();
	return Removed > 0;
}

int32 UVOFollowingComponent::RemoveParamModifiersByTag(FName Tag)
{
	const int32 Removed = ActiveMods.RemoveAll([&](const FVOParamModifier& M){ return M.Tag == Tag; });
	if (Removed > 0) MarkEffectiveDirty();
	return Removed;
}

void UVOFollowingComponent::SetAvoidanceStyle(EAvoidanceStyle NewStyle)
{
	EffectiveParams.AvoidanceStyle = NewStyle; // For instant application
	AvoidanceStyleOverride = NewStyle;
	bHasAvoidanceStyleOverride = true;
}

void UVOFollowingComponent::ResetAvoidanceStyle()
{
	bHasAvoidanceStyleOverride = false;
	MarkEffectiveDirty();
}

void UVOFollowingComponent::SetAgentShape(EVOAgentShape NewShape)
{
	EffectiveParams.Shape = NewShape; // For instant application
	ShapeOverride = NewShape;
	bHasShapeOverride = true;
}

void UVOFollowingComponent::ResetAgentShape()
{
	bHasShapeOverride = false;
	MarkEffectiveDirty();
}

void UVOFollowingComponent::SetAgentOrientation(EVOOrientation NewOrientation)
{
	EffectiveParams.Orientation = NewOrientation; // For instant application
	OrientationOverride = NewOrientation;
	bHasOrientationOverride = true;
}

void UVOFollowingComponent::ResetAgentOrientation()
{
	bHasShapeOverride = false;
	MarkEffectiveDirty();
}

const FVOParams& UVOFollowingComponent::GetEffectiveParams() const
{
	// Return effective params if they are already computed
	if (!bEffectiveDirty)
		return EffectiveParams;

	const UVOSettings* Settings = UVOSettings::Get();

	// Base params (overrides or existing preset)
	FVOParams Base = bUseOverrides ? Overrides : Settings->GetPresetOrDefault(VOProfile); // TODO: Caching current profile

	EffectiveParams = Base;

	// Apply modifiers
	if (!ActiveMods.IsEmpty())
	{
		TArray<FVOParamModifier> Sorted = ActiveMods;
		Sorted.Sort([](const auto& A, const auto& B){ return A.Priority < B.Priority; });
		for (const FVOParamModifier& M : Sorted)
		{
			ApplyModifierTo(EffectiveParams, M);
		}
	}

	// Clamp values
	EffectiveParams.AgentRadius		= FMath::Max(0.f, EffectiveParams.AgentRadius);
	EffectiveParams.AgentExtent		= FMath::Max(0.f, EffectiveParams.AgentExtent);
	EffectiveParams.AgentHeight		= FMath::Max(0.f, EffectiveParams.AgentHeight);
	EffectiveParams.MaxSpeed		= FMath::Max(0.f, EffectiveParams.MaxSpeed);
	EffectiveParams.NeighborRange	= FMath::Max(0.f, EffectiveParams.NeighborRange);
	EffectiveParams.TauHorizon		= FMath::Max(0.01f, EffectiveParams.TauHorizon);
	EffectiveParams.MaxAcceleration = FMath::Max(0.f, EffectiveParams.MaxAcceleration);
	EffectiveParams.CustomAngle		= FMath::Max(0.f, EffectiveParams.CustomAngle);
	EffectiveParams.TauAcceleration = FMath::Max(0.f, EffectiveParams.TauAcceleration);

	// Apply avoidance style override
	if (bHasAvoidanceStyleOverride)
		EffectiveParams.AvoidanceStyle = AvoidanceStyleOverride;

	if (bHasShapeOverride)
		EffectiveParams.Shape = ShapeOverride;

	if (bHasOrientationOverride)
		EffectiveParams.Orientation = OrientationOverride;
	
	bEffectiveDirty = false;
	return EffectiveParams;
}

static void ApplyOp(float& Field, EVOOp Op, float Magnitude)
{
	switch (Op)
	{
		case EVOOp::Add: Field += Magnitude; break;
		case EVOOp::Mul: Field *= Magnitude; break;
		case EVOOp::Set: Field  = Magnitude; break;
		default: break;
	}
}

void UVOFollowingComponent::ApplyModifierTo(FVOParams& P, const FVOParamModifier& M) const
{
	switch (M.Key)
	{
		case EVOParamKey::TauHorizon:    	ApplyOp(P.TauHorizon,    M.Op, M.Magnitude); 	break;
		case EVOParamKey::MaxSpeed:      	ApplyOp(P.MaxSpeed,      M.Op, M.Magnitude); 	break;
		case EVOParamKey::NeighborRange: 	ApplyOp(P.NeighborRange, M.Op, M.Magnitude); 	break;
		case EVOParamKey::AgentRadius:   	ApplyOp(P.AgentRadius,   M.Op, M.Magnitude); 	break;
		case EVOParamKey::AgentExtent:   	ApplyOp(P.AgentExtent,   M.Op, M.Magnitude); 	break;
		case EVOParamKey::AgentCustomAngle: ApplyOp(P.CustomAngle, M.Op, M.Magnitude); 		break;
		case EVOParamKey::AgentHeight:		ApplyOp(P.AgentHeight,M.Op, M.Magnitude); 		break;
		case EVOParamKey::MaxAcceleration:	ApplyOp(P.MaxAcceleration, M.Op, M.Magnitude);	break;
		case EVOParamKey::TauAcceleration:  ApplyOp(P.TauAcceleration, M.Op, M.Magnitude); 	break;
	}
}

APawn* GetControlledPawn_Local(const UVOFollowingComponent* Comp)
{
	if (const AController* C = Cast<AController>(Comp->GetOwner()))
		return C->GetPawn();
	return nullptr;
}
FVector UVOFollowingComponent::GetOwnerLocation() const
{
	// TODO: CachedPawn?
	const APawn* P = GetControlledPawn_Local(this);
	return P ? P->GetActorLocation() : FVector::ZeroVector;
}

FVector UVOFollowingComponent::GetOwnerVelocity() const
{
	if (bUseFakeVelocity)
	{
		return FakeVelocity;
	}
	
	const APawn* P = GetControlledPawn_Local(this);
	if (!P) return FVector::ZeroVector;
	if (const UMovementComponent* Move = P->FindComponentByClass<UMovementComponent>()) //TODO: Maybe unneeded
	{
		return Move->Velocity;
	}
	return FVector::ZeroVector;
}

void UVOFollowingComponent::GetAgentCapsuleSegment(FVector2D& OutP1, FVector2D& OutP2) const
{
	APawn* Pawn = GetControlledPawn_Local(this);
	FVector2D WorldPos = FVector2D(Pawn->GetActorLocation());

	GetAgentCapsuleSegmentLocal(OutP1, OutP2);
	OutP1 = WorldPos + OutP1;
	OutP2 =	WorldPos + OutP2;
}

void UVOFollowingComponent::GetAgentCapsuleSegmentLocal(FVector2D& OutP1, FVector2D& OutP2) const
{
	APawn* Pawn = GetControlledPawn_Local(this);
	
	const FVOParams& CurParams = GetEffectiveParams();
	if (CurParams.Shape == EVOAgentShape::Circle || CurParams.AgentExtent <= KINDA_SMALL_NUMBER)
	{
		OutP1 = OutP2 = FVector2D::ZeroVector;
		return;
	}

	FVector2D WorldAxis;
	switch (CurParams.Orientation)
	{
	case EVOOrientation::Forward: WorldAxis = FVector2D(Pawn->GetActorForwardVector()); break;
	case EVOOrientation::Right:   WorldAxis = FVector2D(Pawn->GetActorRightVector()); break;
	case EVOOrientation::Custom:
		{
			float Rad = FMath::DegreesToRadians(CurParams.CustomAngle);
			FVector2D RotatedDir(FMath::Cos(Rad), FMath::Sin(Rad));
			WorldAxis = FVector2D(Pawn->GetActorForwardVector()) * RotatedDir.X + FVector2D(Pawn->GetActorRightVector()) * RotatedDir.Y;
		}
		break;
	default: UE_LOG(LogVOFollowing, Error, TEXT("Invalid orientation for agent capsule!")); return;
	}

	FVector2D Offset = WorldAxis * CurParams.AgentExtent;
	OutP1 = - Offset;
	OutP2 =	  Offset;
}

void UVOFollowingComponent::GetAgentCapsuleSegment(const APawn* Pawn, const FVOParams& Params, FVector2D& OutP1, FVector2D& OutP2)
{
	FVector2D WorldPos = FVector2D(Pawn->GetActorLocation());

	if (Params.Shape == EVOAgentShape::Circle || Params.AgentExtent <= KINDA_SMALL_NUMBER)
	{
		OutP1 = OutP2 = WorldPos;
		return;
	}

	FVector2D WorldAxis;
	switch (Params.Orientation)
	{
		case EVOOrientation::Forward: WorldAxis = FVector2D(Pawn->GetActorForwardVector()); break;
		case EVOOrientation::Right:   WorldAxis = FVector2D(Pawn->GetActorRightVector()); break;
		case EVOOrientation::Custom:
			{
				float Rad = FMath::DegreesToRadians(Params.CustomAngle);
				FVector2D RotatedDir(FMath::Cos(Rad), FMath::Sin(Rad));
				WorldAxis = FVector2D(Pawn->GetActorForwardVector()) * RotatedDir.X + FVector2D(Pawn->GetActorRightVector()) * RotatedDir.Y;
			}
			break;
		default: UE_LOG(LogVOFollowing, Error, TEXT("Invalid orientation for agent capsule!")); return;
	}

	FVector2D Offset = WorldAxis * Params.AgentExtent;
	OutP1 = WorldPos - Offset;
	OutP2 = WorldPos + Offset;
}

void UVOFollowingComponent::UpdateKinematics(float DeltaTime)
{
	const APawn* P = GetControlledPawn_Local(this);
	if (!P) return;

	const UMovementComponent* Move = P->FindComponentByClass<UMovementComponent>();
	/*const*/ FVector NewVel = Move ? Move->Velocity : FVector::ZeroVector;
	
	if (bUseFakeVelocity)
	{
		NewVel = FakeVelocity;
	}
	
	if (bHasPrevVelocity && DeltaTime > KINDA_SMALL_NUMBER)
	{
		CachedAcceleration = (NewVel - CachedVelocity) / DeltaTime;
		CachedAcceleration.Z = 0.f;
	}

	CachedVelocity   = NewVel;
	bHasPrevVelocity = true;

	if (const UCharacterMovementComponent* CharMove = Cast<UCharacterMovementComponent>(Move)) //TODO: Move to optimize
	{
		FVector Acc2D = CharMove->GetCurrentAcceleration();
		Acc2D.Z = 0.f;
		CachedAcceleration = Acc2D;
	}
}

#ifdef SAVE_VO_PATHS
void UVOFollowingComponent::UpdatePathHistory(float DeltaTime)
{
	float CurrentTime = GetWorld()->GetTimeSeconds();
		
	if (CurrentTime - LastPathSaveTime > PathSaveInterval)
	{
		PathHistory.Add(GetOwnerLocation());
		LastPathSaveTime = CurrentTime;

		if (PathHistory.Num() > PathHistorySize)
		{
			PathHistory.RemoveAt(0);
		}
	}
}
#endif

// TODO: Delete
void UVOFollowingComponent::SetFakeVelocity(const FVector& InVelocity)
{
	FakeVelocity = InVelocity;
	bUseFakeVelocity = true;
}

void UVOFollowingComponent::ClearFakeVelocity()
{
	bUseFakeVelocity = false;
	FakeVelocity = FVector::ZeroVector;
}