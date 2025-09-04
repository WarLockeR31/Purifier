#pragma once
#include "CoreMinimal.h"
#include "Navigation/PathFollowingComponent.h"
#include "VOFollowingComponent.generated.h"

class UVOSettings;

USTRUCT(BlueprintType)
struct FVOParams
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VO") float TauHorizon		= 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VO") float MaxSpeed		= 400.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VO") float NeighborRange	= 600.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VO") float AgentRadius	= 34.f;
};

UENUM(BlueprintType)
enum class EVOParamKey : uint8
{
	TauHorizon,
	MaxSpeed,
	NeighborRange,
	AgentRadius,
};

UENUM(BlueprintType)
enum class EVOOp : uint8 { Add, Mul, Set };

USTRUCT(BlueprintType)
struct VOBASIC_API FVOParamModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VO") EVOParamKey Key = EVOParamKey::MaxSpeed;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VO") EVOOp      Op  = EVOOp::Add;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VO") float     Magnitude = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VO") int32     Priority  = 0; // Lower is higher priority

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VO") FName     Tag = NAME_None;
	int32 Id = 0;
};

UCLASS(ClassGroup=AI, meta=(BlueprintSpawnableComponent))
class VOBASIC_API UVOFollowingComponent : public UPathFollowingComponent
{
	GENERATED_BODY()

// Fields & Properties
public:
	UVOFollowingComponent();
	
	UPROPERTY(EditAnywhere, Category="VO|Debug")	bool		bDebugDraw = false;
	UPROPERTY(EditAnywhere, Category="VO|Debug")	FColor		DebugDrawColor = FColor::Red;
	UPROPERTY(EditAnywhere, Category="VO")			FVOParams	Params;
	
	UFUNCTION(BlueprintCallable, Category="VO|Config") void SetVOProfile(FName InProfileName);
	UFUNCTION(BlueprintCallable, Category="VO|Config") void SetVOOverrides(const FVOParams& InOverrides, bool bEnable);

	UFUNCTION(BlueprintCallable, Category="VO|Mods") int32 AddParamModifier(const FVOParamModifier& Mod);
	UFUNCTION(BlueprintCallable, Category="VO|Mods") bool  RemoveParamModifierById(int32 Id);
	UFUNCTION(BlueprintCallable, Category="VO|Mods") int32 RemoveParamModifiersByTag(FName Tag);

	UFUNCTION(BlueprintCallable, Category="VO") const FVOParams& GetEffectiveParams() const;
	
	UFUNCTION(BlueprintCallable, Category="VO") void SetMoveGoal(const FVector& InGoal) { bHasGoal = true; Goal = InGoal; }
	UFUNCTION(BlueprintCallable, Category="VO") void ClearMoveGoal()					{ bHasGoal = false; }	

	FVector GetOwnerLocation()	const;
	FVector GetOwnerVelocity()	const;
	float   GetAgentRadius()	const	{ return GetEffectiveParams().AgentRadius; }
	bool	HasVOGoal()			const	{ return bHasGoal; }
	FVector GetMoveGoal()		const	{ return Goal; } // TODO: Fix hiding?

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	
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

	
	bool    bHasGoal = false;
	FVector Goal     = FVector::ZeroVector;
};
