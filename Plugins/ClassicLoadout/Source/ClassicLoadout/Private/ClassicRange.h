#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "ClassicRange.generated.h"

class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class USoundBase;
class UPointLightComponent;

enum class EClassicWeapon : uint8 { AK, USP, M9 };
enum class EClassicAction : uint8 { Ready, Draw, Fire, Slash, Stab, Reload, Inspect, Empty, ModifyUSP };

USTRUCT()
struct FClassicViewArt
{
    GENERATED_BODY()
    UPROPERTY() TObjectPtr<UStaticMesh> Body;
    UPROPERTY() TObjectPtr<UStaticMesh> Magazine;
    UPROPERTY() TObjectPtr<UStaticMesh> SupportHand;
    UPROPERTY() TObjectPtr<USoundBase> Attack;
};

UCLASS()
class AClassicLoadout : public AActor
{
    GENERATED_BODY()
public:
    AClassicLoadout();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    EClassicWeapon Weapon = EClassicWeapon::AK;
    EClassicAction Action = EClassicAction::Ready;
    int32 Ammo() const;
    int32 Reserve() const;
    float HitPulse = 0.f;
    bool bLastTargetDown = false;
    bool IsSuppressed() const { return bSuppressed; }
    int32 TargetsCleared = 0;
    void EquipAK();
    void EquipUSP();
    void EquipM9();
    void PressFire();
    void ReleaseFire();
    void SecondaryAttack();
    void Reload();
    void Inspect();
private:
    UPROPERTY() TObjectPtr<USceneComponent> Rig;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Body;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Magazine;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Slide;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Suppressor;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Grip;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Support;
    UPROPERTY() TObjectPtr<UPointLightComponent> Flash;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Impact;
    UPROPERTY() TArray<FClassicViewArt> Art;
    UPROPERTY() TObjectPtr<USoundBase> EquipSound;
    UPROPERTY() TObjectPtr<USoundBase> ReloadSound;
    UPROPERTY() TObjectPtr<USoundBase> EmptySound;
    UPROPERTY() TObjectPtr<USoundBase> HitSound;
    UPROPERTY() TObjectPtr<class UCameraComponent> Camera;
    int32 Rounds[2] = {30,12};
    int32 Spares[2] = {90,36};
    float Elapsed = 0.f;
    float Duration = 0.f;
    float BobPhase = 0.f;
    float ImpactTime = 0.f;
    bool bHeld = false;
    bool bReverseSlash = false;
    bool bSuppressed = true;
    void Select(EClassicWeapon NewWeapon);
    void NextWeapon();
    void PreviousWeapon();
    void SetAction(EClassicAction NewAction,float Seconds);
    void Attack(bool bHeavy);
    void TraceHit(bool bHeavy);
    void Animate(float DeltaSeconds);
    void Play(USoundBase* Sound,float Volume = 1.f);
    UFUNCTION() void OwnerDestroyed(AActor* Actor);
};

UCLASS()
class AClassicRangeMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AClassicRangeMode();
    virtual void RestartPlayer(AController* Controller) override;
};

UCLASS()
class AClassicRangeTarget : public AActor
{
    GENERATED_BODY()
public:
    AClassicRangeTarget();
    virtual float TakeDamage(float Amount,const FDamageEvent& Event,AController* InstigatorController,AActor* Causer) override;
    virtual void BeginPlay() override;
    bool IsDown() const { return Health <= 0.f; }
private:
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Face;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> FaceMaterial;
    float Health = 100.f;
    FTimerHandle ResetHandle;
    void ResetFace();
};

UCLASS()
class AClassicRangeHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
private:
    TWeakObjectPtr<AClassicLoadout> Loadout;
};
