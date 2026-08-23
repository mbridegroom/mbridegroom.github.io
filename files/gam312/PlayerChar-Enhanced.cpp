// Matthew BridegroomS
// July 26, 2026


#include "PlayerChar.h"
#include "BuildingPart.h"
#include "Resource_M.h"
#include "Components/CameraComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"

// Sets default values
APlayerChar::APlayerChar()
{
    PrimaryActorTick.bCanEverTick = true;

    // Camera Setup
    PlayerCamComp = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
    PlayerCamComp->SetupAttachment(GetMesh(), TEXT("head"));
    PlayerCamComp->bUsePawnControlRotation = true;

    // Initialize Arrays
    ResourcesArray.Init(0, 3);
    BuildingArray.Init(0, 3);

    ResourcesNameArray.Add(TEXT("Wood"));
    ResourcesNameArray.Add(TEXT("Stone"));
    ResourcesNameArray.Add(TEXT("Berry"));

    // Default values
    Health = 100.0f;
    Hunger = 100.0f;
    Stamina = 100.0f;
    GridSize = 50.0f;
    BuildTraceDistance = 400.0f;
    bUseGridSnap = true;
    bCanPlace = false;
    isBuilding = false;
    isCrafting = false;
    objectsBuilt = 0.0f;
    matsCollected = 0.0f;
    BuildingTypeBeingPlaced = 0;
    spawnedPart = nullptr;
}

void APlayerChar::BeginPlay()
{
    Super::BeginPlay();

    // Stats decrease timer
    GetWorld()->GetTimerManager().SetTimer(StatsTimerHandle, this, &APlayerChar::DecreaseStats, 2.0f, true);

    if (objWidget)
    {
        objWidget->UpdatebuildObj(0.0f);
        objWidget->UpdatematOBJ(0.0f);
    }
}

// Called every frame
void APlayerChar::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Update UI
    if (PlayerUI)
    {
        PlayerUI->UpdateBars(Health, Hunger, Stamina);
    }

    // Handle building preview
    if (isBuilding && spawnedPart)
    {
        UpdateBuildingPreview();
    }
}

void APlayerChar::UpdateBuildingPreview()
{
    if (!PlayerCamComp || !spawnedPart) return;

    FHitResult HitResult;
    FVector StartLocation = PlayerCamComp->GetComponentLocation();
    FVector EndLocation = StartLocation + PlayerCamComp->GetForwardVector() * BuildTraceDistance;

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    QueryParams.AddIgnoredActor(spawnedPart);

    FVector TargetLocation = EndLocation;

    if (GetWorld()->LineTraceSingleByChannel(HitResult, StartLocation, EndLocation, ECC_Visibility, QueryParams))
    {
        TargetLocation = HitResult.Location;
    }

    // Grid Snapping
    FVector SnappedLocation = TargetLocation;
    if (bUseGridSnap)
    {
        SnappedLocation.X = FMath::RoundToFloat(TargetLocation.X / GridSize) * GridSize;
        SnappedLocation.Y = FMath::RoundToFloat(TargetLocation.Y / GridSize) * GridSize;
        SnappedLocation.Z = HitResult.bBlockingHit ? HitResult.Location.Z : TargetLocation.Z;
    }

    spawnedPart->SetActorLocation(SnappedLocation);

    // Placement Validation
    FCollisionShape Box = FCollisionShape::MakeBox(FVector(50.f, 50.f, 50.f));
    FHitResult OverlapHit;

    bool bBlockingHit = GetWorld()->SweepSingleByChannel(
        OverlapHit,
        SnappedLocation,
        SnappedLocation,
        FQuat::Identity,
        ECC_WorldStatic,
        Box
    );

    bCanPlace = !bBlockingHit;

    // Visual Feedback
    if (UStaticMeshComponent* SpawnedMesh = spawnedPart->FindComponentByClass<UStaticMeshComponent>())
    {
        FVector Color = bCanPlace ? FVector(0.0f, 1.0f, 0.0f) : FVector(1.0f, 0.0f, 0.0f);
        SpawnedMesh->SetVectorParameterValueOnMaterials(TEXT("Color"), Color);
    }
}

// Called to bind functionality to input
void APlayerChar::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    // Movement
    PlayerInputComponent->BindAxis("MoveForward", this, &APlayerChar::MoveForward);
    PlayerInputComponent->BindAxis("MoveRight", this, &APlayerChar::MoveRight);

    // Camera
    PlayerInputComponent->BindAxis("LookUp", this, &APlayerChar::AddControllerPitchInput);
    PlayerInputComponent->BindAxis("Turn", this, &APlayerChar::AddControllerYawInput);

    // Actions
    PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &APlayerChar::StartJump);
    PlayerInputComponent->BindAction("Jump", IE_Released, this, &APlayerChar::StopJump);

    PlayerInputComponent->BindAction("Interact", IE_Pressed, this, &APlayerChar::FindObject);
    PlayerInputComponent->BindAction("RotPart", IE_Pressed, this, &APlayerChar::RotateBuilding);
    PlayerInputComponent->BindAction("ToggleSnap", IE_Pressed, this, &APlayerChar::ToggleGridSnap);
    PlayerInputComponent->BindAction("CancelBuild", IE_Pressed, this, &APlayerChar::CancelBuilding);
}


// Movement

void APlayerChar::MoveForward(float AxisValue)
{
    if (AxisValue != 0.0f)
    {
        FRotator Rotation = GetControlRotation();
        FVector Direction = FRotationMatrix(Rotation).GetScaledAxis(EAxis::X);
        AddMovementInput(Direction, AxisValue);
    }
}

void APlayerChar::MoveRight(float AxisValue)
{
    if (AxisValue != 0.0f)
    {
        FRotator Rotation = GetControlRotation();
        FVector Direction = FRotationMatrix(Rotation).GetScaledAxis(EAxis::Y);
        AddMovementInput(Direction, AxisValue);
    }
}

void APlayerChar::StartJump()
{
    bPressedJump = true;
}

void APlayerChar::StopJump()
{
    bPressedJump = false;
}


// Interaction & Building

void APlayerChar::FindObject()
{
    if (isBuilding)
    {
        PlaceBuilding();
        return;
    }

    if (isCrafting) return;

    // Harvesting / Interaction
    PlayAnimMontage(HarvestMontage);

    FHitResult HitResult;
    FVector Start = PlayerCamComp->GetComponentLocation();
    FVector End = Start + PlayerCamComp->GetForwardVector() * BuildTraceDistance;

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    QueryParams.bTraceComplex = true;

    if (GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, QueryParams))
    {
        // Try building part first
        if (ABuildingPart* HitPart = Cast<ABuildingPart>(HitResult.GetActor()))
        {
            RefundBuildingPart(HitPart);
            return;
        }

        // Resource harvesting
        if (AResource_M* HitResource = Cast<AResource_M>(HitResult.GetActor()))
        {
            HarvestResource(HitResource);
        }
    }
}

void APlayerChar::PlaceBuilding()
{
    if (spawnedPart && bCanPlace)
    {
        spawnedPart->SetActorEnableCollision(true);
        spawnedPart = nullptr;
        isBuilding = false;

        objectsBuilt += 1.0f;
        if (objWidget)
            objWidget->UpdatebuildObj(objectsBuilt);
    }
}

void APlayerChar::RefundBuildingPart(ABuildingPart* HitPart)
{
    if (!HitPart) return;

    int32 ID = HitPart->BuildingID;
    if (BuildingArray.IsValidIndex(ID))
    {
        BuildingArray[ID]++;
    }

    FString Name = GetBuildingName(ID);
    ShowResourcePopup(Name, 1.0f);

    HitPart->Destroy();
}

void APlayerChar::HarvestResource(AResource_M* HitResource)
{
    if (!HitResource || Stamina <= 5.0f) return;

    FString ResourceName = HitResource->resourceName;
    int32 Amount = FMath::RandRange(HitResource->MinResourceAmount, HitResource->MaxResourceAmount);

    HitResource->totalResource -= Amount;

    if (HitResource->totalResource > 0)
    {
        GiveResourceByName(Amount, ResourceName);
        matsCollected += Amount;
        if (objWidget) objWidget->UpdatematOBJ(matsCollected);

        UGameplayStatics::SpawnDecalAtLocation(GetWorld(), hitDecal, FVector(10.f), HitResult.Location, FRotator(-90, 0, 0), 2.0f);
        SetStamina(-5.0f);
    }
    else
    {
        HitResource->Destroy();
    }
}


// Resource Management

void APlayerChar::GiveResourceByName(float Amount, const FString& ResourceType)
{
    for (int32 i = 0; i < ResourcesNameArray.Num(); ++i)
    {
        if (ResourcesNameArray[i] == ResourceType)
        {
            if (ResourcesArray.IsValidIndex(i))
            {
                ResourcesArray[i] += Amount;
                ShowResourcePopup(ResourceType, Amount);
            }
            return;
        }
    }
}

void APlayerChar::UpdateResources(float WoodAmount, float StoneAmount, const FString& BuildingObject)
{
    if (WoodAmount > ResourcesArray[0] || StoneAmount > ResourcesArray[1])
        return;

    ResourcesArray[0] -= WoodAmount;
    ResourcesArray[1] -= StoneAmount;

    if (BuildingObject == "Wall") BuildingArray[0]++;
    else if (BuildingObject == "Floor") BuildingArray[1]++;
    else if (BuildingObject == "Ceiling") BuildingArray[2]++;
}


// Building System

void APlayerChar::SpawnBuilding(int32 BuildingID, bool& bSuccess)
{
    bSuccess = false;

    if (isBuilding || !BuildingArray.IsValidIndex(BuildingID) || BuildingArray[BuildingID] < 1)
        return;

    isBuilding = true;
    BuildingTypeBeingPlaced = BuildingID;
    BuildingArray[BuildingID]--;

    FVector SpawnLoc = PlayerCamComp->GetComponentLocation() + PlayerCamComp->GetForwardVector() * BuildTraceDistance;
    FRotator SpawnRot(0.0f, 0.0f, 0.0f);

    FActorSpawnParameters Params;
    spawnedPart = GetWorld()->SpawnActor<ABuildingPart>(BuildingPartClass, SpawnLoc, SpawnRot, Params);

    if (spawnedPart)
    {
        spawnedPart->BuildingID = BuildingID;
        spawnedPart->SetActorEnableCollision(false);
        bSuccess = true;
    }
    else
    {
        isBuilding = false;
        BuildingArray[BuildingID]++; // refund
    }
}

void APlayerChar::RotateBuilding()
{
    if (isBuilding && spawnedPart)
    {
        spawnedPart->AddActorLocalRotation(FRotator(0.0f, 90.0f, 0.0f));
    }
}

void APlayerChar::ToggleGridSnap()
{
    bUseGridSnap = !bUseGridSnap;
}

void APlayerChar::CancelBuilding()
{
    if (isBuilding && spawnedPart)
    {
        if (BuildingArray.IsValidIndex(BuildingTypeBeingPlaced))
        {
            BuildingArray[BuildingTypeBeingPlaced]++;
        }

        spawnedPart->Destroy();
        spawnedPart = nullptr;
        isBuilding = false;
    }
}

// Player Stats

void APlayerChar::SetHealth(float Amount)
{
    Health = FMath::Clamp(Health + Amount, 0.0f, 100.0f);
}

void APlayerChar::SetHunger(float Amount)
{
    Hunger = FMath::Clamp(Hunger + Amount, 0.0f, 100.0f);
}

void APlayerChar::SetStamina(float Amount)
{
    Stamina = FMath::Clamp(Stamina + Amount, 0.0f, 100.0f);
}

void APlayerChar::DecreaseStats()
{
    if (Hunger > 0.0f)
    {
        SetHunger(-4.0f);
        SetStamina(3.0f); // passive stamina regen
    }

    if (Hunger <= 0.0f && Health > 0.0f)
    {
        SetHealth(-10.0f);
    }
}

// Helper Functions

FString APlayerChar::GetBuildingName(int32 ID) const
{
    switch (ID)
    {
    case 0: return TEXT("Wall");
    case 1: return TEXT("Floor");
    case 2: return TEXT("Ceiling");
    default: return TEXT("Unknown");
    }
}

void APlayerChar::ShowResourcePopup(const FString& ResourceName, float Amount)
{
    
}
