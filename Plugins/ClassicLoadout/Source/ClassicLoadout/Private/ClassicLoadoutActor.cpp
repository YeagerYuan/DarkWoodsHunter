#include "ClassicRange.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    template<class T> T* FindArt(const FString& Path)
    {
        ConstructorHelpers::FObjectFinder<T> Finder(*Path);
        return Finder.Object;
    }

    constexpr int32 Capacity[] = {30,12};
    int32 Slot(EClassicWeapon Weapon) { return static_cast<int32>(Weapon); }
    float Pulse(float T) { return FMath::Sin(PI * FMath::Clamp(T,0.f,1.f)); }
    float Ease(float T) { return FMath::SmoothStep(0.f,1.f,T); }
}

AClassicLoadout::AClassicLoadout()
{
    PrimaryActorTick.bCanEverTick = true;
    Rig = CreateDefaultSubobject<USceneComponent>(TEXT("View rig"));
    SetRootComponent(Rig);
    auto Part = [this](FName Name)
    {
        auto* PartMesh = CreateDefaultSubobject<UStaticMeshComponent>(Name);
        PartMesh->SetupAttachment(Rig);
        PartMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        PartMesh->SetOnlyOwnerSee(true);
        PartMesh->SetCastShadow(false);
        PartMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
        return PartMesh;
    };
    Body = Part(TEXT("Receiver or blade"));
    Magazine = Part(TEXT("Magazine"));
    Grip = Part(TEXT("Gripping hand"));
    Support = Part(TEXT("Support hand"));
    auto Mesh = [](const FString& Name) { return FindArt<UStaticMesh>(TEXT("/Game/ClassicFPS/Meshes/SM_")+Name); };
    auto Sound = [](const FString& Name) { return FindArt<USoundBase>(TEXT("/Game/ClassicFPS/Audio/S_")+Name); };
    Grip->SetStaticMesh(Mesh(TEXT("RightGlove")));
    Slide=Part(TEXT("Pistol slide"));
    Slide->SetStaticMesh(Mesh(TEXT("USP_Slide")));
    Suppressor=Part(TEXT("Pistol accessory"));
    Suppressor->SetStaticMesh(Mesh(TEXT("USP_Suppressor")));
    Art.SetNum(3);
    Art[0].Body=Mesh(TEXT("AK")); Art[0].Magazine=Mesh(TEXT("AK_Mag")); Art[0].SupportHand=Mesh(TEXT("RifleSupport"));
    Art[1].Body=Mesh(TEXT("USP")); Art[1].Magazine=Mesh(TEXT("USP_Mag")); Art[1].SupportHand=Mesh(TEXT("PistolSupport"));
    Art[2].Body=Mesh(TEXT("M9")); Art[2].SupportHand=Mesh(TEXT("KnifeSupport"));
    Art[0].Attack=Sound(TEXT("Rifle")); Art[1].Attack=Sound(TEXT("Pistol")); Art[2].Attack=Sound(TEXT("Swing"));
    EquipSound=Sound(TEXT("Draw")); ReloadSound=Sound(TEXT("Reload")); EmptySound=Sound(TEXT("Empty")); HitSound=Sound(TEXT("Hit"));

    Flash = CreateDefaultSubobject<UPointLightComponent>(TEXT("Muzzle glow"));
    Flash->SetupAttachment(Rig);
    Flash->SetLightColor(FLinearColor(1,.49f,.15f));
    Flash->SetIntensity(0);
    Flash->SetAttenuationRadius(230);
    Flash->SetCastShadows(false);
    Impact = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Impact spark"));
    Impact->SetupAttachment(Rig);
    Impact->SetStaticMesh(FindArt<UStaticMesh>(TEXT("/Engine/BasicShapes/Sphere")));
    Impact->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Impact->SetCastShadow(false);
    Impact->SetVisibility(false);
    Impact->SetAbsolute(true,true,true);
    Impact->SetWorldScale3D(FVector(.035f));
}

void AClassicLoadout::BeginPlay()
{
    Super::BeginPlay();
    APawn* Pawn = GetInstigator();
    auto* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    if (!PC || !PC->IsLocalPlayerController()) { Destroy(); return; }
    Camera = Pawn->FindComponentByClass<UCameraComponent>();
    if (!Camera) { Destroy(); return; }
    Camera->PostProcessSettings.bOverride_MotionBlurAmount=true;
    Camera->PostProcessSettings.MotionBlurAmount=0;
    Pawn->OnDestroyed.AddDynamic(this,&AClassicLoadout::OwnerDestroyed);
    AttachToComponent(Camera,FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    TInlineComponentArray<USkeletalMeshComponent*> Meshes(Pawn);
    for (auto* Mesh : Meshes)
        if (Mesh->FirstPersonPrimitiveType == EFirstPersonPrimitiveType::FirstPerson) Mesh->SetVisibility(false);

    EnableInput(PC);
    InputComponent->BindKey(EKeys::One,IE_Pressed,this,&AClassicLoadout::EquipAK);
    InputComponent->BindKey(EKeys::Two,IE_Pressed,this,&AClassicLoadout::EquipUSP);
    InputComponent->BindKey(EKeys::Three,IE_Pressed,this,&AClassicLoadout::EquipM9);
    InputComponent->BindKey(EKeys::MouseScrollUp,IE_Pressed,this,&AClassicLoadout::PreviousWeapon);
    InputComponent->BindKey(EKeys::MouseScrollDown,IE_Pressed,this,&AClassicLoadout::NextWeapon);
    InputComponent->BindKey(EKeys::LeftMouseButton,IE_Pressed,this,&AClassicLoadout::PressFire);
    InputComponent->BindKey(EKeys::LeftMouseButton,IE_Released,this,&AClassicLoadout::ReleaseFire);
    InputComponent->BindKey(EKeys::RightMouseButton,IE_Pressed,this,&AClassicLoadout::SecondaryAttack);
    InputComponent->BindKey(EKeys::R,IE_Pressed,this,&AClassicLoadout::Reload);
    InputComponent->BindKey(EKeys::F,IE_Pressed,this,&AClassicLoadout::Inspect);
    Select(Weapon);
    Animate(0);
}

void AClassicLoadout::OwnerDestroyed(AActor*) { Destroy(); }
void AClassicLoadout::EquipAK() { if (Weapon!=EClassicWeapon::AK) Select(EClassicWeapon::AK); }
void AClassicLoadout::EquipUSP() { if (Weapon!=EClassicWeapon::USP) Select(EClassicWeapon::USP); }
void AClassicLoadout::EquipM9() { if (Weapon!=EClassicWeapon::M9) Select(EClassicWeapon::M9); }
void AClassicLoadout::NextWeapon() { Select(static_cast<EClassicWeapon>((Slot(Weapon)+1)%3)); }
void AClassicLoadout::PreviousWeapon() { Select(static_cast<EClassicWeapon>((Slot(Weapon)+2)%3)); }
int32 AClassicLoadout::Ammo() const { return Slot(Weapon)<2 ? Rounds[Slot(Weapon)] : 0; }
int32 AClassicLoadout::Reserve() const { return Slot(Weapon)<2 ? Spares[Slot(Weapon)] : 0; }

void AClassicLoadout::Select(EClassicWeapon NewWeapon)
{
    bHeld=false;
    Weapon=NewWeapon;
    const FClassicViewArt& View=Art[Slot(Weapon)];
    Body->SetStaticMesh(View.Body);
    Magazine->SetStaticMesh(View.Magazine);
    Support->SetStaticMesh(View.SupportHand);
    Flash->SetRelativeLocation(Weapon==EClassicWeapon::AK ? FVector(52,0,.6f) : FVector(29,0,1.6));
    SetAction(EClassicAction::Draw,Weapon==EClassicWeapon::AK ? .65f : .45f);
    Play(EquipSound,.48f);
}

void AClassicLoadout::SetAction(EClassicAction Next,float Seconds)
{
    Action=Next;
    Duration=Seconds;
    Elapsed=0;
}

void AClassicLoadout::PressFire() { bHeld=true; Attack(false); }
void AClassicLoadout::ReleaseFire() { bHeld=false; }
void AClassicLoadout::SecondaryAttack()
{
    if (Weapon==EClassicWeapon::M9) Attack(true);
    if (Weapon==EClassicWeapon::USP && (Action==EClassicAction::Ready || Action==EClassicAction::Inspect))
    {
        SetAction(EClassicAction::ModifyUSP,1.1f);
        Play(EquipSound,.4f);
    }
}

void AClassicLoadout::Attack(bool bHeavy)
{
    if (Action!=EClassicAction::Ready && Action!=EClassicAction::Inspect) return;
    if (Weapon==EClassicWeapon::M9)
    {
        bReverseSlash=!bReverseSlash;
        SetAction(bHeavy ? EClassicAction::Stab : EClassicAction::Slash,bHeavy ? .74f : .40f);
    }
    else
    {
        if (Ammo()==0)
        {
            bHeld=false;
            Play(EmptySound,.55f);
            SetAction(EClassicAction::Empty,.20f);
            return;
        }
        --Rounds[Slot(Weapon)];
        SetAction(EClassicAction::Fire,Weapon==EClassicWeapon::AK ? .105f : .17f);
        TraceHit(false);
    }
    if (Weapon==EClassicWeapon::USP && !bSuppressed)
        Play(Art[0].Attack,.62f);
    else
        Play(Art[Slot(Weapon)].Attack,Weapon==EClassicWeapon::USP ? .58f : .79f);
}

void AClassicLoadout::TraceHit(bool bHeavy)
{
    const bool bMelee=Weapon==EClassicWeapon::M9;
    FVector Start; FRotator Aim;
    GetInstigatorController()->GetPlayerViewPoint(Start,Aim);
    const FVector End=Start+Aim.Vector()*(bMelee ? 155.f : 18000.f);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(ClassicAttack),false,GetInstigator());
    Params.AddIgnoredActor(this);
    FHitResult Hit;
    const bool bHit=bMelee
        ? GetWorld()->SweepSingleByChannel(Hit,Start,End,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(11),Params)
        : GetWorld()->LineTraceSingleByChannel(Hit,Start,End,ECC_Visibility,Params);
    if (!bHit || !Hit.GetActor()) return;
    const float Amount=bMelee ? (bHeavy ? 100.f : 50.f) : (Weapon==EClassicWeapon::AK ? 36.f : 26.f);
    const float Applied=UGameplayStatics::ApplyPointDamage(Hit.GetActor(),Amount,Aim.Vector(),Hit,GetInstigatorController(),this,nullptr);
    auto* Target=Cast<AClassicRangeTarget>(Hit.GetActor());
    if (Applied>0 && (Target || Hit.GetActor()->IsA<APawn>()))
    {
        HitPulse=.19f;
        bLastTargetDown=Target && Target->IsDown();
        if (bLastTargetDown) ++TargetsCleared;
        Play(HitSound,.4f);
    }
    if (auto* Comp=Hit.GetComponent(); Comp && Comp->IsSimulatingPhysics())
        Comp->AddImpulseAtLocation(Aim.Vector()*14000.f,Hit.ImpactPoint);
    Impact->SetWorldLocation(Hit.ImpactPoint+Hit.ImpactNormal*1.1f);
    Impact->SetVisibility(true);
    ImpactTime=.083f;
}

void AClassicLoadout::Reload()
{
    if (Weapon==EClassicWeapon::M9 || Action==EClassicAction::Draw || Action==EClassicAction::Reload || Action==EClassicAction::ModifyUSP) return;
    if (Ammo()==Capacity[Slot(Weapon)] || Reserve()==0) return;
    bHeld=false;
    SetAction(EClassicAction::Reload,Weapon==EClassicWeapon::AK ? 2.1f : 1.48f);
    Play(ReloadSound,.63f);
}

void AClassicLoadout::Inspect()
{
    if (Action==EClassicAction::Ready) SetAction(EClassicAction::Inspect,Weapon==EClassicWeapon::M9 ? 2.15f : 1.85f);
}

void AClassicLoadout::Play(USoundBase* Sound,float Volume)
{
    if (Sound) UGameplayStatics::PlaySound2D(this,Sound,Volume);
}

void AClassicLoadout::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!Camera || !GetInstigatorController()) return;
    const float Previous=Elapsed;
    Elapsed+=DeltaSeconds;
    const bool bStab=Action==EClassicAction::Stab;
    const float Contact=bStab ? .24f : .12f;
    if ((bStab || Action==EClassicAction::Slash) && Previous<Contact && Elapsed>=Contact) TraceHit(bStab);
    if (Action!=EClassicAction::Ready && Elapsed>=Duration)
    {
        if (Action==EClassicAction::ModifyUSP) bSuppressed=!bSuppressed;
        if (Action==EClassicAction::Reload)
        {
            const int32 Index=Slot(Weapon);
            const int32 Transfer=FMath::Min(Capacity[Index]-Rounds[Index],Spares[Index]);
            Rounds[Index]+=Transfer;
            Spares[Index]-=Transfer;
        }
        SetAction(EClassicAction::Ready,0);
        if (bHeld && Weapon==EClassicWeapon::AK) Attack(false);
    }
    HitPulse=FMath::Max(0.f,HitPulse-DeltaSeconds);
    ImpactTime-=DeltaSeconds;
    if (Impact->IsVisible() && ImpactTime<=0) Impact->SetVisibility(false);
    Animate(DeltaSeconds);
}

void AClassicLoadout::Animate(float Dt)
{
    const bool Rifle=Weapon==EClassicWeapon::AK;
    const bool Knife=Weapon==EClassicWeapon::M9;
    const float T=Duration>0 ? FMath::Clamp(Elapsed/Duration,0.f,1.f) : 0.f;
    const float Move=FMath::Clamp(GetInstigator()->GetVelocity().Size2D()/550.f,0.f,1.f);
    BobPhase+=Dt*(2.1f+Move*7.5f);
    FVector Pos=Rifle ? FVector(36,11,-9.5) : (Knife ? FVector(35,10,-7.6) : FVector(38,9,-8.8));
    FRotator Rot=Rifle ? FRotator(3,-8,-3) : (Knife ? FRotator(12,-18,-16) : FRotator(3,-7,-4));
    Pos.Y+=FMath::Sin(BobPhase)*(.15f+.65f*Move);
    Pos.Z+=FMath::Cos(2*BobPhase)*(.11f+.39f*Move);
    FVector MagOffset=FVector::ZeroVector, HandOffset=FVector::ZeroVector;
    FRotator MagRot=FRotator::ZeroRotator, HandRot=FRotator::ZeroRotator;
    switch (Action)
    {
        case EClassicAction::Draw:
        {
            const float P=1-Ease(T);
            Pos+=FVector(-7,7,-19)*P;
            Rot+=FRotator(Knife ? -42 : -25,Knife ? 47 : 17,Knife ? -76 : 24)*P;
            break;
        }
        case EClassicAction::Fire:
        {
            const float P=Pulse(FMath::Pow(T,.65f));
            Pos.X-=P*(Rifle ? 3.2f : 1.8f);
            Rot.Pitch+=P*(Rifle ? 4.f : 7.f);
            Rot.Roll+=P*(Rifle ? -2.f : 1.f);
            break;
        }
        case EClassicAction::Slash:
        {
            const float Cut=Pulse(Ease(T));
            const float Wind=Pulse(T/.29f);
            const float Sign=bReverseSlash ? 1.f : -1.f;
            Pos+=FVector(5*Cut,Sign*(6*Wind-13*Cut),2*Cut);
            Rot+=FRotator(9*Wind-13*Cut,Sign*(26*Wind-65*Cut),Sign*31*Cut);
            break;
        }
        case EClassicAction::Stab:
        {
            const float P=Pulse(Ease((T-.19f)/.81f));
            Pos+=FVector(19*P-5*Pulse(T/.3f),-5*P,3*P);
            Rot+=FRotator(-8*P,10*P,48*P);
            break;
        }
        case EClassicAction::Inspect:
        {
            const float Hold=Ease(T/.24f)*(1-Ease((T-.75f)/.25f));
            Pos+=FVector(3,-8,5)*Hold;
            Rot+=FRotator(Knife ? 30 : 19,Knife ? -52 : -34,(Knife ? 64 : 49)+12*FMath::Sin(T*2*PI))*Hold;
            break;
        }
        case EClassicAction::ModifyUSP:
        {
            const float Hold=Ease(T/.22f)*(1-Ease((T-.76f)/.24f));
            Pos+=FVector(-3,-4,2)*Hold;
            Rot+=FRotator(16,-12,-29)*Hold;
            HandOffset=FVector(17,-1,6)*Hold;
            HandRot.Roll=12*FMath::Sin(T*PI*4)*Hold;
            break;
        }
        case EClassicAction::Reload:
        {
            const float Tilt=Ease(T/.22f)*(1-Ease((T-.78f)/.22f));
            const float Out=Ease((T-.20f)/.21f)*(1-Ease((T-.51f)/.23f));
            Pos+=FVector(-2,-3,2)*Tilt;
            Rot+=FRotator(Rifle ? 12 : 24,-14,-33)*Tilt;
            MagOffset=FVector(Rifle ? -5 : -2,-2,-17)*Out;
            MagRot.Pitch=(Rifle ? -18 : -4)*Out;
            HandOffset=FVector(Rifle ? -13 : 0,1,-4)*Tilt+MagOffset;
            HandRot.Roll=10*Out;
            break;
        }
        default: break;
    }
    Rig->SetRelativeLocationAndRotation(Pos,Rot);
    Magazine->SetRelativeLocationAndRotation(MagOffset,MagRot);
    Support->SetRelativeLocationAndRotation(HandOffset,HandRot);
    const bool Pistol=Weapon==EClassicWeapon::USP;
    Slide->SetVisibility(Pistol);
    const float SlideTravel=(Pistol && Ammo()==0) ? 1.f : (Action==EClassicAction::Fire ? Pulse(FMath::Min(T*2,1.f)) : 0.f);
    Slide->SetRelativeLocation(FVector(-2.4f*SlideTravel,0,0));
    const float Travel=Action==EClassicAction::ModifyUSP ? (bSuppressed ? Ease((T-.30f)/.42f) : 1-Ease((T-.30f)/.42f)) : (bSuppressed ? 0.f : 1.f);
    Suppressor->SetRelativeLocation(FVector(5,-1,-2)*Travel);
    Suppressor->SetVisibility(Pistol && Travel<.99f);
    Grip->SetRelativeLocation(Knife ? FVector(0,0,4.6) : FVector::ZeroVector);
    Flash->SetIntensity(Action==EClassicAction::Fire && Elapsed<.045f ? (Rifle || !bSuppressed ? 1250 : 170) : 0);
}
