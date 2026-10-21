// Copyright (c) 2025-2026 Muppetsg2
// Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "VoxImportOption.h"

THIRD_PARTY_INCLUDES_START
#include "vox4c.h"
THIRD_PARTY_INCLUDES_END

/** What to load from a .vox file. */
struct FVoxLoadParams
{
	/** MagicaVoxel front (-Y) -> Unreal +X. Otherwise X is mirrored only (front stays -Y). */
	bool bXForward = true;
	/** Size of one voxel in Unreal units. */
	float Scale = 1.0f;
	/** Origin of model meshes. */
	EVoxPivot Pivot = EVoxPivot::MagicaVoxel;
	/** Build monotone meshes (wound the way Unreal expects). */
	bool bGenerateMeshes = false;
	/** Keep the voxel lists. */
	bool bKeepVoxels = false;
};

/**
 * Owns a vox4c scene. Every frame of an animation is loaded; use ClampFrame() to sample one.
 * Materials are compacted to the ones used by at least one voxel (v4c_material::palette_index
 * keeps the index in the 256 colour palette).
 */
class FVoxScene : public FNoncopyable
{
public:

	FVoxScene() = default;
	~FVoxScene();

	bool Load(const uint8* Data, int64 Size, const FVoxLoadParams& Params, FText& OutError);

	/** Localized description of a vox4c result code. */
	static FText GetResultText(v4c_result Result);

	bool IsValid() const { return Scene != nullptr; }
	const v4c_scene* Get() const { return Scene; }
	const v4c_scene* operator->() const { return Scene; }

	/** Frame clamped to the animation range of the file. */
	uint32 ClampFrame(int32 Frame) const;

	/** Nodes that show a model at Frame, in pre-order. Hidden nodes only if requested. */
	void GetModelNodes(uint32 Frame, bool bIncludeHidden, TArray<uint32>& OutNodes) const;

	/** Palette colour of a (compacted) material, sRGB. */
	FColor GetColor(uint32 Material) const;

	/** Texture coordinate of a material in the palette / properties textures (width = number of materials). */
	FVector2f GetPaletteUV(uint32 Material) const;

	/** Name of the material slot a material is assigned to (stable across reimports). */
	FName GetMaterialSlotName(uint32 Material) const;

	/**
	 * Every model drawn at Frame baked into one mesh, in scene space, wound for Unreal
	 * (MagicaVoxel sampling: whole frames, translations on the voxel grid). Requires a scene loaded with meshes.
	 * Returns false if nothing is drawn at that frame.
	 */
	bool BakeFrame(uint32 Frame, bool bIncludeHidden, struct FVoxBakedMesh& Out) const;

	/** The models drawn at Frame with their scene matrices (MagicaVoxel sampling). */
	void GetFrameInstances(uint32 Frame, bool bIncludeHidden, TArray<v4c_instance>& OutInstances) const;

	/** First and last frame of the animation range of the file (MagicaVoxel's timeline). */
	uint32 GetAnimationFirstFrame() const;
	uint32 GetAnimationLastFrame() const;

	/** True if a node changes its transform or model between the frames of the animation range. */
	bool HasAnimation() const;

	static FVector ToVector(const float V[3]) { return FVector(V[0], V[1], V[2]); }

	/** vox4c TRS (proper rotation quaternion and a +-1 mirror scale) as FTransform. */
	static FTransform ToTransform(const v4c_transform& Transform);

private:

	v4c_scene* Scene = nullptr;
};

/** A mesh baked by vox4c (v4c_bake_frame), freed with this object. */
struct FVoxBakedMesh : public FNoncopyable
{
	v4c_mesh Mesh;

	FVoxBakedMesh() { FMemory::Memzero(Mesh); }
	~FVoxBakedMesh() { v4c_free_mesh(&Mesh); }
};
