// Copyright 2016-2018 mik14a / Admix Network. All Rights Reserved.
// Edited by Muppetsg2 2025

#pragma once

#include <CoreMinimal.h>

enum class EVoxMaterialType : uint8
{
	DIFFUSE = 0,	// _diffuse
	METAL,			// _metal
	GLASS,			// _glass
	EMIT			// _emit
};

struct FVoxMaterial
{
	EVoxMaterialType Type = EVoxMaterialType::DIFFUSE; // _type
	float Weight        = 1.0f;		// _weight
	float Roughness     = 0.1f;		// _rough
	float Metallic      = 0.0f;		// _metal
	float Specular      = 1.0f;		// _spec || _sp
	float IOR           = 1.3f;		// _ior || _ri - 1.0f
	float Att           = 1.0f;		// _att
	float Emissive      = 0.0f;		// _emit
	float EmissionPower = 0.0f;		// _flux
	float LDR           = 0.0f;		// _ldr
	float Transparency  = 0.0f;		// _trans || _alpha
	bool Plastic        = false;	// _plastic
};