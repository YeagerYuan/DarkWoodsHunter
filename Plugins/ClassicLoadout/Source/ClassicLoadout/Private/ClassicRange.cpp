#include "ClassicRange.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AClassicRangeMode::AClassicRangeMode()
{
    ConstructorHelpers::FClassFinder<APawn> PawnBP(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"));
    ConstructorHelpers::FClassFinder<APlayerController> ControllerBP(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonPlayerController"));
    if (PawnBP.Succeeded()) DefaultPawnClass=PawnBP.Class;
    if (ControllerBP.Succeeded()) PlayerControllerClass=ControllerBP.Class;
    HUDClass=AClassicRangeHUD::StaticClass();
}

void AClassicRangeMode::RestartPlayer(AController* Controller)
{
    Super::RestartPlayer(Controller);
    APawn* Pawn=Controller ? Controller->GetPawn() : nullptr;
    if (!Pawn || !Pawn->IsLocallyControlled()) return;
    TArray<AActor*> Attachments;
    Pawn->GetAttachedActors(Attachments);
    for (AActor* Child : Attachments)
        if (Child->IsA<AClassicLoadout>()) return;
    FActorSpawnParameters Args;
    Args.Owner=Pawn;
    Args.Instigator=Pawn;
    Args.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    GetWorld()->SpawnActor<AClassicLoadout>(AClassicLoadout::StaticClass(),Pawn->GetActorTransform(),Args);
}

AClassicRangeTarget::AClassicRangeTarget()
{
    auto* Mount=CreateDefaultSubobject<USceneComponent>(TEXT("Mount"));
    SetRootComponent(Mount);
    ConstructorHelpers::FObjectFinder<UStaticMesh> Disc(TEXT("/Engine/BasicShapes/Cylinder"));
    ConstructorHelpers::FObjectFinder<UMaterialInterface> Paint(TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
    for (int32 I=0; I<5; ++I)
    {
        const FName Name(*FString::Printf(TEXT("Disc%d"),I));
        auto* Ring=CreateDefaultSubobject<UStaticMeshComponent>(Name);
        Ring->SetupAttachment(Mount);
        Ring->SetStaticMesh(Disc.Object);
        Ring->SetMaterial(0,Paint.Object);
        const float Radius[]={.91f,.77f,.60f,.39f,.17f};
        Ring->SetRelativeScale3D(FVector(Radius[I],Radius[I],I==0 ? .075f : .009f));
        Ring->SetRelativeRotation(FRotator(90,0,0));
        Ring->SetRelativeLocation(FVector(I==0 ? 0.f : -4.2f-(I-1)*.85f,0,0));
        Ring->SetCollisionProfileName(I==0 ? FName(TEXT("BlockAll")) : FName(TEXT("NoCollision")));
        if (I==0) Face=Ring;
    }
}
void AClassicRangeTarget::BeginPlay()
{
    Super::BeginPlay();
    const FLinearColor Colors[]={{.05f,.08f,.085f},{.70f,.61f,.42f},{.12f,.16f,.15f},{.75f,.64f,.45f},{.94f,.21f,.05f}};
    TInlineComponentArray<UStaticMeshComponent*> Rings(this);
    for (auto* Ring : Rings)
    {
        const int32 Index=FCString::Atoi(*Ring->GetName().Right(1));
        if (auto* Mat=Ring->CreateDynamicMaterialInstance(0))
        {
            Mat->SetVectorParameterValue(TEXT("Color"),Colors[FMath::Clamp(Index,0,4)]);
            if (Index==4) FaceMaterial=Mat;
        }
    }
}
float AClassicRangeTarget::TakeDamage(float Amount,const FDamageEvent& Event,AController* Controller,AActor* Causer)
{
    if (Amount<=0 || IsDown()) return 0;
    const float Applied=Super::TakeDamage(Amount,Event,Controller,Causer);
    Health=FMath::Max(0.f,Health-Applied);
    if (FaceMaterial) FaceMaterial->SetVectorParameterValue(TEXT("Color"),FLinearColor(1,.78f,.25f));
    if (IsDown())
    {
        SetActorHiddenInGame(true);
        SetActorEnableCollision(false);
    }
    GetWorldTimerManager().SetTimer(ResetHandle,this,&AClassicRangeTarget::ResetFace,IsDown() ? 1.5f : .18f,false);
    return Applied;
}
void AClassicRangeTarget::ResetFace()
{
    if (IsDown()) Health=100;
    SetActorHiddenInGame(false);
    SetActorEnableCollision(true);
    if (FaceMaterial) FaceMaterial->SetVectorParameterValue(TEXT("Color"),FLinearColor(.94f,.21f,.05f));
}

void AClassicRangeHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!Canvas || !PlayerOwner || !PlayerOwner->GetPawn()) return;
    if (!Loadout.IsValid() || Loadout->GetOwner()!=PlayerOwner->GetPawn())
    {
        TArray<AActor*> Attachments;
        PlayerOwner->GetPawn()->GetAttachedActors(Attachments);
        for (auto* Child : Attachments) if (auto* Found=Cast<AClassicLoadout>(Child)) { Loadout=Found; break; }
    }
    if (!Loadout.IsValid()) return;
    const auto& L=*Loadout.Get();
    const float W=Canvas->ClipX,H=Canvas->ClipY,S=FMath::Clamp(FMath::Min(W/1280.f,H/720.f),.55f,2.1f);
    const FLinearColor White(.94f,.95f,.91f),Accent(1,.63f,.24f),Muted(.71f,.79f,.74f),Panel(.009f,.018f,.019f,.82f);
    const bool Knife=L.Weapon==EClassicWeapon::M9;
    const FString Name=Knife ? TEXT("M9 BAYONET") : (L.Weapon==EClassicWeapon::AK ? TEXT("AK-47") : (L.IsSuppressed() ? TEXT("USP-S") : TEXT("USP")));
    const float X=W*.5f,Y=H*.5f;
    const float Gap=(L.Action==EClassicAction::Fire ? 7.5f : 4.5f)*S;
    for (const FVector2D& Dir : {FVector2D(1,0),FVector2D(-1,0),FVector2D(0,1),FVector2D(0,-1)})
    {
        const FVector2D A=FVector2D(X,Y)+Dir*Gap,B=A+Dir*5*S;
        DrawLine(A.X,A.Y,B.X,B.Y,FLinearColor::Black,3*S);
        DrawLine(A.X,A.Y,B.X,B.Y,White,1.3*S);
    }
    if (L.HitPulse>0)
        for (const FVector2D& Dir : {FVector2D(1,1),FVector2D(1,-1),FVector2D(-1,-1),FVector2D(-1,1)})
            DrawLine(X+Dir.X*7*S,Y+Dir.Y*7*S,X+Dir.X*13*S,Y+Dir.Y*13*S,L.bLastTargetDown ? Accent : White,2*S);
    auto Label=[&](const FString& Text,float PX,float PY,const FLinearColor& Color,float Size)
    {
        DrawText(Text,Color,PX,PY,GEngine->GetSmallFont(),Size*S);
    };
    const float BX=W-226*S,BY=H-96*S;
    DrawRect(Panel,BX,BY,204*S,72*S);
    DrawRect(Accent,BX,BY,3*S,72*S);
    Label(Name,BX+15*S,BY+6*S,White,1.42f);
    const FString Count=Knife ? TEXT("MELEE") : FString::Printf(TEXT("%02d  /  %02d"),L.Ammo(),L.Reserve());
    Label(Count,BX+15*S,BY+36*S,Accent,1.32f);
    if (L.Action==EClassicAction::Reload) Label(TEXT("RELOAD"),BX+116*S,BY+39*S,Muted,.89f);
    if (!Knife && L.Ammo()==0 && L.Action!=EClassicAction::Reload) Label(TEXT("EMPTY"),BX+125*S,BY+39*S,Accent,.89f);
    DrawRect(Panel,22*S,22*S,173*S,36*S);
    Label(FString::Printf(TEXT("TARGETS   /   %02d"),L.TargetsCleared),35*S,29*S,White,1.08f);
    DrawRect(Panel,22*S,H-73*S,315*S,49*S);
    Label(TEXT("1   AK-47      2   USP-S      3   M9"),34*S,H-70*S,White,1.f);
    const TCHAR* GunKeys=L.Weapon==EClassicWeapon::USP ? TEXT("LMB Fire   RMB Suppressor   R Reload   F Inspect") : TEXT("LMB  Fire     R  Reload     F  Inspect");
    Label(Knife ? TEXT("LMB  Slash     RMB  Stab     F  Inspect") : GunKeys,34*S,H-48*S,Muted,.93f);
}
