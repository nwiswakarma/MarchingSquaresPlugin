////////////////////////////////////////////////////////////////////////////////
//
// MIT License
// 
// Copyright (c) 2018-2019 Nuraga Wiswakarma
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
////////////////////////////////////////////////////////////////////////////////
// 

#pragma once

#include "CoreMinimal.h"
#include "MarchingSquaresMap.h"
#include "Mesh/PMUMeshTypes.h"
#include "Shaders/RULShaderParameters.h"
#include "MarchingSquaresMapRef.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMarchingSquaresMapRef_OnBuildMapDone, bool, bResult, int32, FillType);

UCLASS(BlueprintType, Blueprintable)
class UMarchingSquaresMapRef : public UObject
{
    GENERATED_BODY()

    FMarchingSquaresMap Map;

    void OnBuildMapDoneCallback(bool bBuildMapResult, uint32 FillType);

public:

    UPROPERTY(EditAnywhere, Category="Map Settings", BlueprintReadWrite, meta=(ClampMin="1", UIMin="1", DisplayName="Dimension X"))
    int32 DimX = 256;

    UPROPERTY(EditAnywhere, Category="Map Settings", BlueprintReadWrite, meta=(ClampMin="1", UIMin="1", DisplayName="Dimension Y"))
    int32 DimY = 256;

    UPROPERTY(EditAnywhere, Category="Map Settings", BlueprintReadWrite, meta=(ClampMin="1", UIMin="1"))
    int32 BlockSize = 64;

    UPROPERTY(EditAnywhere, Category="Map Settings", BlueprintReadWrite)
    bool bOverrideBoundsZ = false;

    UPROPERTY(EditAnywhere, Category="Map Settings", BlueprintReadWrite)
    float BoundsSurfaceOverrideZ = 0.f;

    UPROPERTY(EditAnywhere, Category="Map Settings", BlueprintReadWrite)
    float BoundsExtrudeOverrideZ = 0.f;

    UPROPERTY(EditAnywhere, Category="Height Settings", BlueprintReadWrite)
    float SurfaceHeightScale = 1.0f;

    UPROPERTY(EditAnywhere, Category="Height Settings", BlueprintReadWrite)
    float ExtrudeHeightScale = -1.0f;

    UPROPERTY(EditAnywhere, Category="Height Settings", BlueprintReadWrite)
    float BaseHeightOffset = 0.5f;

    UPROPERTY(EditAnywhere, Category="Texture Settings", BlueprintReadWrite)
    int32 HeightMapMipLevel = 0;

    UPROPERTY(BlueprintAssignable, Category="Map Settings")
    FMarchingSquaresMapRef_OnBuildMapDone OnBuildMapDone;

    UPROPERTY(BlueprintReadWrite, Category="Prefabs")
    TArray<class UStaticMesh*> MeshPrefabs;

    UPROPERTY(BlueprintReadWrite, Category="Debug")
    UTextureRenderTarget2D* DebugRTT;

    UMarchingSquaresMapRef(const FObjectInitializer& ObjectInitializer);

    FORCEINLINE FMarchingSquaresMap& GetMap()
    {
        return Map;
    }

    FORCEINLINE const FMarchingSquaresMap& GetMap() const
    {
        return Map;
    }

    UFUNCTION(BlueprintCallable)
    bool HasValidMap() const
    {
        return Map.HasValidDimension();
    }

    void ApplyMapSettings();

    UFUNCTION(BlueprintCallable, DisplayName="Apply Map Settings")
    void ApplyMapSettingsBP()
    {
        ApplyMapSettings();
    }

    UFUNCTION(BlueprintCallable)
    FIntPoint GetMapDimension() const
    {
        return Map.GetDimension();
    }

    UFUNCTION(BlueprintCallable)
    void GetMapDimensionData(FIntPoint& MapDimensionI, FVector2D& MapDimensionV, FIntPoint& VoxDimensionI, FVector2D& VoxDimensionV);

    UFUNCTION(BlueprintCallable)
    void BuildMap(int32 FillType, bool bGenerateWalls);

    UFUNCTION(BlueprintCallable)
    void ClearMap();

    UFUNCTION(BlueprintCallable)
    void SetHeightMap(FRULShaderTextureParameterInput TextureInput);

    // SECTION FUNCTIONS

    UFUNCTION(BlueprintCallable)
    bool HasSection(int32 FillType, int32 Index) const;

    UFUNCTION(BlueprintCallable)
    bool HasSectionGeometry(int32 FillType, int32 Index) const;

    UFUNCTION(BlueprintCallable)
    int32 GetSectionCount() const;

    UFUNCTION(BlueprintCallable)
    FPMUMeshSectionRef GetSection(int32 FillType, int32 Index);

    // PREFAB FUNCTIONS

    //UFUNCTION(BlueprintCallable)
    //bool HasPrefab(int32 PrefabIndex) const;

    //UFUNCTION(BlueprintCallable)
    //TArray<FBox2D> GetPrefabBounds(int32 PrefabIndex) const;

    //UFUNCTION(BlueprintCallable)
    //bool ApplyPrefab(
    //    int32 PrefabIndex,
    //    int32 SectionIndex,
    //    FVector Center,
    //    bool bApplyHeightMap,
    //    bool bInverseHeightMap,
    //    UPARAM(ref) FPMUMeshSection& OutSection
    //    );
};
