// Copyright 2016-2018 mik14a / Admix Network. All Rights Reserved.
// Edited by Muppetsg2 2025

#include "VoxelFactory.h"
#include <Editor.h>
#include <EditorFramework/AssetImportData.h>
//#include <Engine/SkeletalMesh.h>
#include <Engine/StaticMesh.h>
#include <Engine/Texture2D.h>
#include <HAL/FileManager.h>
#include <Materials/Material.h>
#include <Materials/MaterialExpressionTextureSample.h>
#include <Materials/MaterialExpressionVectorParameter.h>
#include <Materials/MaterialExpressionScalarParameter.h>
#include <Materials/MaterialExpressionMultiply.h>
#include <Materials/MaterialInstanceConstant.h>
#include <MaterialEditingLibrary.h>
#include <PhysicsEngine/BodySetup.h>
#include <PhysicsEngine/BoxElem.h>
#include <RawMesh.h>
#include "VOX.h"
#include "VoxAssetImportData.h"
#include "VoxImportOption.h"
#include "Voxel.h"

DEFINE_LOG_CATEGORY_STATIC(LogVoxelFactory, Log, All)

UVoxelFactory::UVoxelFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ImportOption(nullptr)
	, bShowOption(true)
{
	Formats.Add(TEXT("vox;MagicaVoxel"));

	bCreateNew = false;
	bText = false;
	bEditorImport = true;
}

void UVoxelFactory::PostInitProperties()
{
	Super::PostInitProperties();
	ImportOption = NewObject<UVoxImportOption>(this, NAME_None, RF_NoFlags);
}

bool UVoxelFactory::DoesSupportClass(UClass* Class)
{
	return Class == UStaticMesh::StaticClass()
		//|| Class == USkeletalMesh::StaticClass()
		|| Class == UVoxel::StaticClass();
}

UClass* UVoxelFactory::ResolveSupportedClass()
{
	UClass* Class = nullptr;
	if (ImportOption->VoxImportType == EVoxImportType::StaticMesh) {
		Class = UStaticMesh::StaticClass();
	/*} else if (ImportOption->VoxImportType == EVoxImportType::SkeletalMesh) {
		Class = USkeletalMesh::StaticClass();*/
	} else if (ImportOption->VoxImportType == EVoxImportType::Voxel) {
		Class = UVoxel::StaticClass();
	}
	return Class;
}

UObject* UVoxelFactory::FactoryCreateBinary(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, FFeedbackContext* Warn)
{
	UObject* Result = nullptr;
	UImportSubsystem* ImportSubsystem = GEditor->GetEditorSubsystem<UImportSubsystem>();

	ImportSubsystem->OnAssetPreImport.Broadcast(this, InClass, InParent, InName, Type);
	bool bImportAll = true;
	if (!bShowOption || ImportOption->GetImportOption(bImportAll)) {
		bShowOption = !bImportAll;
		FBufferReader Reader((void*)Buffer, BufferEnd - Buffer, false);
		FVox Vox(GetCurrentFilename(), Reader, ImportOption);
		switch (ImportOption->VoxImportType) {
		case EVoxImportType::StaticMesh:
			Result = CreateStaticMesh(InParent, InName, Flags, &Vox);
			break;
		/*case EVoxImportType::SkeletalMesh:
			Result = CreateSkeletalMesh(InParent, InName, Flags, &Vox);
			break;*/
		case EVoxImportType::Voxel:
			Result = CreateVoxel(InParent, InName, Flags, &Vox);
			break;
		default:
			break;
		}
	}
	ImportSubsystem->OnAssetPostImport.Broadcast(this, Result);

	return Result;
}

bool UVoxelFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	UStaticMesh* StaticMesh = Cast<UStaticMesh>(Obj);
	//USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Obj);
	UVoxel* Voxel = Cast<UVoxel>(Obj);

	const auto& AssetImportData = StaticMesh != nullptr ? StaticMesh->GetAssetImportData()
		//: SkeletalMesh != nullptr ? SkeletalMesh->GetAssetImportData()
		: Voxel != nullptr ? Voxel->GetAssetImportData()
		: nullptr;
	if (AssetImportData != nullptr) {
		const auto& SourcePath = AssetImportData->GetFirstFilename();
		FString Path, Filename, Extension;
		FPaths::Split(SourcePath, Path, Filename, Extension);
		if (Extension.Compare("vox", ESearchCase::IgnoreCase) == 0) {
			AssetImportData->ExtractFilenames(OutFilenames);
			return true;
		}
	}
	return false;
}

void UVoxelFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	UStaticMesh* StaticMesh = Cast<UStaticMesh>(Obj);
	//USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Obj);
	UVoxel* Voxel = Cast<UVoxel>(Obj);

	const auto& AssetImportData = StaticMesh ? StaticMesh->GetAssetImportData()
		//: SkeletalMesh ? SkeletalMesh->GetAssetImportData()
		: Voxel ? Voxel->GetAssetImportData()
		: nullptr;
	if (AssetImportData && ensure(NewReimportPaths.Num() == 1)) {
		AssetImportData->UpdateFilenameOnly(NewReimportPaths[0]);
	}
}

EReimportResult::Type UVoxelFactory::Reimport(UObject* Obj)
{
	UStaticMesh* StaticMesh = Cast<UStaticMesh>(Obj);
	//USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Obj);
	UVoxel* Voxel = Cast<UVoxel>(Obj);

	const auto& AssetImportData = StaticMesh ? Cast<UVoxAssetImportData>(StaticMesh->GetAssetImportData())
		//: SkeletalMesh ? Cast<UVoxAssetImportData>(SkeletalMesh->GetAssetImportData())
		: Voxel ? Cast<UVoxAssetImportData>(Voxel->GetAssetImportData())
		: nullptr;
	if (!AssetImportData) {
		return EReimportResult::Failed;
	}

	const auto& Filename = AssetImportData->GetFirstFilename();
	if (!Filename.Len() || IFileManager::Get().FileSize(*Filename) == INDEX_NONE) {
		return EReimportResult::Failed;
	}

	auto Result = EReimportResult::Failed;
	auto OutCanceled = false;
	AssetImportData->ToVoxImportOption(*ImportOption);
	bShowOption = false;
	if (ImportObject(Obj->GetClass(), Obj->GetOuter(), *Obj->GetName(), RF_Public | RF_Standalone, Filename, nullptr, OutCanceled) != nullptr) {
		UE_LOG(LogVoxelFactory, Verbose, TEXT("Reimport successfully."));
		AssetImportData->Update(Filename);
		if (Obj->GetOuter()) {
			Obj->GetOuter()->MarkPackageDirty();
		} else {
			Obj->MarkPackageDirty();
		}
		Result = EReimportResult::Succeeded;
	} else {
		if (OutCanceled) {
			UE_LOG(LogVoxelFactory, Warning, TEXT("Reimport canceled."));
		} else {
			UE_LOG(LogVoxelFactory, Warning, TEXT("Reimport failed."));
		}
		Result = EReimportResult::Failed;
	}
	return Result;
}

UStaticMesh* UVoxelFactory::CreateStaticMesh(UObject* InParent, FName InName, EObjectFlags Flags, const FVox* Vox) const
{
	UStaticMesh* StaticMesh = NewObject<UStaticMesh>(InParent, InName, Flags | RF_Public);
	if (!StaticMesh->GetAssetImportData() || !StaticMesh->GetAssetImportData()->IsA<UVoxAssetImportData>()) {
		auto AssetImportData = NewObject<UVoxAssetImportData>(StaticMesh);
		AssetImportData->FromVoxImportOption(*ImportOption);
		StaticMesh->SetAssetImportData(AssetImportData);
	}

	FRawMesh RawMesh;
	if (!Vox->CreateOptimizedRawMesh(RawMesh, ImportOption))
	{
		UE_LOG(LogVoxelFactory, Warning, TEXT("Failed to create optimized raw mesh"));
	}

	if (ImportOption->bImportMaterial) {
		if (ImportOption->bOneMaterial) {
			UMaterialInterface* Material = CreateMaterial(InParent, InName, Flags, Vox);
			StaticMesh->GetStaticMaterials().Add(FStaticMaterial(Material));
		}
		else {
			TArray<uint8> Palette;
			for (const auto& cell : Vox->Voxel) {
				Palette.AddUnique(cell.Value);
			}

			UMaterial* Material = NewObject<UMaterial>(InParent, *FString::Printf(TEXT("%s_MT"), *InName.GetPlainNameString()), Flags | RF_Public);
			Material->TwoSided = false;
			Material->SetShadingModel(MSM_DefaultLit);
			auto EditorOnly = Material->GetEditorOnlyData();

			// Color Expression
			UMaterialExpressionVectorParameter* ColorExpression = NewObject<UMaterialExpressionVectorParameter>(Material);
			ColorExpression->ParameterName = TEXT("Color");
			ColorExpression->DefaultValue = FLinearColor::Gray;
			ColorExpression->MaterialExpressionEditorX = -250;
			ColorExpression->MaterialExpressionEditorY = 0;
			EditorOnly->ExpressionCollection.AddExpression(ColorExpression);
			EditorOnly->BaseColor.Expression = ColorExpression;

			// Roughness Expression
			UMaterialExpressionScalarParameter* RoughnessExpression = NewObject<UMaterialExpressionScalarParameter>(Material);
			RoughnessExpression->ParameterName = TEXT("Roughness");
			RoughnessExpression->DefaultValue = 0.5f;
			RoughnessExpression->MaterialExpressionEditorX = -250;
			RoughnessExpression->MaterialExpressionEditorY = 220;
			EditorOnly->ExpressionCollection.AddExpression(RoughnessExpression);
			EditorOnly->Roughness.Expression = RoughnessExpression;

			// Metallic Expression - [for Metal Material]
			UMaterialExpressionScalarParameter* MetallicExpression = NewObject<UMaterialExpressionScalarParameter>(Material);
			MetallicExpression->ParameterName = TEXT("Metallic");
			MetallicExpression->DefaultValue = 0.0f;
			MetallicExpression->MaterialExpressionEditorX = -250;
			MetallicExpression->MaterialExpressionEditorY = 325;
			EditorOnly->ExpressionCollection.AddExpression(MetallicExpression);
			EditorOnly->Metallic.Expression = MetallicExpression;

			// Opacity Expression - [for Glass Material]
			UMaterialExpressionScalarParameter* OpacityExpression = NewObject<UMaterialExpressionScalarParameter>(Material);
			OpacityExpression->ParameterName = TEXT("Opacity");
			OpacityExpression->DefaultValue = 1.0f;
			OpacityExpression->MaterialExpressionEditorX = -625;
			OpacityExpression->MaterialExpressionEditorY = 220;
			EditorOnly->ExpressionCollection.AddExpression(OpacityExpression);
			EditorOnly->Opacity.Expression = OpacityExpression;

			// Emission Color Expression - [for Emissive Material]
			UMaterialExpressionVectorParameter* EmissionColorExpression = NewObject<UMaterialExpressionVectorParameter>(Material);
			EmissionColorExpression->ParameterName = TEXT("EmissionColor");
			EmissionColorExpression->DefaultValue = FLinearColor::White;
			EmissionColorExpression->MaterialExpressionEditorX = -625;
			EmissionColorExpression->MaterialExpressionEditorY = 325;
			EditorOnly->ExpressionCollection.AddExpression(EmissionColorExpression);

			// Emission Expression - [for Emissive Material]
			UMaterialExpressionScalarParameter* EmissionExpression = NewObject<UMaterialExpressionScalarParameter>(Material);
			EmissionExpression ->ParameterName = TEXT("Emission");
			EmissionExpression ->DefaultValue = 0.0f;
			EmissionExpression ->MaterialExpressionEditorX = -625;
			EmissionExpression ->MaterialExpressionEditorY = 535;
			EditorOnly->ExpressionCollection.AddExpression(EmissionExpression);

			// Emission Power Expression - [for Emissive Material]
			UMaterialExpressionScalarParameter* EmissionPowerExpression = NewObject<UMaterialExpressionScalarParameter>(Material);
			EmissionPowerExpression ->ParameterName = TEXT("EmissionPower");
			EmissionPowerExpression ->DefaultValue = 0.0f;
			EmissionPowerExpression ->MaterialExpressionEditorX = -625;
			EmissionPowerExpression ->MaterialExpressionEditorY = 640;
			EditorOnly->ExpressionCollection.AddExpression(EmissionPowerExpression);

			// Multiply Node Expression 1 - [for Emissive Material]
			UMaterialExpressionMultiply* MultiplyEmissionExpression1 = NewObject<UMaterialExpressionMultiply>(Material);
			MultiplyEmissionExpression1->MaterialExpressionEditorX = -375;
			MultiplyEmissionExpression1->MaterialExpressionEditorY = 520;
			MultiplyEmissionExpression1->A.Connect(0, EmissionExpression);
			MultiplyEmissionExpression1->B.Connect(0, EmissionPowerExpression);
			EditorOnly->ExpressionCollection.AddExpression(MultiplyEmissionExpression1);

			// Multiply Node Expression 2 - [for Emissive Material]
			UMaterialExpressionMultiply* MultiplyEmissionExpression2 = NewObject<UMaterialExpressionMultiply>(Material);
			MultiplyEmissionExpression2->MaterialExpressionEditorX = -250;
			MultiplyEmissionExpression2->MaterialExpressionEditorY = 430;
			MultiplyEmissionExpression2->A.Connect(0, EmissionColorExpression);
			MultiplyEmissionExpression2->B.Connect(0, MultiplyEmissionExpression1);
			EditorOnly->ExpressionCollection.AddExpression(MultiplyEmissionExpression2);

			EditorOnly->EmissiveColor.Expression = MultiplyEmissionExpression2;

			Material->PostEditChange();
			
			for (uint8 color : Palette) {
				UMaterialInstanceConstant* MaterialInstance = NewObject<UMaterialInstanceConstant>(InParent, *FString::Printf(TEXT("%s_MI%d"), *InName.GetPlainNameString(), color), Flags | RF_Public);
				const auto& VoxMaterialData = Vox->Materials[color];
				MaterialInstance->SetParentEditorOnly(Material);
				FLinearColor LinearColor = FLinearColor::FromSRGBColor(Vox->Palette[color]);
				MaterialInstance->SetVectorParameterValueEditorOnly(TEXT("Color"), LinearColor);
				MaterialInstance->SetScalarParameterValueEditorOnly(TEXT("Roughness"), VoxMaterialData.Roughness);
				if (VoxMaterialData.Type == EVoxMaterialType::GLASS) {
					UE_LOG(LogVoxelFactory, Verbose, TEXT("Create Glass Material Instance: %s"), *MaterialInstance->GetName());
					MaterialInstance->BasePropertyOverrides.bOverride_BlendMode = true;
					MaterialInstance->BasePropertyOverrides.BlendMode = EBlendMode::BLEND_Translucent;
					MaterialInstance->SetScalarParameterValueEditorOnly(TEXT("Opacity"), 1.0f - VoxMaterialData.Transparency);

					MaterialInstance->UpdateOverridableBaseProperties();
				}
				else if (VoxMaterialData.Type == EVoxMaterialType::METAL) {
					UE_LOG(LogVoxelFactory, Verbose, TEXT("Create Metal Material Instance: %s"), *MaterialInstance->GetName());
					MaterialInstance->SetScalarParameterValueEditorOnly(TEXT("Metallic"), VoxMaterialData.Metallic);
				}
				else if (VoxMaterialData.Type == EVoxMaterialType::EMIT) {
					UE_LOG(LogVoxelFactory, Verbose, TEXT("Create Emissive Material Instance: %s"), *MaterialInstance->GetName());
					MaterialInstance->SetVectorParameterValueEditorOnly(TEXT("EmissionColor"), LinearColor);
					MaterialInstance->SetScalarParameterValueEditorOnly(TEXT("EmissionPower"), VoxMaterialData.EmissionPower);
					MaterialInstance->SetScalarParameterValueEditorOnly(TEXT("Emission"), VoxMaterialData.Emissive);
				}
				
				UMaterialEditingLibrary::UpdateMaterialInstance(MaterialInstance);
				StaticMesh->GetStaticMaterials().Add(FStaticMaterial(MaterialInstance));
			}
		}
	}

	BuildStaticMesh(StaticMesh, RawMesh);
	StaticMesh->GetAssetImportData()->Update(Vox->Filename);
	return StaticMesh;
}

/*
USkeletalMesh* UVoxelFactory::CreateSkeletalMesh(UObject* InParent, FName InName, EObjectFlags Flags, const FVox* Vox) const
{
	USkeletalMesh* SkeletalMesh = NewObject<USkeletalMesh>(InParent, InName, Flags | RF_Public);
	if (!SkeletalMesh->GetAssetImportData() || !SkeletalMesh->GetAssetImportData()->IsA<UVoxAssetImportData>()) {
		auto AssetImportData = NewObject<UVoxAssetImportData>(SkeletalMesh);
		AssetImportData->FromVoxImportOption(*ImportOption);
		SkeletalMesh->SetAssetImportData(AssetImportData);
	}
	
	SkeletalMesh->GetAssetImportData()->Update(Vox->Filename);
	return SkeletalMesh;
}
*/

UVoxel* UVoxelFactory::CreateVoxel(UObject* InParent, FName InName, EObjectFlags Flags, const FVox* Vox) const
{
	UVoxel* Voxel = NewObject<UVoxel>(InParent, InName, Flags | RF_Public);
	if (!Voxel->GetAssetImportData() || !Voxel->GetAssetImportData()->IsA<UVoxAssetImportData>()) {
		auto AssetImportData = NewObject<UVoxAssetImportData>(Voxel);
		AssetImportData->FromVoxImportOption(*ImportOption);
		Voxel->SetAssetImportData(AssetImportData);
	}
	Voxel->Size = Vox->Size;
	TArray<uint8> Palette;
	for (const auto& cell : Vox->Voxel) {
		Palette.AddUnique(cell.Value);
	}

	UMaterial* Material = NewObject<UMaterial>(InParent, *FString::Printf(TEXT("%s_MT"), *InName.GetPlainNameString()), Flags | RF_Public);
	Material->TwoSided = false;
	Material->SetShadingModel(MSM_DefaultLit);
	auto EditorOnly = Material->GetEditorOnlyData();

	// Color Expression
	UMaterialExpressionVectorParameter* ColorExpression = NewObject<UMaterialExpressionVectorParameter>(Material);
	ColorExpression->ParameterName = TEXT("Color");
	ColorExpression->DefaultValue = FLinearColor::Gray;
	ColorExpression->MaterialExpressionEditorX = -250;
	ColorExpression->MaterialExpressionEditorY = 0;
	EditorOnly->ExpressionCollection.AddExpression(ColorExpression);
	EditorOnly->BaseColor.Expression = ColorExpression;

	// Roughness Expression
	UMaterialExpressionScalarParameter* RoughnessExpression = NewObject<UMaterialExpressionScalarParameter>(Material);
	RoughnessExpression->ParameterName = TEXT("Roughness");
	RoughnessExpression->DefaultValue = 0.5f;
	RoughnessExpression->MaterialExpressionEditorX = -250;
	RoughnessExpression->MaterialExpressionEditorY = 220;
	EditorOnly->ExpressionCollection.AddExpression(RoughnessExpression);
	EditorOnly->Roughness.Expression = RoughnessExpression;

	// Metallic Expression - [for Metal Material]
	UMaterialExpressionScalarParameter* MetallicExpression = NewObject<UMaterialExpressionScalarParameter>(Material);
	MetallicExpression->ParameterName = TEXT("Metallic");
	MetallicExpression->DefaultValue = 0.0f;
	MetallicExpression->MaterialExpressionEditorX = -250;
	MetallicExpression->MaterialExpressionEditorY = 325;
	EditorOnly->ExpressionCollection.AddExpression(MetallicExpression);
	EditorOnly->Metallic.Expression = MetallicExpression;

	// Opacity Expression - [for Glass Material]
	UMaterialExpressionScalarParameter* OpacityExpression = NewObject<UMaterialExpressionScalarParameter>(Material);
	OpacityExpression->ParameterName = TEXT("Opacity");
	OpacityExpression->DefaultValue = 1.0f;
	OpacityExpression->MaterialExpressionEditorX = -625;
	OpacityExpression->MaterialExpressionEditorY = 220;
	EditorOnly->ExpressionCollection.AddExpression(OpacityExpression);
	EditorOnly->Opacity.Expression = OpacityExpression;

	// Emission Color Expression - [for Emissive Material]
	UMaterialExpressionVectorParameter* EmissionColorExpression = NewObject<UMaterialExpressionVectorParameter>(Material);
	EmissionColorExpression->ParameterName = TEXT("EmissionColor");
	EmissionColorExpression->DefaultValue = FLinearColor::White;
	EmissionColorExpression->MaterialExpressionEditorX = -625;
	EmissionColorExpression->MaterialExpressionEditorY = 325;
	EditorOnly->ExpressionCollection.AddExpression(EmissionColorExpression);

	// Emission Expression - [for Emissive Material]
	UMaterialExpressionScalarParameter* EmissionExpression = NewObject<UMaterialExpressionScalarParameter>(Material);
	EmissionExpression ->ParameterName = TEXT("Emission");
	EmissionExpression ->DefaultValue = 0.0f;
	EmissionExpression ->MaterialExpressionEditorX = -625;
	EmissionExpression ->MaterialExpressionEditorY = 535;
	EditorOnly->ExpressionCollection.AddExpression(EmissionExpression);

	// Emission Power Expression - [for Emissive Material]
	UMaterialExpressionScalarParameter* EmissionPowerExpression = NewObject<UMaterialExpressionScalarParameter>(Material);
	EmissionPowerExpression ->ParameterName = TEXT("EmissionPower");
	EmissionPowerExpression ->DefaultValue = 0.0f;
	EmissionPowerExpression ->MaterialExpressionEditorX = -625;
	EmissionPowerExpression ->MaterialExpressionEditorY = 640;
	EditorOnly->ExpressionCollection.AddExpression(EmissionPowerExpression);

	// Multiply Node Expression 1 - [for Emissive Material]
	UMaterialExpressionMultiply* MultiplyEmissionExpression1 = NewObject<UMaterialExpressionMultiply>(Material);
	MultiplyEmissionExpression1->MaterialExpressionEditorX = -375;
	MultiplyEmissionExpression1->MaterialExpressionEditorY = 520;
	MultiplyEmissionExpression1->A.Connect(0, EmissionExpression);
	MultiplyEmissionExpression1->B.Connect(0, EmissionPowerExpression);
	EditorOnly->ExpressionCollection.AddExpression(MultiplyEmissionExpression1);

	// Multiply Node Expression 2 - [for Emissive Material]
	UMaterialExpressionMultiply* MultiplyEmissionExpression2 = NewObject<UMaterialExpressionMultiply>(Material);
	MultiplyEmissionExpression2->MaterialExpressionEditorX = -250;
	MultiplyEmissionExpression2->MaterialExpressionEditorY = 430;
	MultiplyEmissionExpression2->A.Connect(0, EmissionColorExpression);
	MultiplyEmissionExpression2->B.Connect(0, MultiplyEmissionExpression1);
	EditorOnly->ExpressionCollection.AddExpression(MultiplyEmissionExpression2);

	EditorOnly->EmissiveColor.Expression = MultiplyEmissionExpression2;

	Material->PostEditChange();

	for (uint8 color : Palette) {
		UMaterialInstanceConstant* MaterialInstance = NewObject<UMaterialInstanceConstant>(InParent, *FString::Printf(TEXT("%s_MI%d"), *InName.GetPlainNameString(), color), Flags | RF_Public);
		const auto& VoxMaterialData = Vox->Materials[color];
		MaterialInstance->SetParentEditorOnly(Material);
		FLinearColor LinearColor = FLinearColor::FromSRGBColor(Vox->Palette[color]);
		MaterialInstance->SetVectorParameterValueEditorOnly(TEXT("Color"), LinearColor);
		MaterialInstance->SetScalarParameterValueEditorOnly(TEXT("Roughness"), VoxMaterialData.Roughness);
		if (VoxMaterialData.Type == EVoxMaterialType::GLASS) {
			MaterialInstance->BasePropertyOverrides.bOverride_BlendMode = true;
			MaterialInstance->BasePropertyOverrides.BlendMode = EBlendMode::BLEND_Translucent;
			MaterialInstance->SetScalarParameterValueEditorOnly(TEXT("Opacity"), 1.0f - VoxMaterialData.Transparency);

			MaterialInstance->UpdateOverridableBaseProperties();
		}
		else if (VoxMaterialData.Type == EVoxMaterialType::METAL) {
			MaterialInstance->SetScalarParameterValueEditorOnly(TEXT("Metallic"), VoxMaterialData.Metallic);
		}
		else if (VoxMaterialData.Type == EVoxMaterialType::EMIT) {
			MaterialInstance->SetVectorParameterValueEditorOnly(TEXT("EmissionColor"), LinearColor);
			MaterialInstance->SetScalarParameterValueEditorOnly(TEXT("EmissionPower"), VoxMaterialData.EmissionPower);
		}

		UMaterialEditingLibrary::UpdateMaterialInstance(MaterialInstance);

		FRawMesh RawMesh;
		FVox::CreateMesh(RawMesh, ImportOption);
		UStaticMesh* StaticMesh = NewObject<UStaticMesh>(InParent, *FString::Printf(TEXT("%s_SM%d"), *InName.GetPlainNameString(), color), Flags | RF_Public);
		StaticMesh->GetStaticMaterials().Add(FStaticMaterial(MaterialInstance));
		BuildStaticMesh(StaticMesh, RawMesh);
		
		const FVector& Scale = ImportOption->GetBuildSettings().BuildScale3D;
		FKBoxElem BoxElem(Scale.X, Scale.Y, Scale.Z);
		StaticMesh->GetBodySetup()->AggGeom.BoxElems.Add(BoxElem);
		
		Voxel->Mesh.Add(StaticMesh);
	}
	for (const auto& cell : Vox->Voxel) {
		Voxel->Voxel.Add(cell.Key, Palette.IndexOfByKey(cell.Value));
		check(INDEX_NONE != Palette.IndexOfByKey(cell.Value));
	}
	Voxel->bXYCenter = ImportOption->bImportXYCenter;
	Voxel->CalcCellBounds();
	Voxel->GetAssetImportData()->Update(Vox->Filename);
	return Voxel;
}

UStaticMesh* UVoxelFactory::BuildStaticMesh(UStaticMesh* OutStaticMesh, FRawMesh& RawMesh) const
{
	check(OutStaticMesh);
	FStaticMeshSourceModel& StaticMeshSourceModel = OutStaticMesh->AddSourceModel();
	StaticMeshSourceModel.BuildSettings = ImportOption->GetBuildSettings();
	StaticMeshSourceModel.RawMeshBulkData->SaveRawMesh(RawMesh);
	TArray<FText> Errors;
	OutStaticMesh->Build(false, &Errors);
	return OutStaticMesh;
}

UMaterialInterface* UVoxelFactory::CreateMaterial(UObject* InParent, FName& InName, EObjectFlags Flags, const FVox* Vox) const
{
	UMaterial* Material = NewObject<UMaterial>(InParent, *FString::Printf(TEXT("%s_MT"), *InName.GetPlainNameString()), Flags | RF_Public);
	UTexture2D* Texture = NewObject<UTexture2D>(InParent, *FString::Printf(TEXT("%s_TX"), *InName.GetPlainNameString()), Flags | RF_Public);
	if (Vox->CreateTexture(Texture, ImportOption)) {
		Material->TwoSided = false;
		Material->SetShadingModel(MSM_DefaultLit);
		UMaterialExpressionTextureSample* Expression = NewObject<UMaterialExpressionTextureSample>(Material);
		auto EditorOnly = Material->GetEditorOnlyData();
		EditorOnly->ExpressionCollection.AddExpression(Expression);
		EditorOnly->BaseColor.Expression = Expression;
		Expression->Texture = Texture;
		Material->PostEditChange();
	}
	return Material;
}