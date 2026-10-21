// Copyright (c) 2025-2026 Muppetsg2
// Licensed under the MIT License.

#include "VoxMeshBuilder.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "StaticMeshAttributes.h"

DEFINE_LOG_CATEGORY_STATIC(LogVoxMeshBuilder, Log, All)

namespace VoxMeshBuilderPrivate
{
	/** Welding key: position in half voxels. Mesh corners are whole voxels, pivots may be half voxels. */
	FIntVector WeldKey(const FVector3f& Position, float Scale)
	{
		const float InvHalfVoxel = 2.0f / Scale;
		return FIntVector(
			FMath::RoundToInt(Position.X * InvHalfVoxel),
			FMath::RoundToInt(Position.Y * InvHalfVoxel),
			FMath::RoundToInt(Position.Z * InvHalfVoxel));
	}

	/** Planar texture coordinates in voxel units, projected along the dominant axis of the normal. */
	FVector2f PlanarUV(const FVector3f& Position, const FVector3f& Normal, float Scale)
	{
		const FVector3f P = Position / Scale;
		const FVector3f A(FMath::Abs(Normal.X), FMath::Abs(Normal.Y), FMath::Abs(Normal.Z));
		if (A.X >= A.Y && A.X >= A.Z)
		{
			return FVector2f(P.Y, -P.Z);
		}
		if (A.Y >= A.Z)
		{
			return FVector2f(P.X, -P.Z);
		}
		return FVector2f(P.X, P.Y);
	}

	FVector4f ToVertexColor(const FColor& Color)
	{
		const FLinearColor Linear(Color); // sRGB -> linear
		return FVector4f(Linear.R, Linear.G, Linear.B, Linear.A);
	}

	/** Every edge of a voxel mesh is a crease. */
	void HardenEdges(FMeshDescription& Description)
	{
		FStaticMeshAttributes Attributes(Description);
		TEdgeAttributesRef<bool> EdgeHardnesses = Attributes.GetEdgeHardnesses();
		for (const FEdgeID EdgeID : Description.Edges().GetElementIDs())
		{
			EdgeHardnesses[EdgeID] = true;
		}
	}
}

FBox3f VoxMeshBuilder::GetBounds(const v4c_mesh& Mesh)
{
	FBox3f Bounds(ForceInit);
	if (Mesh.positions)
	{
		for (uint32 VertexIndex = 0; VertexIndex < Mesh.num_vertices; ++VertexIndex)
		{
			const float* P = Mesh.positions + VertexIndex * 3;
			Bounds += FVector3f(P[0], P[1], P[2]);
		}
	}
	return Bounds;
}

FVector3f VoxMeshBuilder::GetPivotOffset(const FBox3f& Bounds, EVoxPivot Pivot)
{
	if (!Bounds.IsValid)
	{
		return FVector3f::ZeroVector;
	}
	switch (Pivot)
	{
	case EVoxPivot::Corner:       return Bounds.Min;
	case EVoxPivot::Center:       return Bounds.GetCenter();
	case EVoxPivot::BottomCenter: return FVector3f(Bounds.GetCenter().X, Bounds.GetCenter().Y, Bounds.Min.Z);
	case EVoxPivot::MagicaVoxel:
	default:                      return FVector3f::ZeroVector; // origin of the MagicaVoxel scene
	}
}

bool VoxMeshBuilder::BuildMesh(const FVoxScene& Scene, const v4c_mesh& Mesh, const FVector3f& Offset, FVoxMeshData& Out)
{
	Out = FVoxMeshData();
	if (!Scene.IsValid() || !Mesh.positions || !Mesh.normals || !Mesh.texcoords || !Mesh.materials || Mesh.num_indices == 0)
	{
		return false;
	}

	const float Scale = Scene->scale;

	// One polygon group (= material slot) per used material, ordered by material index.
	TArray<uint32> UsedMaterials;
	for (uint32 SubmeshIndex = 0; SubmeshIndex < Mesh.num_submeshes; ++SubmeshIndex)
	{
		if (Mesh.submeshes[SubmeshIndex].num_indices > 0)
		{
			UsedMaterials.AddUnique(Mesh.submeshes[SubmeshIndex].material);
		}
	}
	if (UsedMaterials.Num() == 0)
	{
		return false;
	}
	UsedMaterials.Sort();

	FMeshDescription& Description = Out.Description;
	FStaticMeshAttributes Attributes(Description);
	Attributes.Register();

	TVertexAttributesRef<FVector3f> VertexPositions = Attributes.GetVertexPositions();
	TVertexInstanceAttributesRef<FVector3f> VertexInstanceNormals = Attributes.GetVertexInstanceNormals();
	TVertexInstanceAttributesRef<FVector2f> VertexInstanceUVs = Attributes.GetVertexInstanceUVs();
	TVertexInstanceAttributesRef<FVector4f> VertexInstanceColors = Attributes.GetVertexInstanceColors();
	TPolygonGroupAttributesRef<FName> PolygonGroupSlotNames = Attributes.GetPolygonGroupMaterialSlotNames();
	VertexInstanceUVs.SetNumChannels(2);

	const int32 NumVertices = static_cast<int32>(Mesh.num_vertices);
	const int32 NumTriangles = static_cast<int32>(Mesh.num_indices / 3);
	Description.ReserveNewVertices(NumVertices);
	Description.ReserveNewVertexInstances(NumVertices);
	Description.ReserveNewTriangles(NumTriangles);
	Description.ReserveNewPolygons(NumTriangles);
	Description.ReserveNewEdges(NumTriangles * 2);

	TMap<uint32, FPolygonGroupID> PolygonGroups;
	for (const uint32 Material : UsedMaterials)
	{
		const FPolygonGroupID PolygonGroupID = Description.CreatePolygonGroup();
		PolygonGroupSlotNames[PolygonGroupID] = Scene.GetMaterialSlotName(Material);
		PolygonGroups.Add(Material, PolygonGroupID);
		Out.Materials.Add(Material);
	}

	TMap<FIntVector, FVertexID> WeldedVertices;
	WeldedVertices.Reserve(NumVertices / 2);

	TArray<FVertexInstanceID> Instances;
	Instances.SetNumUninitialized(NumVertices);
	for (int32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
	{
		const float* P = Mesh.positions + VertexIndex * 3;
		const float* N = Mesh.normals + VertexIndex * 3;
		const FVector3f Position = FVector3f(P[0], P[1], P[2]) - Offset;
		const FVector3f Normal(N[0], N[1], N[2]);
		const uint32 Material = Mesh.materials[VertexIndex];

		const FIntVector Key = VoxMeshBuilderPrivate::WeldKey(Position, Scale);
		FVertexID VertexID;
		if (const FVertexID* Found = WeldedVertices.Find(Key))
		{
			VertexID = *Found;
		}
		else
		{
			VertexID = Description.CreateVertex();
			VertexPositions[VertexID] = Position;
			WeldedVertices.Add(Key, VertexID);
		}

		const FVertexInstanceID InstanceID = Description.CreateVertexInstance(VertexID);
		VertexInstanceNormals[InstanceID] = Normal;
		VertexInstanceUVs.Set(InstanceID, 0, FVector2f(Mesh.texcoords[VertexIndex * 2], Mesh.texcoords[VertexIndex * 2 + 1]));
		VertexInstanceUVs.Set(InstanceID, 1, VoxMeshBuilderPrivate::PlanarUV(Position, Normal, Scale));
		VertexInstanceColors[InstanceID] = VoxMeshBuilderPrivate::ToVertexColor(Scene.GetColor(Material));
		Instances[VertexIndex] = InstanceID;
	}

	for (uint32 SubmeshIndex = 0; SubmeshIndex < Mesh.num_submeshes; ++SubmeshIndex)
	{
		const v4c_submesh& Submesh = Mesh.submeshes[SubmeshIndex];
		const FPolygonGroupID* PolygonGroupID = PolygonGroups.Find(Submesh.material);
		if (!PolygonGroupID)
		{
			continue;
		}

		const uint32 End = Submesh.first_index + Submesh.num_indices;
		for (uint32 Index = Submesh.first_index; Index + 2 < End; Index += 3)
		{
			// Works for 32 and 16-bit index buffers.
			FVertexInstanceID Triangle[3] = {
				Instances[v4c_mesh_index(&Mesh, Index)],
				Instances[v4c_mesh_index(&Mesh, Index + 1)],
				Instances[v4c_mesh_index(&Mesh, Index + 2)]
			};

			const FVertexID V0 = Description.GetVertexInstanceVertex(Triangle[0]);
			const FVertexID V1 = Description.GetVertexInstanceVertex(Triangle[1]);
			const FVertexID V2 = Description.GetVertexInstanceVertex(Triangle[2]);
			if (V0 == V1 || V1 == V2 || V0 == V2)
			{
				continue; // degenerate after welding
			}

			Description.CreateTriangle(*PolygonGroupID, MakeArrayView(Triangle, 3));
			++Out.NumTriangles;
		}
	}

	VoxMeshBuilderPrivate::HardenEdges(Description);

	UE_LOG(LogVoxMeshBuilder, Verbose, TEXT("Built mesh: %d triangles, %d vertices, %d materials"),
		Out.NumTriangles, Description.Vertices().Num(), Out.Materials.Num());
	return Out.NumTriangles > 0;
}

void VoxMeshBuilder::BuildCube(const FVoxScene& Scene, uint32 Material, float Size, FVoxMeshData& Out)
{
	Out = FVoxMeshData();

	FMeshDescription& Description = Out.Description;
	FStaticMeshAttributes Attributes(Description);
	Attributes.Register();

	TVertexAttributesRef<FVector3f> VertexPositions = Attributes.GetVertexPositions();
	TVertexInstanceAttributesRef<FVector3f> VertexInstanceNormals = Attributes.GetVertexInstanceNormals();
	TVertexInstanceAttributesRef<FVector2f> VertexInstanceUVs = Attributes.GetVertexInstanceUVs();
	TVertexInstanceAttributesRef<FVector4f> VertexInstanceColors = Attributes.GetVertexInstanceColors();
	TPolygonGroupAttributesRef<FName> PolygonGroupSlotNames = Attributes.GetPolygonGroupMaterialSlotNames();
	VertexInstanceUVs.SetNumChannels(2);

	const FPolygonGroupID PolygonGroupID = Description.CreatePolygonGroup();
	PolygonGroupSlotNames[PolygonGroupID] = Scene.GetMaterialSlotName(Material);
	Out.Materials.Add(Material);

	// Face normal N with two in-plane axes U, V such that U x V = N.
	struct FCubeFace { FVector3f N, U, V; };
	static const FCubeFace Faces[6] = {
		{ FVector3f( 1, 0, 0), FVector3f(0, 1, 0), FVector3f(0, 0, 1) },
		{ FVector3f(-1, 0, 0), FVector3f(0, 0, 1), FVector3f(0, 1, 0) },
		{ FVector3f( 0, 1, 0), FVector3f(0, 0, 1), FVector3f(1, 0, 0) },
		{ FVector3f( 0,-1, 0), FVector3f(1, 0, 0), FVector3f(0, 0, 1) },
		{ FVector3f( 0, 0, 1), FVector3f(1, 0, 0), FVector3f(0, 1, 0) },
		{ FVector3f( 0, 0,-1), FVector3f(0, 1, 0), FVector3f(1, 0, 0) },
	};

	const FVector2f PaletteUV = Scene.GetPaletteUV(Material);
	const FVector4f Color = VoxMeshBuilderPrivate::ToVertexColor(Scene.GetColor(Material));
	TMap<FIntVector, FVertexID> Corners;

	for (const FCubeFace& Face : Faces)
	{
		FVertexInstanceID Instances[2][2];
		for (int32 S = 0; S < 2; ++S)
		{
			for (int32 T = 0; T < 2; ++T)
			{
				const FVector3f Corner = (Face.N * 0.5f + Face.U * (S - 0.5f) + Face.V * (T - 0.5f)) * Size;
				const FIntVector Key(FMath::RoundToInt(Corner.X * 2.0f / Size), FMath::RoundToInt(Corner.Y * 2.0f / Size), FMath::RoundToInt(Corner.Z * 2.0f / Size));
				FVertexID VertexID;
				if (const FVertexID* Found = Corners.Find(Key))
				{
					VertexID = *Found;
				}
				else
				{
					VertexID = Description.CreateVertex();
					VertexPositions[VertexID] = Corner;
					Corners.Add(Key, VertexID);
				}

				const FVertexInstanceID InstanceID = Description.CreateVertexInstance(VertexID);
				VertexInstanceNormals[InstanceID] = Face.N;
				VertexInstanceUVs.Set(InstanceID, 0, PaletteUV);
				VertexInstanceUVs.Set(InstanceID, 1, FVector2f(static_cast<float>(S), 1.0f - static_cast<float>(T)));
				VertexInstanceColors[InstanceID] = Color;
				Instances[S][T] = InstanceID;
			}
		}

		// (P1 - P0) x (P2 - P0) = -N: the winding Unreal treats as front facing.
		FVertexInstanceID Triangle0[3] = { Instances[0][0], Instances[1][1], Instances[1][0] };
		FVertexInstanceID Triangle1[3] = { Instances[0][0], Instances[0][1], Instances[1][1] };
		Description.CreateTriangle(PolygonGroupID, MakeArrayView(Triangle0, 3));
		Description.CreateTriangle(PolygonGroupID, MakeArrayView(Triangle1, 3));
		Out.NumTriangles += 2;
	}

	VoxMeshBuilderPrivate::HardenEdges(Description);
}

void VoxMeshBuilder::CommitStaticMesh(UStaticMesh* StaticMesh, FVoxMeshData& Data, const FVoxScene& Scene,
	TFunctionRef<UMaterialInterface*(uint32 Material)> GetMaterial, bool bGenerateLightmapUVs)
{
	check(StaticMesh);

	if (StaticMesh->GetNumSourceModels() == 0)
	{
		StaticMesh->AddSourceModel();
	}
	else if (StaticMesh->GetNumSourceModels() > 1)
	{
		StaticMesh->SetNumSourceModels(1);
	}

	FStaticMeshSourceModel& SourceModel = StaticMesh->GetSourceModel(0);
	FMeshBuildSettings& BuildSettings = SourceModel.BuildSettings;
	BuildSettings.bRecomputeNormals = false;       // flat normals come from the mesher
	BuildSettings.bRecomputeTangents = true;
	BuildSettings.bUseMikkTSpace = true;
	BuildSettings.bRemoveDegenerates = true;
	BuildSettings.BuildScale3D = FVector(1.0f);    // the scale is already applied by vox4c
	BuildSettings.bGenerateLightmapUVs = bGenerateLightmapUVs;
	BuildSettings.SrcLightmapIndex = 1;            // planar voxel UVs
	BuildSettings.DstLightmapIndex = 2;
	StaticMesh->SetLightMapCoordinateIndex(bGenerateLightmapUVs ? 2 : 1);

	// Material slots in polygon group order, matched by slot name.
	TArray<FStaticMaterial>& StaticMaterials = StaticMesh->GetStaticMaterials();
	StaticMaterials.Reset();
	for (const uint32 Material : Data.Materials)
	{
		const FName SlotName = Scene.GetMaterialSlotName(Material);
		StaticMaterials.Add(FStaticMaterial(GetMaterial(Material), SlotName, SlotName));
	}

	FMeshDescription* MeshDescription = StaticMesh->CreateMeshDescription(0);
	*MeshDescription = MoveTemp(Data.Description);
	StaticMesh->CommitMeshDescription(0);

	FMeshSectionInfoMap& SectionInfoMap = StaticMesh->GetSectionInfoMap();
	SectionInfoMap.Clear();
	for (int32 SectionIndex = 0; SectionIndex < Data.Materials.Num(); ++SectionIndex)
	{
		SectionInfoMap.Set(0, SectionIndex, FMeshSectionInfo(SectionIndex));
	}
	StaticMesh->GetOriginalSectionInfoMap().CopyFrom(SectionInfoMap);

	StaticMesh->Build(false);
	StaticMesh->PostEditChange();
	StaticMesh->MarkPackageDirty();
}
