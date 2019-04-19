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

#include "MarchingSquaresMapRef.h"

#include "GenericWorkerThread.h"
#include "GWTTickManager.h"

#include "MarchingSquaresPlugin.h"

UMarchingSquaresMapRef::UMarchingSquaresMapRef(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Register build map callback
    Map.OnBuildMapDone().AddUObject(this, &UMarchingSquaresMapRef::OnBuildMapDoneCallback);
}

// MAP SETTINGS FUNCTIONS

void UMarchingSquaresMapRef::ApplyMapSettings()
{
    Map.SetDimension(FIntPoint(DimX, DimY));
    Map.BlockSize = BlockSize;

    Map.bOverrideBoundsZ = bOverrideBoundsZ;
    Map.BoundsSurfaceOverrideZ = BoundsSurfaceOverrideZ;
    Map.BoundsExtrudeOverrideZ = BoundsExtrudeOverrideZ;

    Map.SurfaceHeightScale = SurfaceHeightScale;
    Map.ExtrudeHeightScale = ExtrudeHeightScale;

    Map.BaseHeightOffset  = BaseHeightOffset;
    Map.HeightMapMipLevel = HeightMapMipLevel;

    Map.MeshPrefabs = MeshPrefabs;
    Map.DebugRTT = DebugRTT;
}

void UMarchingSquaresMapRef::SetHeightMap(FRULShaderTextureParameterInput TextureInput)
{
    FMarchingSquaresMap* MapRef(&Map);
    FRULShaderTextureParameterInputResource TextureResource(TextureInput.GetResource_GT());
    ENQUEUE_RENDER_COMMAND(UMarchingSquaresMapRef_SetHeightMap)(
        [MapRef, TextureResource](FRHICommandListImmediate& RHICmdList)
        {
            MapRef->SetHeightMap(TextureResource.GetTextureParamRef_RT());
        } );
}

void UMarchingSquaresMapRef::OnBuildMapDoneCallback(bool bBuildMapResult, uint32 FillType)
{
    FGWTTickManager& TickManager(IGenericWorkerThread::Get().GetTickManager());
    FGWTTickManager::FTickCallback TickCallback(
        [this, bBuildMapResult, FillType]()
        {
            OnBuildMapDone.Broadcast(bBuildMapResult, FillType);
        } );
    TickManager.EnqueueTickCallback(TickCallback);
}

void UMarchingSquaresMapRef::GetMapDimensionData(FIntPoint& MapDimensionI, FVector2D& MapDimensionV, FIntPoint& VoxDimensionI, FVector2D& VoxDimensionV)
{
    MapDimensionI = FIntPoint(DimX, DimY);
    MapDimensionV = FVector2D(DimX, DimY);

    VoxDimensionI = MapDimensionI / BlockSize;
    VoxDimensionI.X *= BlockSize-1;
    VoxDimensionI.Y *= BlockSize-1;

    VoxDimensionV = FVector2D(VoxDimensionI.X, VoxDimensionI.Y);
}

void UMarchingSquaresMapRef::BuildMap(int32 FillType, bool bGenerateWalls)
{
    if (Map.HasValidDimension())
    {
        Map.BuildMap(FMath::Max(0, FillType), bGenerateWalls);
    }
    else
    {
        UE_LOG(LogMSQ,Warning, TEXT("UMarchingSquaresMapRef::BuildMap() ABORTED - Invalid map dimension"));
    }
}

void UMarchingSquaresMapRef::ClearMap()
{
    Map.ClearMap();
}

// SECTION FUNCTIONS

bool UMarchingSquaresMapRef::HasSection(int32 FillType, int32 Index) const
{
    return Map.HasSection(FillType, Index);
}

bool UMarchingSquaresMapRef::HasSectionGeometry(int32 FillType, int32 Index) const
{
    if (HasSection(FillType, Index))
    {
        const FPMUMeshSection& Section(Map.GetSectionChecked(FillType, Index));
        return Section.HasGeometry();
    }

    return false;
}

int32 UMarchingSquaresMapRef::GetSectionCount() const
{
    int32 SectionCountX = DimX / BlockSize;
    int32 SectionCountY = DimY / BlockSize;
    return SectionCountX * SectionCountY;
}

FPMUMeshSectionRef UMarchingSquaresMapRef::GetSection(int32 FillType, int32 Index)
{
    return Map.HasSection(FillType, Index)
        ? FPMUMeshSectionRef(Map.GetSectionChecked(FillType, Index))
        : FPMUMeshSectionRef();
}

// PREFAB FUNCTIONS

//TArray<FBox2D> UMarchingSquaresMapRef::GetPrefabBounds(int32 PrefabIndex) const
//{
//    //return HasValidMap()
//    //    ? Map.GetPrefabBounds(PrefabIndex)
//    //    : TArray<FBox2D>();
//
//    return TArray<FBox2D>();
//}

//bool UMarchingSquaresMapRef::HasPrefab(int32 PrefabIndex) const
//{
//    return HasValidMap() ? Map.HasPrefab(PrefabIndex) : false;
//}

//bool UMarchingSquaresMapRef::ApplyPrefab(
//    int32 PrefabIndex,
//    int32 SectionIndex,
//    FVector Location,
//    bool bApplyHeightMap,
//    bool bInverseHeightMap,
//    FPMUMeshSection& OutSection
//    )
//{
//    //if (HasValidMap())
//    //{
//    //    return Map.ApplyPrefab(
//    //        PrefabIndex,
//    //        SectionIndex,
//    //        Location,
//    //        bApplyHeightMap,
//    //        bInverseHeightMap,
//    //        OutSection
//    //        );
//    //}
//
//    return false;
//}
