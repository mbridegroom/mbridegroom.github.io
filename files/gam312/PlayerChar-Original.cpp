// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerChar.h"


// Sets default values
APlayerChar::APlayerChar()
{
//
// CAMERA USE 
// 
// Camera libraries UCameraComponent
// handle how the player views the world. They manage position, rotation,
// field of view, and attachment to player bones or components.

	PrimaryActorTick.bCanEverTick = true;

	PlayerCamComp = CreateDefaultSubobject<UCameraComponent>(TEXT("First Person Cam")); 
	PlayerCamComp->SetupAttachment(GetMesh(), "head"); 
	PlayerCamComp->bUsePawnControlRotation = true;

	BuildingArray.SetNum(3);
	ResourcesArray.SetNum(3);
	ResourcesNameArray.Add(TEXT("Wood"));
	ResourcesNameArray.Add(TEXT("Stone"));
	ResourcesNameArray.Add(TEXT("Berry"));

}

// Called when the game starts or when spawned
void APlayerChar::BeginPlay()
{
	Super::BeginPlay();

	FTimerHandle StatsTimerHandle;
	GetWorld()->GetTimerManager().SetTimer(StatsTimerHandle, this, &APlayerChar::DecreaseStats, 2.0f, true);

	if (objWidget)
	{
		// Initialize objective widget values
		objWidget->UpdatebuildObj(0.0f);
		objWidget->UpdatematOBJ(0.0f);
	}
	
}

// Called every frame
void APlayerChar::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	PlayerUI->UpdateBars(Health, Hunger, Stamina);

	// If player is building, update building part position
	PlayerUI->UpdateBars(Health, Hunger, Stamina);

	if (isBuilding && spawnedPart)
	{
		FHitResult HitResult;

		FVector StartLocation = PlayerCamComp->GetComponentLocation();
		FVector EndLocation = StartLocation + PlayerCamComp->GetForwardVector() * 400.0f;

		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(this);
		QueryParams.AddIgnoredActor(spawnedPart);

		FVector TargetLocation;

		// LINE TRACE
		if (GetWorld()->LineTraceSingleByChannel(HitResult, StartLocation, EndLocation, ECC_Visibility, QueryParams))
		{
			TargetLocation = HitResult.Location;
		}
		else
		{
			TargetLocation = EndLocation;
		}

		
		// GRID SNAP
		
		FVector SnappedLocation = TargetLocation;

		if (bUseGridSnap)
		{
			SnappedLocation.X = FMath::RoundToFloat(TargetLocation.X / GridSize) * GridSize;
			SnappedLocation.Y = FMath::RoundToFloat(TargetLocation.Y / GridSize) * GridSize;
			SnappedLocation.Z = HitResult.Location.Z;
		}
		

		spawnedPart->SetActorLocation(SnappedLocation);
		

		// Checking Placement
		
		FHitResult OverlapHit;

		FCollisionShape Box = FCollisionShape::MakeBox(FVector(50.f, 50.f, 50.f));

		bool bBlockingHit = GetWorld()->SweepSingleByChannel(
			OverlapHit,
			SnappedLocation,
			SnappedLocation,
			FQuat::Identity,
			ECC_WorldStatic,
			Box
		);

		bCanPlace = !bBlockingHit;

		
		// Future Improvement to change material color to indicate if placement is valid
		
		UStaticMeshComponent* SpawnedMesh = spawnedPart->FindComponentByClass<UStaticMeshComponent>();

		if (SpawnedMesh)
		{
			if (bCanPlace)
			{
				SpawnedMesh->SetVectorParameterValueOnMaterials(TEXT("Color"), FVector(0, 1, 0));
			}
			else
			{
				SpawnedMesh->SetVectorParameterValueOnMaterials(TEXT("Color"), FVector(1, 0, 0));
			}
		}
	}
}

// Called to bind functionality to input
void APlayerChar::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	// movement input
	PlayerInputComponent->BindAxis("MoveForward", this, &APlayerChar::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &APlayerChar::MoveRight);

	// camera input
	PlayerInputComponent->BindAxis("LookUp", this, &APlayerChar::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("Turn", this, &APlayerChar::AddControllerYawInput);

	// jump input
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &APlayerChar::StartJump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &APlayerChar::StopJump);

	PlayerInputComponent->BindAction("Interact", IE_Pressed, this, &APlayerChar::FindObject);
	// Setup Rotate Building input
	PlayerInputComponent->BindAction("RotPart", IE_Pressed, this, &APlayerChar::RotateBuilding);
	PlayerInputComponent->BindAction("ToggleSnap", IE_Pressed, this, &APlayerChar::ToggleGridSnap);

	PlayerInputComponent->BindAction("CancelBuild", IE_Pressed, this, &APlayerChar::CancelBuilding);
}


// LINEAR ALGEBRA for Movement + Rotation Math
//
// Games rely heavily on vectors and matrices to calculate movement,
// rotation, and physics. Unreal uses FVector and FRotator internally.

void APlayerChar::MoveForward(float axisValue)
{
	// get forward direction from camera rotation
	FRotator Rotation = GetActorRotation();

	FVector Direction = FRotationMatrix(Rotation).GetScaledAxis(EAxis::X);
	AddMovementInput(Direction, axisValue);
}

void APlayerChar::MoveRight(float axisValue)
{
	// get right direction from camera rotation
	FRotator Rotation = GetActorRotation();

	FVector Direction = FRotationMatrix(Rotation).GetScaledAxis(EAxis::Y);
	AddMovementInput(Direction, axisValue);
}

void APlayerChar::StartJump()
{
	// start jumping
	bPressedJump = true;
}

void APlayerChar::StopJump()
{
	// stop jumping
	bPressedJump = false;
}



// TRACE / COLLISION for Line Tracing + Hit Detection
// 
// Traces simulate invisible rays used for interaction, shooting,
// building placement, and environmental detection.

void APlayerChar::FindObject()
{
	FHitResult HitResult;

	// Get camera position
	FVector StartLocation = PlayerCamComp->GetComponentLocation();

	// Set trace distance
	FVector Direction = PlayerCamComp->GetForwardVector() * 400.0f;
	FVector EndLocation = StartLocation + Direction;

	// Setup collision settings
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	QueryParams.bTraceComplex = true;
	QueryParams.bReturnFaceIndex = true;

	if (!isBuilding && !isCrafting)
	{
		PlayAnimMontage(HarvestMontage);

		// Check for hit object
		if (GetWorld()->LineTraceSingleByChannel(HitResult, StartLocation, EndLocation, ECC_Visibility, QueryParams))
		{
			
			//  BUILDING PART LOGIC 
			
			if (ABuildingPart* HitPart = Cast<ABuildingPart>(HitResult.GetActor()))
			{
				int ID = HitPart->BuildingID;

				// Refund resource
				BuildingArray[ID]++;

				FString BuildingName = TEXT("Unknown");

				if (ID == 0) BuildingName = TEXT("Wall");
				if (ID == 1) BuildingName = TEXT("Floor");
				if (ID == 2) BuildingName = TEXT("Ceiling");

				ShowResourcePopup(BuildingName, 1.0f);

				ShowResourcePopup(BuildingName, 1.0f);
			
				// Destroy piece
				HitPart->Destroy();
				return; // stop here so we don't also treat it as a resource
			}

			
			//  Resource Logic 
			
			AResource_M* HitResource = Cast<AResource_M>(HitResult.GetActor());

			// Check stamina amount
			if (Stamina > 5.0f)
			{
				if (HitResource)
				{
					FString hitName = HitResource->resourceName;

					int resourceValue = FMath::RandRange(
						HitResource->MinResourceAmount,
						HitResource->MaxResourceAmount
					);

					// Remove collected amount
					HitResource->totalResource -= resourceValue;

					// Resource still available
					if (HitResource->totalResource > resourceValue)
					{
						GiveResources(resourceValue, hitName);

						// Update resource objective widget
						matsCollected = matsCollected + resourceValue;
						objWidget->UpdatematOBJ(matsCollected);

						UGameplayStatics::SpawnDecalAtLocation(
							GetWorld(),
							hitDecal,
							FVector(10.0f, 10.0f, 10.0f),
							HitResult.Location,
							FRotator(-90, 0, 0),
							2.0f);

						SetStamina(-5.0f);
					}
					else
					{
						HitResource->Destroy();
					}
				}
			}
		}
	}
	else
	{
		// Enabling Collision and stopping building if currently building
		isBuilding = false;

		if (spawnedPart)
		{
			spawnedPart->SetActorEnableCollision(true);
			spawnedPart = nullptr;
			// Update building objective widget
			objectsBuilt = objectsBuilt + 1.0f;
			objWidget->UpdatebuildObj(objectsBuilt);
		}
	}
}

void APlayerChar::SetHealth(float Amount)
{
	// Keep health below max
	if (Health + Amount <= 100)
	{
		Health = Health + Amount;
	}
	else
		{
		Health = 100.0f;
	}
}

void APlayerChar::SetHunger(float Amount)
{
	// Keep hunger below max
	if (Hunger + Amount <= 100)
	{
		Hunger = Hunger + Amount;
	}
	else
	{
		Hunger = 100.0f;
	}
}

void APlayerChar::SetStamina(float Amount)
{
	// Keep stamina below max
	if (Stamina + Amount <= 100)
	{
		Stamina = Stamina + Amount;
	}
	else
	{
		Stamina = 100.0f;
	}
}

void APlayerChar::DecreaseStats()
{
	// Lower hunger over time
	if (Hunger > 0.0f)
	{
		SetHunger(-4.0f);
		SetStamina(3.0f);
	}

	// Lose health if starving
	if (Health > 0.0f && Hunger <= 0.0f)
	{
		SetHealth(-10.0f);
	}
}

void APlayerChar::GiveResources(float Amount, FString resourceType)
{

	// Add wood amount
	if (resourceType == "Wood")
	{
		ResourcesArray[0] = ResourcesArray[0] + Amount;
	}

	// Add stone amount
	if (resourceType == "Stone")
	{
		ResourcesArray[1] = ResourcesArray[1] + Amount;
	}

	// Add berry amount
	if (resourceType == "Berry")
	{
		ResourcesArray[2] = ResourcesArray[2] + Amount;
	}

	ShowResourcePopup(resourceType, Amount);
}
// Updates resources when building
void APlayerChar::UpdateResources(float woodAmount, float stoneAmount, FString buildingObject)
{
	if (woodAmount <= ResourcesArray[0])
	{
		if (stoneAmount <= ResourcesArray[1])
		{    // Subtract used resources
			ResourcesArray[0] = ResourcesArray[0] - woodAmount;
			ResourcesArray[1] = ResourcesArray[1] - stoneAmount;

			if (buildingObject == "Wall")
			{
				BuildingArray[0] = BuildingArray[0] + 1;
			}

			if (buildingObject == "Floor")
			{
				BuildingArray[1] = BuildingArray[1] + 1;
			}

			if (buildingObject == "Ceiling")
			{
				BuildingArray[2] = BuildingArray[2] + 1;
			}

		}
	}
}

// Spawns building part
void APlayerChar::SpawnBuilding(int buildingID, bool& isSuccess)
{
	if (!isBuilding)
	{
		if (BuildingArray[buildingID] >= 1)
		{    // Spawn building part
			isBuilding = true;
			FActorSpawnParameters SpawnParams;
			FVector StartLocation = PlayerCamComp->GetComponentLocation();
			FVector Direction = PlayerCamComp->GetForwardVector() * 400.0f;
			FVector EndLocation = StartLocation + Direction;
			FRotator myRot(0, 0, 0);
			// Subtract from building array
			BuildingArray[buildingID] = BuildingArray[buildingID] - 1;
			BuildingTypeBeingPlaced = buildingID;

			spawnedPart = GetWorld()->SpawnActor<ABuildingPart>(BuildingPartClass, EndLocation, myRot, SpawnParams);
			
			if (spawnedPart)
			// Set building ID and disable collision for the spawned part
			{
				spawnedPart->BuildingID = buildingID;
				spawnedPart->SetActorEnableCollision(false);
				spawnedPart->SetActorTickEnabled(true);
			}

			isSuccess = true;

		
		}
		else
		{
			isSuccess = false;
		}
	}
}
// Rotates building part
void APlayerChar::RotateBuilding()
{
	if (isBuilding)
	{
		spawnedPart->AddActorLocalRotation(FRotator(0, 90, 0));

	}
}

void APlayerChar::ToggleGridSnap()
// Toggles grid snap on and off
{
    bUseGridSnap = !bUseGridSnap;
}

void APlayerChar::CancelBuilding()
// Cancels building and refunds resource
{
	if (isBuilding && spawnedPart)
	{
		BuildingArray[BuildingTypeBeingPlaced]++;

		spawnedPart->Destroy();

		spawnedPart = nullptr;
		isBuilding = false;
	}
}

// DONE IN BLUEPRINTS
// AI Nav Mesh and Pathfinding

// AI navigation in Unreal uses a NavMesh 
// which is a simplified walkable surface for AI agents.
// Pathfinding uses algorithms like A* to move AI efficiently.
