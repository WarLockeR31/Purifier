#pragma once

UENUM(BlueprintType)
enum class ECustomCollisionChannels : uint8
{
    Player  UMETA(DisplayName = "Player Channel"),
    PlayerProjectile    UMETA(DisplayName = "Player Projectile Channel")
};

namespace ECustomCollision
{
    UENUM(BlueprintType)
        enum Type
    {
        Player = ECC_GameTraceChannel1,
        PlayerProjectile = ECC_GameTraceChannel2
    };
}