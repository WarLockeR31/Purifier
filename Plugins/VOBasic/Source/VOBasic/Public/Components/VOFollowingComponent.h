#pragma once
#include "CoreMinimal.h"
#include "Navigation/CrowdFollowingComponent.h"
#include "VOFollowingComponent.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogVOFollowing, Warning, All);

class UVOSettings;

USTRUCT(BlueprintType)
struct FAOScoringParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AO Scoring") float WeightEffort = 400.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AO Scoring") float WeightDesired = 400.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AO Scoring") float ProximityTargetThreshold = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AO Scoring") float WeightTurningPenalty = 100.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AO Scoring") float TurningRestrictionRange = 100.f;
};

USTRUCT(BlueprintType)
struct FAOSocialForces
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AO Social Force") float CloseRange = 20.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AO Social Force") float CloseForce = 50.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AO Social Force") float FarRange = 60.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AO Social Force") float FarForce = 20.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AO Social Force") float StaticRange = 20.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AO Social Force") float StaticForce = 50.f;
};

UENUM(BlueprintType)
enum class EAvoidanceStyle : uint8
{
	VelocityObstacle,
	AccelerationObstacle,
};

UENUM(BlueprintType)
enum class EVOAgentShape : uint8
{
	Circle,
	Capsule,
};

UENUM(BlueprintType)
enum class EVOOrientation : uint8
{
	Forward,
	Right,
	Custom,
};

USTRUCT(BlueprintType)
struct FVOParams
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VO") 	float 			TauHorizon		= 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VO") 	float 			MaxSpeed		= 400.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VO") 	float 			NeighborRange	= 600.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VO")  EVOAgentShape	Shape			= EVOAgentShape::Circle;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VO") 	float 			AgentRadius		= 34.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VO",
		meta=(EditCondition="Shape==EVOAgentShape::Capsule",
			EditConditionHides))								float			AgentExtent		= 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VO",
		meta=(EditCondition="Shape==EVOAgentShape::Capsule",
			EditConditionHides))								EVOOrientation	Orientation		= EVOOrientation::Forward;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VO",
		meta=(EditCondition="Shape==EVOAgentShape::Capsule && Orientation==EVOOrientation::Custom",
			EditConditionHides, Units="Degrees"))				float			CustomAngle		= 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VO") 	float 			AgentHeight		= 200.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VO")	EAvoidanceStyle	AvoidanceStyle	= EAvoidanceStyle::VelocityObstacle;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VO",
		meta=(EditCondition="AvoidanceStyle==EAvoidanceStyle::AccelerationObstacle",
			EditConditionHides))								float			MaxAcceleration = 2000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VO",
		meta=(EditCondition="AvoidanceStyle==EAvoidanceStyle::AccelerationObstacle",
			EditConditionHides))								float			TauAcceleration = 1.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VO",
		meta=(EditCondition="AvoidanceStyle==EAvoidanceStyle::AccelerationObstacle",
			EditConditionHides))								FAOScoringParams AOScoringParams;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VO",
    	meta=(EditCondition="AvoidanceStyle==EAvoidanceStyle::AccelerationObstacle",
    		EditConditionHides))								FAOSocialForces	AOSocialForces;
};

UENUM(BlueprintType)
enum class EVOParamKey : uint8
{
	TauHorizon,
	MaxSpeed,
	NeighborRange,
	AgentRadius,
	MaxAcceleration,
	AgentHeight,
	AgentExtent,
	AgentCustomAngle,
	TauAcceleration,
};

UENUM(BlueprintType)
enum class EVOOp : uint8 { Add, Mul, Set };

USTRUCT(BlueprintType)
struct VOBASIC_API FVOParamModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VO") EVOParamKey	Key = EVOParamKey::MaxSpeed;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VO") EVOOp		Op  = EVOOp::Add;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VO") float		Magnitude = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VO") int32		Priority  = 0; // Lower is higher priority

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VO") FName		Tag = NAME_None;
	int32 Id = 0;
};

UCLASS(ClassGroup=AI, meta=(BlueprintSpawnableComponent))
class VOBASIC_API UVOFollowingComponent : public UCrowdFollowingComponent
{
	GENERATED_BODY()

// Fields & Properties
public:
	UVOFollowingComponent();

	virtual void Initialize() override;
	virtual void SetMoveSegment(int32 SegmentStartIndex) override; // Copy-pasted from UE source-code, adapted for UVOController
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VO|Debug")	bool		bDebugDraw = false;
	UPROPERTY(EditAnywhere, Category="VO|Debug")	FColor		DebugDrawColor = FColor::Red;

#ifdef SAVE_VO_PATHS
	TArray<FVector> PathHistory;
	float LastPathSaveTime = 0.0f;
	float PathSaveInterval = 0.1f;
	int PathHistorySize = 100;

	void UpdatePathHistory(float DeltaTime);
#endif
	
	UPROPERTY(EditAnywhere, Category="VO")			FVOParams	Params;
	
	UFUNCTION(BlueprintCallable, Category="VO|Config") void SetVOProfile(FName InProfileName);
	UFUNCTION(BlueprintCallable, Category="VO|Config") void SetVOOverrides(const FVOParams& InOverrides, bool bEnable);

	UFUNCTION(BlueprintCallable, Category="VO|Mods") int32 AddParamModifier(const FVOParamModifier& Mod);
	UFUNCTION(BlueprintCallable, Category="VO|Mods") bool  RemoveParamModifierById(int32 Id);
	UFUNCTION(BlueprintCallable, Category="VO|Mods") int32 RemoveParamModifiersByTag(FName Tag);
	
	UFUNCTION(BlueprintCallable, Category="VO|Mods") void  SetAvoidanceStyle(EAvoidanceStyle NewStyle);
	UFUNCTION(BlueprintCallable, Category="VO|Mods") void  ResetAvoidanceStyle();
	UFUNCTION(BlueprintCallable, Category="VO|Mods") void  SetAgentShape(EVOAgentShape NewShape);
	UFUNCTION(BlueprintCallable, Category="VO|Mods") void  ResetAgentShape();
	UFUNCTION(BlueprintCallable, Category="VO|Mods") void  SetAgentOrientation(EVOOrientation NewOrientation);
	UFUNCTION(BlueprintCallable, Category="VO|Mods") void  ResetAgentOrientation();
	UFUNCTION(BlueprintCallable, Category="VO|Mods") void  SetAgentAOScoring(FAOScoringParams NewAOScoring);
	UFUNCTION(BlueprintCallable, Category="VO|Mods") void  ResetAgentScoring();
	UFUNCTION(BlueprintCallable, Category="VO|Mods") void  SetAgentAOSocialForces(FAOSocialForces NewAOSocialForces);
	UFUNCTION(BlueprintCallable, Category="VO|Mods") void  ResetAgentSocialForces();

	UFUNCTION(BlueprintCallable, Category="VO") const FVOParams& GetEffectiveParams() const;

	// TODO: Delete
	UFUNCTION(BlueprintCallable, Category="VO") void SetMoveGoal(const FVector& InGoal) { bHasGoal = true; Goal = InGoal; }
	// TODO: Delete
	UFUNCTION(BlueprintCallable, Category="VO") void ClearMoveGoal()					{ bHasGoal = false; }

	// TODO: Delete
	UFUNCTION(BlueprintCallable, Category="VO|Debug")
	void SetFakeVelocity(const FVector& InVelocity);

	UFUNCTION(BlueprintCallable, Category="VO|Debug")
	void ClearFakeVelocity();

	
	FVector 		GetOwnerLocation()	const;
	FVector 		GetOwnerVelocity()	const;
	float   		GetAgentRadius()	const	{ return GetEffectiveParams().AgentRadius; }
	float			GetAgentHeight()	const	{ return GetEffectiveParams().AgentHeight; }
	bool			HasVOGoal()			const	{ return bHasGoal; }
	//FVector			GetMoveGoal()		const	{ return Goal; } // TODO: Fix hiding?
	EAvoidanceStyle GetAvoidanceStyle() const	{ return GetEffectiveParams().AvoidanceStyle; }
	void			GetAgentCapsuleSegment(FVector2D& OutP1, FVector2D& OutP2) const;
	void			GetAgentCapsuleSegmentLocal(FVector2D& OutP1, FVector2D& OutP2) const;
	static void		GetAgentCapsuleSegment(const APawn* Pawn, const FVOParams& Params, FVector2D& OutP1, FVector2D& OutP2);
	
	void			UpdateKinematics(float DeltaTime);
	FVector			GetCachedVelocity()		const { return CachedVelocity; }
	FVector			GetCachedAcceleration() const { return CachedAcceleration; }

	// Index in UVOManager arrays (for O(1) access and removal)
	int32			VOManagerIndex = INDEX_NONE;

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;

	virtual void FollowPathSegment(float DeltaTime) override;
	
private:
	// Source config
	UPROPERTY(EditAnywhere, Category="VO|Config")
	FName VOProfile = NAME_None;

	UPROPERTY(EditAnywhere, Category="VO|Config")
	bool bUseOverrides = false;

	UPROPERTY(EditAnywhere, Category="VO|Config")
	FVOParams Overrides;

	UPROPERTY(Transient)
	TArray<FVOParamModifier> ActiveMods;

	mutable FVOParams EffectiveParams;
	mutable bool bEffectiveDirty = true;

	int32 NextModId = 1;
	void MarkEffectiveDirty() { bEffectiveDirty = true; }
	void ApplyModifierTo(FVOParams& P, const FVOParamModifier& M) const;

	bool			bHasAvoidanceStyleOverride	= false;
	EAvoidanceStyle AvoidanceStyleOverride		= EAvoidanceStyle::VelocityObstacle;

	bool			bHasShapeOverride			= false;
	EVOAgentShape	ShapeOverride				= EVOAgentShape::Circle;

	bool			bHasOrientationOverride		= false;
	EVOOrientation	OrientationOverride			= EVOOrientation::Forward;

	bool			bHasScoringOverride			= false;
	FAOScoringParams AOScoringParamsOverride;

	bool			bHasSocialForcesOverride	= false;
	FAOSocialForces AOSocialForcesOverride;
	
	bool    bHasGoal = false;
	FVector Goal     = FVector::ZeroVector;

	// Acceleration cache
	FVector CachedVelocity     = FVector::ZeroVector;
	FVector CachedAcceleration = FVector::ZeroVector;
	bool    bHasPrevVelocity   = false;

	// TODO: Delete
	bool bUseFakeVelocity = false;
	FVector FakeVelocity = FVector::ZeroVector;
};
