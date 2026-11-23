#if WITH_DEV_AUTOMATION_TESTS

#include "ClassicRange.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "InputKeyEventArgs.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClassicRangeTest,"DarkWoods.ClassicFPS.PlayableRange",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

namespace
{
    class FExerciseLoadout final : public IAutomationLatentCommand
    {
        FAutomationTestBase& Test;
        int32 Step=0;
        double Next=0;
        int32 AfterBurst=0;
        bool bPoseCaptured=false;
        TWeakObjectPtr<AClassicRangeTarget> Target;
    public:
        explicit FExerciseLoadout(FAutomationTestBase& InTest) : Test(InTest) {}
        virtual bool Update() override
        {
            UWorld* World=GEngine->GetCurrentPlayWorld();
            if (!World || !World->IsGameWorld()) { Test.AddError(TEXT("Run in the standalone ClassicRange map.")); return true; }
            if (World->GetTimeSeconds()<Next) return false;
            auto* PC=World->GetFirstPlayerController();
            TActorIterator<AClassicLoadout> It(World);
            if (!It || !PC || !PC->GetPawn()) { Test.AddError(TEXT("The default map did not possess a pawn with a loadout.")); return true; }
            auto* L=*It;
            auto Key=[&](FKey K,EInputEvent E)
            {
                PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,E,FPlatformTime::Cycles64()));
            };
            auto Tap=[&](FKey K) { Key(K,IE_Pressed); Key(K,IE_Released); };
            auto Shot=[&](const TCHAR* Name)
            {
                if (FParse::Param(FCommandLine::Get(),TEXT("ClassicScreens")))
                    FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots")/Name,false,false);
            };
            auto Aim=[&]()
            {
                FVector Eye; FRotator Old;
                PC->GetPlayerViewPoint(Eye,Old);
                PC->SetControlRotation((Target->GetActorLocation()-Eye).Rotation());
            };
            float Wait=.25f;
            if (!bPoseCaptured && (Step==1 || Step==6 || Step==10))
            {
                Shot(Step==1 ? TEXT("ClassicAK.png") : (Step==6 ? TEXT("ClassicUSP.png") : TEXT("ClassicM9.png")));
                bPoseCaptured=true;
                Next=World->GetTimeSeconds()+.25f;
                return false;
            }
            bPoseCaptured=false;
            switch (Step++)
            {
            case 0:
                for (TActorIterator<AClassicRangeTarget> T(World); T; ++T)
                    if (!Target.IsValid() || T->GetActorLocation().SizeSquared2D()<Target->GetActorLocation().SizeSquared2D()) Target=*T;
                if (!Target.IsValid()) { Test.AddError(TEXT("No damageable target in map.")); return true; }
                Test.TestTrue(TEXT("Classic HUD is active"),PC->GetHUD() && PC->GetHUD()->IsA<AClassicRangeHUD>());
                Wait=1.f;
                break;
            case 1: Aim(); break;
            case 2: Key(EKeys::LeftMouseButton,IE_Pressed); Wait=.55f; break;
            case 3:
                Key(EKeys::LeftMouseButton,IE_Released);
                AfterBurst=L->Ammo();
                Test.TestTrue(TEXT("Holding AK fires multiple rounds"),AfterBurst<=27 && AfterBurst>0);
                Test.TestTrue(TEXT("Aimed shots score a target"),L->TargetsCleared==1);
                break;
            case 4: Tap(EKeys::R); break;
            case 5:
                Test.TestTrue(TEXT("Reload entered"),L->Action==EClassicAction::Reload);
                Tap(EKeys::Two); Wait=.7f; break;
            case 6:
                Test.TestTrue(TEXT("USP selected with full independent magazine"),L->Weapon==EClassicWeapon::USP && L->Ammo()==12);
                Key(EKeys::LeftMouseButton,IE_Pressed); Wait=.6f; break;
            case 7:
                Test.TestEqual(TEXT("Held USP fires exactly once"),L->Ammo(),11);
                Key(EKeys::LeftMouseButton,IE_Released); Tap(EKeys::R); Wait=1.8f; break;
            case 8:
                Test.TestTrue(TEXT("Reload transfers one round from reserve"),L->Ammo()==12 && L->Reserve()==35);
                Tap(EKeys::One); Wait=.82f; break;
            case 9:
                Test.TestEqual(TEXT("Interrupted rifle reload preserves rounds"),L->Ammo(),AfterBurst);
                Test.TestEqual(TEXT("Interrupted reload preserves reserve"),L->Reserve(),90);
                Tap(EKeys::Three); Wait=.65f; break;
            case 10:
                Test.TestTrue(TEXT("M9 selected"),L->Weapon==EClassicWeapon::M9);
                Tap(EKeys::F); break;
            case 11:
                Test.TestTrue(TEXT("Inspect input plays action"),L->Action==EClassicAction::Inspect);
                Tap(EKeys::RightMouseButton); Wait=.85f; break;
            case 12:
                Test.TestTrue(TEXT("Knife cannot reach distant target"),L->TargetsCleared==1 && !Target->IsDown());
                PC->GetPawn()->SetActorLocation(Target->GetActorLocation()+FVector(-105,0,96-Target->GetActorLocation().Z));
                break;
            case 13: Aim(); break;
            case 14: Tap(EKeys::LeftMouseButton); Wait=.48f; break;
            case 15: Tap(EKeys::LeftMouseButton); Wait=.48f; break;
            case 16:
                Test.TestTrue(TEXT("Two light knife hits clear a close target"),L->TargetsCleared==2 && Target->IsDown());
                Wait=1.7f; break;
            case 17:
                Test.TestFalse(TEXT("Target resets"),Target->IsDown());
                Tap(EKeys::RightMouseButton); Wait=.80f; break;
            case 18:
                Test.TestTrue(TEXT("Heavy knife attack clears reset target"),L->TargetsCleared==3 && Target->IsDown());
                Tap(EKeys::Two); Wait=.65f; break;
            case 19: Tap(EKeys::RightMouseButton); Wait=1.4f; break;
            case 20:
                Test.TestFalse(TEXT("USP accessory detaches"),L->IsSuppressed());
                Tap(EKeys::RightMouseButton); Wait=1.4f; break;
            case 21:
                Test.TestTrue(TEXT("USP accessory reattaches"),L->IsSuppressed());
                return true;
            }
            Next=World->GetTimeSeconds()+Wait;
            return false;
        }
    };
}

bool FClassicRangeTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(FExerciseLoadout(*this));
    return true;
}
#endif
