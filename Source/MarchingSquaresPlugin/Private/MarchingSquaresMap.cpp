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

#include "MarchingSquaresMap.h"

#include "ShaderParameters.h"
#include "ShaderCore.h"
#include "ShaderParameterUtils.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/TextureRenderTarget2D.h"
#include "RHIUtilities.h"

#include "MarchingSquaresPlugin.h"
#include "RenderingUtilityLibrary.h"
#include "RHI/RULAlignedTypes.h"
#include "Shaders/RULShaderDefinitions.h"
#include "Shaders/RULPrefixSumScan.h"

// COMPUTE SHADER DEFINITIONS

template<uint32 bGenerateWalls>
class TMarchingSquaresMapWriteCellCaseCS : public FRULBaseComputeShader<16,16,1>
{
public:

    typedef FRULBaseComputeShader<16,16,1> FBaseType;

    DECLARE_SHADER_TYPE(TMarchingSquaresMapWriteCellCaseCS, Global);

public:

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return RHISupportsComputeShaders(Parameters.Platform);
    }

    static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
    {
        FBaseType::ModifyCompilationEnvironment(Parameters, OutEnvironment);
        OutEnvironment.SetDefine(TEXT("MARCHING_SQUARES_GENERATE_WALLS"), bGenerateWalls);
    }

    RUL_DECLARE_SHADER_CONSTRUCTOR_SERIALIZER(TMarchingSquaresMapWriteCellCaseCS)

    RUL_DECLARE_SHADER_PARAMETERS_1(
        SRV,
        FShaderResourceParameter,
        FResourceId,
        "VoxelStateData", VoxelStateData
        )

    RUL_DECLARE_SHADER_PARAMETERS_3(
        UAV,
        FShaderResourceParameter,
        FResourceId,
        "OutCellCaseData",  OutCellCaseData,
        "OutGeomCountData", OutGeomCountData,
        "OutDebugTexture",  OutDebugTexture
        )

    RUL_DECLARE_SHADER_PARAMETERS_3(
        Value,
        FShaderParameter,
        FParameterId,
        "_GDim",     Params_GDim,
        "_LDim",     Params_LDim,
        "_FillType", Params_FillType
        )
};

class FMarchingSquaresMapWriteCellCompactIdCS : public FRULBaseComputeShader<16,16,1>
{
    typedef FRULBaseComputeShader<16,16,1> FBaseType;

    RUL_DECLARE_SHADER_CONSTRUCTOR_DEFAULT_STATICS(
        FMarchingSquaresMapWriteCellCompactIdCS,
        Global,
        RHISupportsComputeShaders(Parameters.Platform)
        )

    RUL_DECLARE_SHADER_PARAMETERS_2(
        SRV,
        FShaderResourceParameter,
        FResourceId,
        "GeomCountData", GeomCountData,
        "OffsetData",    OffsetData
        )

    RUL_DECLARE_SHADER_PARAMETERS_2(
        UAV,
        FShaderResourceParameter,
        FResourceId,
        "OutFillCellIdData", OutFillCellIdData,
        "OutEdgeCellIdData", OutEdgeCellIdData
        )

    RUL_DECLARE_SHADER_PARAMETERS_2(
        Value,
        FShaderParameter,
        FParameterId,
        "_GDim", Params_GDim,
        "_LDim", Params_LDim
        )
};

template<uint32 bGenerateWalls>
class TMarchingSquaresMapTriangulateFillCellCS : public FRULBaseComputeShader<256,1,1>
{
public:

    typedef FRULBaseComputeShader<256,1,1> FBaseType;

    DECLARE_SHADER_TYPE(TMarchingSquaresMapTriangulateFillCellCS, Global);

public:

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return RHISupportsComputeShaders(Parameters.Platform);
    }

    static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
    {
        FBaseType::ModifyCompilationEnvironment(Parameters, OutEnvironment);
        OutEnvironment.SetDefine(TEXT("MARCHING_SQUARES_GENERATE_WALLS"), bGenerateWalls);
    }

    RUL_DECLARE_SHADER_CONSTRUCTOR_SERIALIZER_WITH_TEXTURE(TMarchingSquaresMapTriangulateFillCellCS)

    RUL_DECLARE_SHADER_PARAMETERS_1(
        Texture,
        FShaderResourceParameter,
        FResourceId,
        "HeightMap", HeightMap
        )

    RUL_DECLARE_SHADER_PARAMETERS_1(
        Sampler,
        FShaderResourceParameter,
        FResourceId,
        "samplerHeightMap", SurfaceHeightMapSampler
        )

    RUL_DECLARE_SHADER_PARAMETERS_3(
        SRV,
        FShaderResourceParameter,
        FResourceId,
        "OffsetData",     OffsetData,
        "SumData",        SumData,
        "FillCellIdData", FillCellIdData
        )

    RUL_DECLARE_SHADER_PARAMETERS_5(
        UAV,
        FShaderResourceParameter,
        FResourceId,
        "OutPositionData", OutPositionData,
        "OutTexCoordData", OutTexCoordData,
        "OutTangentData",  OutTangentData,
        "OutColorData",    OutColorData,
        "OutIndexData",    OutIndexData
        )

    RUL_DECLARE_SHADER_PARAMETERS_9(
        Value,
        FShaderParameter,
        FParameterId,
        "_GDim",          Params_GDim,
        "_LDim",          Params_LDim,
        "_GeomCount",     Params_GeomCount,
        "_SampleLevel",   Params_SampleLevel,
        "_FillCellCount", Params_FillCellCount,
        "_BlockOffset",   Params_BlockOffset,
        "_HeightScale",   Params_HeightScale,
        "_HeightOffset",  Params_HeightOffset,
        "_Color",         Params_Color
        )
};

template<uint32 bGenerateWalls>
class TMarchingSquaresMapTriangulateEdgeCellCS : public FRULBaseComputeShader<256,1,1>
{
public:

    typedef FRULBaseComputeShader<256,1,1> FBaseType;

    DECLARE_SHADER_TYPE(TMarchingSquaresMapTriangulateEdgeCellCS, Global);

public:

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return RHISupportsComputeShaders(Parameters.Platform);
    }

    static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
    {
        FBaseType::ModifyCompilationEnvironment(Parameters, OutEnvironment);
        OutEnvironment.SetDefine(TEXT("MARCHING_SQUARES_GENERATE_WALLS"), bGenerateWalls);
    }

    RUL_DECLARE_SHADER_CONSTRUCTOR_SERIALIZER_WITH_TEXTURE(TMarchingSquaresMapTriangulateEdgeCellCS)

    RUL_DECLARE_SHADER_PARAMETERS_1(
        Texture,
        FShaderResourceParameter,
        FResourceId,
        "HeightMap", HeightMap
        )

    RUL_DECLARE_SHADER_PARAMETERS_1(
        Sampler,
        FShaderResourceParameter,
        FResourceId,
        "samplerHeightMap", SurfaceHeightMapSampler
        )

    RUL_DECLARE_SHADER_PARAMETERS_5(
        SRV,
        FShaderResourceParameter,
        FResourceId,
        "VoxelFeatureData", VoxelFeatureData,
        "OffsetData",       OffsetData,
        "SumData",          SumData,
        "EdgeCellIdData",   EdgeCellIdData,
        "CellCaseData",     CellCaseData
        )

    RUL_DECLARE_SHADER_PARAMETERS_5(
        UAV,
        FShaderResourceParameter,
        FResourceId,
        "OutPositionData", OutPositionData,
        "OutTexCoordData", OutTexCoordData,
        "OutTangentData",  OutTangentData,
        "OutColorData",    OutColorData,
        "OutIndexData",    OutIndexData
        )

    RUL_DECLARE_SHADER_PARAMETERS_9(
        Value,
        FShaderParameter,
        FParameterId,
        "_GDim",          Params_GDim,
        "_LDim",          Params_LDim,
        "_GeomCount",     Params_GeomCount,
        "_SampleLevel",   Params_SampleLevel,
        "_EdgeCellCount", Params_FillCellCount,
        "_BlockOffset",   Params_BlockOffset,
        "_HeightScale",   Params_HeightScale,
        "_HeightOffset",  Params_HeightOffset,
        "_Color",         Params_Color
        )
};

IMPLEMENT_SHADER_TYPE(template<>, TMarchingSquaresMapWriteCellCaseCS<0>, TEXT("/Plugin/MarchingSquaresPlugin/Private/MarchingSquaresCS.usf"), TEXT("CellWriteCaseKernel"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, TMarchingSquaresMapWriteCellCaseCS<1>, TEXT("/Plugin/MarchingSquaresPlugin/Private/MarchingSquaresCS.usf"), TEXT("CellWriteCaseKernel"), SF_Compute);

IMPLEMENT_SHADER_TYPE(, FMarchingSquaresMapWriteCellCompactIdCS, TEXT("/Plugin/MarchingSquaresPlugin/Private/MarchingSquaresCS.usf"), TEXT("CellWriteCompactIdKernel"), SF_Compute);

IMPLEMENT_SHADER_TYPE(template<>, TMarchingSquaresMapTriangulateFillCellCS<0>, TEXT("/Plugin/MarchingSquaresPlugin/Private/MarchingSquaresCS.usf"), TEXT("TriangulateFillCell"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, TMarchingSquaresMapTriangulateFillCellCS<1>, TEXT("/Plugin/MarchingSquaresPlugin/Private/MarchingSquaresCS.usf"), TEXT("TriangulateFillCell"), SF_Compute);

IMPLEMENT_SHADER_TYPE(template<>, TMarchingSquaresMapTriangulateEdgeCellCS<0>, TEXT("/Plugin/MarchingSquaresPlugin/Private/MarchingSquaresCS.usf"), TEXT("TriangulateEdgeCell"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, TMarchingSquaresMapTriangulateEdgeCellCS<1>, TEXT("/Plugin/MarchingSquaresPlugin/Private/MarchingSquaresCS.usf"), TEXT("TriangulateEdgeCell"), SF_Compute);

void FMarchingSquaresMap::SetDimension(FIntPoint InDimension)
{
    if (Dimension_GT != InDimension)
    {
        Dimension_GT = InDimension;
    }
}

void FMarchingSquaresMap::SetHeightMap(FTexture2DRHIParamRef InHeightMap)
{
    HeightMap = InHeightMap;
}

void FMarchingSquaresMap::ClearMap()
{
    FMarchingSquaresMap* Map(this);
    ENQUEUE_RENDER_COMMAND(FMarchingSquaresMap_ClearMap)(
        [Map](FRHICommandListImmediate& RHICmdList)
        {
            Map->ClearMap_RT(RHICmdList);
        } );
}

void FMarchingSquaresMap::InitializeVoxelData()
{
    check(HasValidDimension());

    FMarchingSquaresMap* Map(this);
    FIntPoint Dimension(Dimension_GT);
    ENQUEUE_RENDER_COMMAND(FMarchingSquaresMap_InitializeVoxelData)(
        [Map, Dimension](FRHICommandListImmediate& RHICmdList)
        {
            Map->InitializeVoxelData_RT(RHICmdList, Dimension);
        } );
}

void FMarchingSquaresMap::ClearMap_RT(FRHICommandListImmediate& RHICmdList)
{
    VoxelStateData.Release();
    VoxelFeatureData.Release();

    DebugRTTRHI = nullptr;
	DebugTextureRHI.SafeRelease();
    DebugTextureUAV.SafeRelease();
}

void FMarchingSquaresMap::InitializeVoxelData_RT(FRHICommandListImmediate& RHICmdList, FIntPoint InDimension)
{
    check(IsInRenderingThread());

    // Update dimension and clear previous voxel data if required

    if (Dimension_RT != InDimension)
    {
        VoxelStateData.Release();
        VoxelFeatureData.Release();

        Dimension_RT = InDimension;
    }

    check(HasValidDimension_RT());

    FIntPoint Dimension = Dimension_RT;
    int32 VoxelCount = GetVoxelCount_RT();

    // Create UAV compatible debug texture if debug RTT is valid

    if (DebugRTT && ! DebugTextureRHI)
    {
        DebugRTTRHI = DebugRTT->GetRenderTargetResource()->TextureRHI;

        if (DebugRTTRHI && DebugRTT->GetFormat() == PF_FloatRGBA)
        {
            FRHIResourceCreateInfo CreateInfo;
            DebugTextureRHI = RHICreateTexture2D(Dimension.X, Dimension.Y, PF_FloatRGBA, 1, 1, TexCreate_ShaderResource | TexCreate_UAV, CreateInfo);
            DebugTextureUAV = RHICreateUnorderedAccessView(DebugTextureRHI);
        }
    }

    typedef TResourceArray<FRULAlignedUint, VERTEXBUFFER_ALIGNMENT> FVoxelData;

    // Construct voxel state data

    if (! VoxelStateData.IsValid())
    {
        FVoxelData VoxelStateDefaultData(false);
        VoxelStateDefaultData.SetNumZeroed(VoxelCount);

        VoxelStateData.Initialize(
            sizeof(FVoxelData::ElementType),
            VoxelCount,
            PF_R32_UINT,
            &VoxelStateDefaultData,
            BUF_Static,
            TEXT("VoxelStateData")
            );
    }

    // Construct cell feature data

    if (! VoxelFeatureData.IsValid())
    {
        FVoxelData VoxelFeatureDefaultData(false);
        VoxelFeatureDefaultData.SetNumUninitialized(VoxelCount);
        FMemory::Memset(VoxelFeatureDefaultData.GetData(), 0xFF, VoxelFeatureDefaultData.GetResourceDataSize());

        VoxelFeatureData.Initialize(
            sizeof(FVoxelData::ElementType),
            VoxelCount,
            PF_R32_UINT,
            &VoxelFeatureDefaultData,
            BUF_Static,
            TEXT("VoxelFeatureData")
            );
    }
}

void FMarchingSquaresMap::BuildMap(int32 FillType, bool bGenerateWalls)
{
    uint32 BuildFillType = FMath::Max(0, FillType);
    bool bCallResult = BuildMapExec(BuildFillType, bGenerateWalls);

    // Call failed, broadcast build map done event
    if (! bCallResult)
    {
        BuildMapDoneEvent.Broadcast(false, BuildFillType);
    }
}

bool FMarchingSquaresMap::BuildMapExec(uint32 FillType, bool bGenerateWalls)
{
    if (! HasValidDimension())
    {
        UE_LOG(LogMSQ,Warning, TEXT("FMarchingSquaresMap::BuildMap() ABORTED - Invalid map dimension"));
        return false;
    }

    FMarchingSquaresMap* Map(this);
    ENQUEUE_RENDER_COMMAND(FMarchingSquaresMap_BuildMap)(
        [Map, FillType, bGenerateWalls](FRHICommandListImmediate& RHICmdList)
        {
            Map->BuildMap_RT(RHICmdList, FillType, bGenerateWalls, GMaxRHIFeatureLevel);
        } );

    return true;
}

void FMarchingSquaresMap::BuildMap_RT(FRHICommandListImmediate& RHICmdList, uint32 FillType, bool bGenerateWalls, ERHIFeatureLevel::Type InFeatureLevel)
{
    checkf(VoxelStateData.IsValid()  , TEXT("FMarchingSquaresMap::BuildMap() ABORTED - Dimension has been updated and InitializeVoxelData() has not been called"));
    checkf(VoxelFeatureData.IsValid(), TEXT("FMarchingSquaresMap::BuildMap() ABORTED - Dimension has been updated and InitializeVoxelData() has not been called"));
    check(IsInRenderingThread());
    check(HasValidDimension_RT());

    RHICmdListPtr = &RHICmdList;
    RHIShaderMap  = GetGlobalShaderMap(InFeatureLevel);

    check(RHIShaderMap != nullptr);

    GenerateMarchingCubes_RT(FillType, bGenerateWalls);

    // Copy resolve debug texture to debug rtt

    if (DebugRTTRHI && DebugTextureRHI)
    {
        RHICmdList.CopyToResolveTarget(
            DebugTextureRHI,
            DebugRTTRHI,
            FResolveParams()
            );

        DebugTextureUAV.SafeRelease();
        DebugTextureRHI.SafeRelease();
        DebugRTTRHI = nullptr;
    }

    RHICmdListPtr = nullptr;
    RHIShaderMap  = nullptr;

    BuildMapDoneEvent.Broadcast(true, FillType);
}

void FMarchingSquaresMap::GenerateMarchingCubes_RT(uint32 FillType, bool bInGenerateWalls)
{
    check(IsInRenderingThread());
    check(RHICmdListPtr != nullptr);
    check(VoxelStateData.IsValid());
    check(VoxelFeatureData.IsValid());
    check(HasValidDimension_RT());

    FIntPoint Dimension = Dimension_RT;
    int32 VoxelCount = GetVoxelCount();

    FRHICommandListImmediate& RHICmdList(*RHICmdListPtr);

    const bool bUseDualMesh = ! bInGenerateWalls;

    typedef TResourceArray<FRULAlignedUint, VERTEXBUFFER_ALIGNMENT> FIndexData;

    // Construct cell case data

    FRULRWBuffer CellCaseData;
    CellCaseData.Initialize(
        sizeof(FIndexData::ElementType),
        VoxelCount,
        PF_R32_UINT,
        BUF_Static,
        TEXT("CellCaseData")
        );

    // Construct cell geometry count data

    typedef FRULAlignedUintVector4 FGeomCountDataType;
    typedef TResourceArray<FGeomCountDataType, VERTEXBUFFER_ALIGNMENT> FGeomCountData;

    FGeomCountData GeomCountDefaultData(false);
    GeomCountDefaultData.SetNumZeroed(VoxelCount);
    
    FRULRWBufferStructured GeomCountData;
    GeomCountData.Initialize(
        sizeof(FGeomCountData::ElementType),
        VoxelCount,
        &GeomCountDefaultData,
        BUF_Static,
        TEXT("GeomCountData")
        );

    // Write cell case data

    RHICmdList.BeginComputePass(TEXT("MarchingSquaresMapWriteCellCase"));
    {
        TMarchingSquaresMapWriteCellCaseCS<0>::FBaseType* ComputeShader;

        if (bUseDualMesh)
        {
            ComputeShader = *TShaderMapRef<TMarchingSquaresMapWriteCellCaseCS<0>>(RHIShaderMap);
        }
        else
        {
            ComputeShader = *TShaderMapRef<TMarchingSquaresMapWriteCellCaseCS<1>>(RHIShaderMap);
        }

        ComputeShader->SetShader(RHICmdList);
        ComputeShader->BindSRV(RHICmdList, TEXT("VoxelStateData"), VoxelStateData.SRV);
        ComputeShader->BindUAV(RHICmdList, TEXT("OutCellCaseData"), CellCaseData.UAV);
        ComputeShader->BindUAV(RHICmdList, TEXT("OutGeomCountData"), GeomCountData.UAV);
        ComputeShader->BindUAV(RHICmdList, TEXT("OutDebugTexture"), DebugTextureUAV);
        ComputeShader->SetParameter(RHICmdList, TEXT("_GDim"), Dimension);
        ComputeShader->SetParameter(RHICmdList, TEXT("_LDim"), FIntPoint(BlockSize, BlockSize));
        ComputeShader->SetParameter(RHICmdList, TEXT("_FillType"), FillType);
        ComputeShader->DispatchAndClear(RHICmdList, Dimension.X, Dimension.Y, 1);
    }
    RHICmdList.EndComputePass();

    // Scan cell geometry count data to generate geometry offset and sum data

    FRULRWBufferStructured OffsetData;
    FRULRWBufferStructured SumData;

    const int32 ScanBlockCount = FRULPrefixSumScan::ExclusiveScan4D(
        GeomCountData.SRV,
        sizeof(FGeomCountData::ElementType),
        VoxelCount,
        OffsetData,
        SumData,
        BUF_Static
        );

    // Get geometry count scan sum data

    check(ScanBlockCount > 0);
    check(SumData.Buffer->GetStride() > 0);

    const int32 SumDataCount = SumData.Buffer->GetSize() / SumData.Buffer->GetStride();
    FGeomCountData SumArr;
    SumArr.SetNumUninitialized(SumDataCount);

    check(SumDataCount > 0);

    void* SumDataPtr = RHILockStructuredBuffer(SumData.Buffer, 0, SumData.Buffer->GetSize(), RLM_ReadOnly);
    FMemory::Memcpy(SumArr.GetData(), SumDataPtr, SumData.Buffer->GetSize());
    RHIUnlockStructuredBuffer(SumData.Buffer);

    FGeomCountDataType BufferSum = SumArr[ScanBlockCount];

    UE_LOG(UntMSQ,Warning, TEXT("FMarchingSquaresMap::GenerateMarchingCubes_RT() BufferSum: X=%d,Y=%d,Z=%d,W=%d"), BufferSum.X,BufferSum.Y,BufferSum.Z,BufferSum.W);

    // Calculate geometry allocation sizes

    const int32 BlockOffset = FRULPrefixSumScan::GetBlockOffsetForSize(BlockSize*BlockSize);

    const int32 VCount = BufferSum.X;
    const int32 ICount = BufferSum.Y;

    const int32 FillCellCount = BufferSum.Z;
    const int32 EdgeCellCount = BufferSum.W;

    int32 TotalVCount = VCount;
    int32 TotalICount = ICount;

    if (bUseDualMesh)
    {
        TotalVCount *= 2;
        TotalICount *= 2;
    }
    
    UE_LOG(UntMSQ,Warning, TEXT("FMarchingSquaresMap::GenerateMarchingCubes_RT() BlockOffset: %d"), BlockOffset);
    UE_LOG(UntMSQ,Warning, TEXT("FMarchingSquaresMap::GenerateMarchingCubes_RT() VCount: %d"), VCount);
    UE_LOG(UntMSQ,Warning, TEXT("FMarchingSquaresMap::GenerateMarchingCubes_RT() ICount: %d"), ICount);
    UE_LOG(UntMSQ,Warning, TEXT("FMarchingSquaresMap::GenerateMarchingCubes_RT() FillCellCount: %d"), FillCellCount);
    UE_LOG(UntMSQ,Warning, TEXT("FMarchingSquaresMap::GenerateMarchingCubes_RT() EdgeCellCount: %d"), EdgeCellCount);
    UE_LOG(UntMSQ,Warning, TEXT("FMarchingSquaresMap::GenerateMarchingCubes_RT() TotalVCount: %d"), TotalVCount);
    UE_LOG(UntMSQ,Warning, TEXT("FMarchingSquaresMap::GenerateMarchingCubes_RT() TotalICount: %d"), TotalICount);

    // Empty map, return
    if (TotalVCount < 3 || TotalICount < 3)
    {
        return;
    }

    // Generate compact triangulation data
    
    FRULRWBuffer FillCellIdData;
    FRULRWBuffer EdgeCellIdData;

    if (FillCellCount > 0)
    {
        FillCellIdData.Initialize(
            sizeof(FIndexData::ElementType),
            FillCellCount,
            PF_R32_UINT,
            BUF_Static,
            TEXT("FillCellIdData")
            );
    }

    if (EdgeCellCount > 0)
    {
        EdgeCellIdData.Initialize(
            sizeof(FIndexData::ElementType),
            EdgeCellCount,
            PF_R32_UINT,
            BUF_Static,
            TEXT("EdgeCellIdData")
            );
    }

    RHICmdList.BeginComputePass(TEXT("MarchingSquaresMapWriteCellCompactId"));
    {
        TShaderMapRef<FMarchingSquaresMapWriteCellCompactIdCS> CellWriteCompactIdCS(RHIShaderMap);
        CellWriteCompactIdCS->SetShader(RHICmdList);
        CellWriteCompactIdCS->BindSRV(RHICmdList, TEXT("GeomCountData"), GeomCountData.SRV);
        CellWriteCompactIdCS->BindSRV(RHICmdList, TEXT("OffsetData"), OffsetData.SRV);
        CellWriteCompactIdCS->BindUAV(RHICmdList, TEXT("OutFillCellIdData"), FillCellIdData.UAV);
        CellWriteCompactIdCS->BindUAV(RHICmdList, TEXT("OutEdgeCellIdData"), EdgeCellIdData.UAV);
        CellWriteCompactIdCS->SetParameter(RHICmdList, TEXT("_GDim"), Dimension);
        CellWriteCompactIdCS->SetParameter(RHICmdList, TEXT("_LDim"), FIntPoint(BlockSize, BlockSize));
        CellWriteCompactIdCS->DispatchAndClear(RHICmdList, Dimension.X, Dimension.Y, 1);
    }
    RHICmdList.EndComputePass();

    FRWByteAddressBuffer PositionData;
    FRWByteAddressBuffer TangentData;
    FRULRWBuffer TexCoordData;
    FRULRWBuffer ColorData;
    FRULRWBuffer IndexData;

    PositionData.Initialize(TotalVCount*3*sizeof(float), BUF_Static);

    TangentData.Initialize(TotalVCount*2*sizeof(FRULAlignedUint), BUF_Static);

    TexCoordData.Initialize(
        sizeof(FRULAlignedVector2D),
        TotalVCount,
        PF_G32R32F,
        BUF_Static,
        TEXT("UV Data")
        );

    ColorData.Initialize(
        sizeof(FRULAlignedUint),
        TotalVCount,
        PF_R32_UINT,
        BUF_Static,
        TEXT("Color Data")
        );

    IndexData.Initialize(
        sizeof(FIndexData::ElementType),
        TotalICount,
        PF_R32_UINT,
        BUF_Static,
        TEXT("Index Data")
        );

    if (FillCellCount > 0)
    {
        RHICmdList.BeginComputePass(TEXT("MarchingSquaresMapTriangulateFillCell"));

        TMarchingSquaresMapTriangulateFillCellCS<0>::FBaseType* ComputeShader;

        if (bUseDualMesh)
        {
            ComputeShader = *TShaderMapRef<TMarchingSquaresMapTriangulateFillCellCS<0>>(RHIShaderMap);
        }
        else
        {
            ComputeShader = *TShaderMapRef<TMarchingSquaresMapTriangulateFillCellCS<1>>(RHIShaderMap);
        }

        FSamplerStateRHIParamRef HeightMapSampler = TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();

        FVector2D HeightScale;
        HeightScale.X = SurfaceHeightScale;
        HeightScale.Y = ExtrudeHeightScale;

        FIntPoint GeomCount;
        GeomCount.X = VCount;
        GeomCount.Y = ICount;

        float  HeightOffset = BaseHeightOffset;
        uint32 SampleLevel  = FMath::Max(0, HeightMapMipLevel);

        ComputeShader->SetShader(RHICmdList);
        ComputeShader->BindTexture(RHICmdList, TEXT("HeightMap"), TEXT("samplerHeightMap"), HeightMap, HeightMapSampler);
        ComputeShader->BindSRV(RHICmdList, TEXT("OffsetData"),     OffsetData.SRV);
        ComputeShader->BindSRV(RHICmdList, TEXT("SumData"),        SumData.SRV);
        ComputeShader->BindSRV(RHICmdList, TEXT("FillCellIdData"), FillCellIdData.SRV);
        ComputeShader->BindUAV(RHICmdList, TEXT("OutPositionData"), PositionData.UAV);
        ComputeShader->BindUAV(RHICmdList, TEXT("OutTangentData"),  TangentData.UAV);
        ComputeShader->BindUAV(RHICmdList, TEXT("OutTexCoordData"), TexCoordData.UAV);
        ComputeShader->BindUAV(RHICmdList, TEXT("OutColorData"),    ColorData.UAV);
        ComputeShader->BindUAV(RHICmdList, TEXT("OutIndexData"),    IndexData.UAV);
        ComputeShader->SetParameter(RHICmdList, TEXT("_GDim"),          Dimension);
        ComputeShader->SetParameter(RHICmdList, TEXT("_LDim"),          FIntPoint(BlockSize, BlockSize));
        ComputeShader->SetParameter(RHICmdList, TEXT("_GeomCount"),     GeomCount);
        ComputeShader->SetParameter(RHICmdList, TEXT("_SampleLevel"),   SampleLevel);
        ComputeShader->SetParameter(RHICmdList, TEXT("_FillCellCount"), FillCellCount);
        ComputeShader->SetParameter(RHICmdList, TEXT("_BlockOffset"),   BlockOffset);
        ComputeShader->SetParameter(RHICmdList, TEXT("_HeightScale"),   HeightScale);
        ComputeShader->SetParameter(RHICmdList, TEXT("_HeightOffset"),  HeightOffset);
        ComputeShader->SetParameter(RHICmdList, TEXT("_Color"),         FVector4(1,0,0,1));
        ComputeShader->DispatchAndClear(RHICmdList, FillCellCount, 1, 1);

        RHICmdList.EndComputePass();
    }

    if (EdgeCellCount > 0)
    {
        RHICmdList.BeginComputePass(TEXT("MarchingSquaresMapTriangulateEdgeCell"));

        TMarchingSquaresMapTriangulateEdgeCellCS<0>::FBaseType* ComputeShader;

        if (bUseDualMesh)
        {
            ComputeShader = *TShaderMapRef<TMarchingSquaresMapTriangulateEdgeCellCS<0>>(RHIShaderMap);
        }
        else
        {
            ComputeShader = *TShaderMapRef<TMarchingSquaresMapTriangulateEdgeCellCS<1>>(RHIShaderMap);
        }

        FSamplerStateRHIParamRef HeightMapSampler = TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();

        FVector2D HeightScale;
        HeightScale.X = SurfaceHeightScale;
        HeightScale.Y = ExtrudeHeightScale;

        FIntPoint GeomCount;
        GeomCount.X = VCount;
        GeomCount.Y = ICount;

        float  HeightOffset = BaseHeightOffset;
        uint32 SampleLevel  = FMath::Max(0, HeightMapMipLevel);

        ComputeShader->SetShader(RHICmdList);
        ComputeShader->BindTexture(RHICmdList, TEXT("HeightMap"), TEXT("samplerHeightMap"), HeightMap, HeightMapSampler);
        ComputeShader->BindSRV(RHICmdList, TEXT("VoxelFeatureData"), VoxelFeatureData.SRV);
        ComputeShader->BindSRV(RHICmdList, TEXT("OffsetData"),       OffsetData.SRV);
        ComputeShader->BindSRV(RHICmdList, TEXT("SumData"),          SumData.SRV);
        ComputeShader->BindSRV(RHICmdList, TEXT("EdgeCellIdData"),   EdgeCellIdData.SRV);
        ComputeShader->BindSRV(RHICmdList, TEXT("CellCaseData"),     CellCaseData.SRV);
        ComputeShader->BindUAV(RHICmdList, TEXT("OutPositionData"), PositionData.UAV);
        ComputeShader->BindUAV(RHICmdList, TEXT("OutTangentData"),  TangentData.UAV);
        ComputeShader->BindUAV(RHICmdList, TEXT("OutTexCoordData"), TexCoordData.UAV);
        ComputeShader->BindUAV(RHICmdList, TEXT("OutColorData"),    ColorData.UAV);
        ComputeShader->BindUAV(RHICmdList, TEXT("OutIndexData"),    IndexData.UAV);
        ComputeShader->SetParameter(RHICmdList, TEXT("_GDim"),          Dimension);
        ComputeShader->SetParameter(RHICmdList, TEXT("_LDim"),          FIntPoint(BlockSize, BlockSize));
        ComputeShader->SetParameter(RHICmdList, TEXT("_GeomCount"),     GeomCount);
        ComputeShader->SetParameter(RHICmdList, TEXT("_SampleLevel"),   SampleLevel);
        ComputeShader->SetParameter(RHICmdList, TEXT("_EdgeCellCount"), EdgeCellCount);
        ComputeShader->SetParameter(RHICmdList, TEXT("_BlockOffset"),   BlockOffset);
        ComputeShader->SetParameter(RHICmdList, TEXT("_HeightScale"),   HeightScale);
        ComputeShader->SetParameter(RHICmdList, TEXT("_HeightOffset"),  HeightOffset);
        ComputeShader->SetParameter(RHICmdList, TEXT("_Color"),         FVector4(1,0,0,1));
        ComputeShader->DispatchAndClear(RHICmdList, EdgeCellCount, 1, 1);

        RHICmdList.EndComputePass();
    }

    // Construct mesh sections

    int32 GridCountX = (Dimension.X / BlockSize);
    int32 GridCountY = (Dimension.Y / BlockSize);
    int32 GridCount  = (GridCountX * GridCountY);
    int32 TotalGridCount = GridCount;

    if (bUseDualMesh)
    {
        TotalGridCount *= 2;
    }

    if (! SectionGroups.IsValidIndex(FillType))
    {
        SectionGroups.SetNum(FillType+1, false);
    }

    TArray<FPMUMeshSection>& SectionSections(SectionGroups[FillType]);

    SectionSections.Reset(TotalGridCount);
    SectionSections.SetNum(TotalGridCount);

    uint8* PositionDataPtr = reinterpret_cast<uint8*>(RHILockStructuredBuffer(PositionData.Buffer, 0, PositionData.Buffer->GetSize(), RLM_ReadOnly));
    uint8* TangentDataPtr  = reinterpret_cast<uint8*>(RHILockStructuredBuffer(TangentData.Buffer, 0, TangentData.Buffer->GetSize(), RLM_ReadOnly));
    uint8* TexCoordDataPtr = reinterpret_cast<uint8*>(RHILockVertexBuffer(TexCoordData.Buffer, 0, TexCoordData.Buffer->GetSize(), RLM_ReadOnly));
    uint8* ColorDataPtr    = reinterpret_cast<uint8*>(RHILockVertexBuffer(ColorData.Buffer, 0, ColorData.Buffer->GetSize(), RLM_ReadOnly));
    uint8* IndexDataPtr    = reinterpret_cast<uint8*>(RHILockVertexBuffer(IndexData.Buffer, 0, IndexData.Buffer->GetSize(), RLM_ReadOnly));

    const int32 PositionDataStride = sizeof(FRULAlignedVector);
    const int32 TangentDataStride  = sizeof(FRULAlignedUintPoint);
    const int32 TexCoordDataStride = sizeof(FRULAlignedVector2D);
    const int32 ColorDataStride    = sizeof(FRULAlignedUint);
    const int32 IndexDataStride    = sizeof(FIndexData::ElementType);

    for (int32 gy=0; gy<GridCountY; ++gy)
    for (int32 gx=0; gx<GridCountX; ++gx)
    {
        int32 i  = gx + gy*GridCountX;
        int32 i0 =  i    * BlockOffset;
        int32 i1 = (i+1) * BlockOffset;

        FGeomCountDataType Sum0 = SumArr[i0];
        FGeomCountDataType Sum1 = SumArr[i1];

        uint32 GVOffset = Sum0[0];
        uint32 GIOffset = Sum0[1];

        uint32 GVCount = Sum1[0] - GVOffset;
        uint32 GICount = Sum1[1] - GIOffset;

        UE_LOG(UntMSQ,Warning, TEXT("FMarchingSquaresMap::GenerateMarchingCubes_RT() [%d] Grid Geometry Count (%d, %d)"), i, i0, i1);
        UE_LOG(UntMSQ,Warning, TEXT("FMarchingSquaresMap::GenerateMarchingCubes_RT() [%d] GVOffset: %u"), i, GVOffset);
        UE_LOG(UntMSQ,Warning, TEXT("FMarchingSquaresMap::GenerateMarchingCubes_RT() [%d] GIOffset: %u"), i, GIOffset);
        UE_LOG(UntMSQ,Warning, TEXT("FMarchingSquaresMap::GenerateMarchingCubes_RT() [%d] GVCount: %u"), i, GVCount);
        UE_LOG(UntMSQ,Warning, TEXT("FMarchingSquaresMap::GenerateMarchingCubes_RT() [%d] GICount: %u"), i, GICount);

        bool bValidSection = (GVCount >= 3 && GICount >= 3);

        // Skip empty sections
        if (! bValidSection)
        {
            continue;
        }

        // Calculate local bounds

        float BoundsSurfaceZ =  BaseHeightOffset + SurfaceHeightScale + 1.f;
        float BoundsExtrudeZ = -BaseHeightOffset + ExtrudeHeightScale - 1.f;

        if (bOverrideBoundsZ)
        {
            if (BoundsSurfaceOverrideZ > KINDA_SMALL_NUMBER)
            {
                BoundsSurfaceZ =  BaseHeightOffset + BoundsSurfaceOverrideZ + 1.f;
            }

            if (BoundsExtrudeOverrideZ > KINDA_SMALL_NUMBER)
            {
                BoundsExtrudeZ = -BaseHeightOffset - BoundsExtrudeOverrideZ - 1.f;
            }
        }

        FBox LocalBounds(ForceInitToZero);
        LocalBounds.Min = FVector(gx*BlockSize, gy*BlockSize, 0);
        LocalBounds.Max = LocalBounds.Min + FVector(BlockSize, BlockSize, 0);
        LocalBounds.Min.Z = BoundsExtrudeZ;
        LocalBounds.Max.Z = BoundsSurfaceZ;
        LocalBounds = LocalBounds.ShiftBy(-FVector(Dimension.X,Dimension.Y,0)/2.f);

        // Copy surface geometry

        {
            FPMUMeshSection& Section(SectionSections[i]);

            TArray<FVector>&   SectionPositionData(Section.Positions);
            TArray<uint32>&    SectionTangentData(Section.Tangents);
            TArray<FVector2D>& SectionTexCoordData(Section.UVs);
            TArray<FColor>&    SectionColorData(Section.Colors);
            TArray<uint32>&    SectionIndexData(Section.Indices);

            SectionPositionData.SetNumUninitialized(GVCount);
            SectionTangentData.SetNumUninitialized(GVCount*2);
            SectionTexCoordData.SetNumUninitialized(GVCount);
            SectionColorData.SetNumUninitialized(GVCount);
            SectionIndexData.SetNumUninitialized(GICount);

            void* SectionPositionDataPtr = SectionPositionData.GetData();
            void* SectionTangentDataPtr = SectionTangentData.GetData();
            void* SectionTexCoordDataPtr = SectionTexCoordData.GetData();
            void* SectionColorDataPtr = SectionColorData.GetData();
            void* SectionIndexDataPtr = SectionIndexData.GetData();

            uint32 PositionByteOffset = GVOffset * PositionDataStride;
            uint32 TangentByteOffset  = GVOffset * TangentDataStride;
            uint32 TexCoordByteOffset = GVOffset * TexCoordDataStride;
            uint32 ColorByteOffset    = GVOffset * ColorDataStride;
            uint32 IndexByteOffset    = GIOffset * IndexDataStride;

            uint32 PositionByteCount = GVCount * PositionDataStride;
            uint32 TangentByteCount  = GVCount * TangentDataStride;
            uint32 TexCoordByteCount = GVCount * TexCoordDataStride;
            uint32 ColorByteCount    = GVCount * ColorDataStride;
            uint32 IndexByteCount    = GICount * IndexDataStride;

            FMemory::Memcpy(SectionPositionDataPtr, PositionDataPtr+PositionByteOffset, PositionByteCount);
            FMemory::Memcpy(SectionTangentDataPtr, TangentDataPtr+TangentByteOffset, TangentByteCount);
            FMemory::Memcpy(SectionTexCoordDataPtr, TexCoordDataPtr+TexCoordByteOffset, TexCoordByteCount);
            FMemory::Memcpy(SectionColorDataPtr, ColorDataPtr+ColorByteOffset, ColorByteCount);
            FMemory::Memcpy(SectionIndexDataPtr, IndexDataPtr+IndexByteOffset, IndexByteCount);

            Section.bInitializeInvalidVertexData = false;
            Section.bSectionVisible = bValidSection;
            Section.SectionLocalBox = LocalBounds;
        }

        // Copy extrude geometry if generating dual mesh

        if (bUseDualMesh)
        {
            FPMUMeshSection& Section(SectionSections[i+GridCount]);

            TArray<FVector>&   SectionPositionData(Section.Positions);
            TArray<uint32>&    SectionTangentData(Section.Tangents);
            TArray<FVector2D>& SectionTexCoordData(Section.UVs);
            TArray<FColor>&    SectionColorData(Section.Colors);
            TArray<uint32>&    SectionIndexData(Section.Indices);

            SectionPositionData.SetNumUninitialized(GVCount);
            SectionTangentData.SetNumUninitialized(GVCount*2);
            SectionTexCoordData.SetNumUninitialized(GVCount);
            SectionColorData.SetNumUninitialized(GVCount);
            SectionIndexData.SetNumUninitialized(GICount);

            void* SectionPositionDataPtr = SectionPositionData.GetData();
            void* SectionTangentDataPtr = SectionTangentData.GetData();
            void* SectionTexCoordDataPtr = SectionTexCoordData.GetData();
            void* SectionColorDataPtr = SectionColorData.GetData();
            void* SectionIndexDataPtr = SectionIndexData.GetData();

            uint32 PositionByteOffset = (GVOffset+VCount) * PositionDataStride;
            uint32 TangentByteOffset  = (GVOffset+VCount) * TangentDataStride;
            uint32 TexCoordByteOffset = (GVOffset+VCount) * TexCoordDataStride;
            uint32 ColorByteOffset    = (GVOffset+VCount) * ColorDataStride;
            uint32 IndexByteOffset    = (GIOffset+ICount) * IndexDataStride;

            uint32 PositionByteCount = GVCount * PositionDataStride;
            uint32 TangentByteCount  = GVCount * TangentDataStride;
            uint32 TexCoordByteCount = GVCount * TexCoordDataStride;
            uint32 ColorByteCount    = GVCount * ColorDataStride;
            uint32 IndexByteCount    = GICount * IndexDataStride;

            FMemory::Memcpy(SectionPositionDataPtr, PositionDataPtr+PositionByteOffset, PositionByteCount);
            FMemory::Memcpy(SectionTangentDataPtr, TangentDataPtr+TangentByteOffset, TangentByteCount);
            FMemory::Memcpy(SectionTexCoordDataPtr, TexCoordDataPtr+TexCoordByteOffset, TexCoordByteCount);
            FMemory::Memcpy(SectionColorDataPtr, ColorDataPtr+ColorByteOffset, ColorByteCount);
            FMemory::Memcpy(SectionIndexDataPtr, IndexDataPtr+IndexByteOffset, IndexByteCount);

            Section.bInitializeInvalidVertexData = false;
            Section.bSectionVisible = bValidSection;
            Section.SectionLocalBox = LocalBounds;
        }
    }

    RHIUnlockVertexBuffer(IndexData.Buffer);
    RHIUnlockVertexBuffer(ColorData.Buffer);
    RHIUnlockVertexBuffer(TexCoordData.Buffer);
    RHIUnlockStructuredBuffer(TangentData.Buffer);
    RHIUnlockStructuredBuffer(PositionData.Buffer);
}

//bool FMarchingSquaresMap::IsPrefabValid(int32 PrefabIndex, int32 LODIndex, int32 SectionIndex) const
//{
//    //if (! HasPrefab(PrefabIndex))
//    //{
//    //    return false;
//    //}
//
//    //const UStaticMesh* Mesh = MeshPrefabs[PrefabIndex];
//
//    //if (Mesh->bAllowCPUAccess &&
//    //    Mesh->RenderData      &&
//    //    Mesh->RenderData->LODResources.IsValidIndex(LODIndex) && 
//    //    Mesh->RenderData->LODResources[LODIndex].Sections.IsValidIndex(SectionIndex)
//    //    )
//    //{
//    //    return true;
//    //}
//
//    return false;
//}

//bool FMarchingSquaresMap::HasIntersectingBounds(const TArray<FBox2D>& Bounds) const
//{
//    //if (Bounds.Num() > 0)
//    //{
//    //    for (const FPrefabData& PrefabData : AppliedPrefabs)
//    //    {
//    //        const TArray<FBox2D>& AppliedBounds(PrefabData.Bounds);
//
//    //        for (const FBox2D& Box0 : Bounds)
//    //        for (const FBox2D& Box1 : AppliedBounds)
//    //        {
//    //            if (Box0.Intersect(Box1))
//    //            {
//    //                // Skip exactly intersecting box
//
//    //                if (FMath::IsNearlyEqual(Box0.Min.X, Box1.Max.X, 1.e-3f) ||
//    //                    FMath::IsNearlyEqual(Box1.Min.X, Box0.Max.X, 1.e-3f))
//    //                {
//    //                    continue;
//    //                }
//
//    //                if (FMath::IsNearlyEqual(Box0.Min.Y, Box1.Max.Y, 1.e-3f) ||
//    //                    FMath::IsNearlyEqual(Box1.Min.Y, Box0.Max.Y, 1.e-3f))
//    //                {
//    //                    continue;
//    //                }
//
//    //                return true;
//    //            }
//    //        }
//    //    }
//    //}
//
//    return false;
//}

//bool FMarchingSquaresMap::TryPlacePrefabAt(int32 PrefabIndex, const FVector2D& Center)
//{
//    TArray<FBox2D> Bounds;
//    GetPrefabBounds(PrefabIndex, Bounds);
//
//    // Offset prefab bounds towards the specified center location
//    for (FBox2D& Box : Bounds)
//    {
//        Box = Box.ShiftBy(Center);
//    }
//
//    if (! HasIntersectingBounds(Bounds))
//    {
//        AppliedPrefabs.Emplace(Bounds);
//        return true;
//    }
//
//    return false;
//}

//void FMarchingSquaresMap::GetPrefabBounds(int32 PrefabIndex, TArray<FBox2D>& Bounds) const
//{
//    Bounds.Empty();
//
//    if (! HasPrefab(PrefabIndex))
//    {
//        return;
//    }
//
//    const UStaticMesh& Mesh(*MeshPrefabs[PrefabIndex]);
//    const TArray<UStaticMeshSocket*>& Sockets(Mesh.Sockets);
//
//    typedef TKeyValuePair<UStaticMeshSocket*, UStaticMeshSocket*> FBoundsPair;
//    TArray<FBoundsPair> BoundSockets;
//
//    const FString MIN_PREFIX(TEXT("Bounds_MIN_"));
//    const FString MAX_PREFIX(TEXT("Bounds_MAX_"));
//    const int32 PREFIX_LEN = MIN_PREFIX.Len();
//
//    for (UStaticMeshSocket* Socket0 : Sockets)
//    {
//        // Invalid socket, skip
//        if (! IsValid(Socket0))
//        {
//            continue;
//        }
//
//        FString MinSocketName(Socket0->SocketName.ToString());
//        FString MaxSocketName(MAX_PREFIX);
//
//        // Not a min bounds socket, skip
//        if (! MinSocketName.StartsWith(*MIN_PREFIX, ESearchCase::IgnoreCase))
//        {
//            continue;
//        }
//
//        MaxSocketName += MinSocketName.RightChop(PREFIX_LEN);
//
//        for (UStaticMeshSocket* Socket1 : Sockets)
//        {
//            if (IsValid(Socket1) && Socket1->SocketName.ToString() == MaxSocketName)
//            {
//                BoundSockets.Emplace(Socket0, Socket1);
//                break;
//            }
//        }
//    }
//
//    for (FBoundsPair BoundsPair : BoundSockets)
//    {
//        FBox2D Box;
//        const FVector& Min(BoundsPair.Key->RelativeLocation);
//        const FVector& Max(BoundsPair.Value->RelativeLocation);
//        const float MinX = FMath::RoundHalfFromZero(Min.X);
//        const float MinY = FMath::RoundHalfFromZero(Min.Y);
//        const float MaxX = FMath::RoundHalfFromZero(Max.X);
//        const float MaxY = FMath::RoundHalfFromZero(Max.Y);
//        Box += FVector2D(MinX, MinY);
//        Box += FVector2D(MaxX, MaxY);
//        Bounds.Emplace(Box);
//    }
//}

//TArray<FBox2D> FMarchingSquaresMap::GetPrefabBounds(int32 PrefabIndex) const
//{
//    TArray<FBox2D> Bounds;
//    GetPrefabBounds(PrefabIndex, Bounds);
//    return Bounds;
//}

//bool FMarchingSquaresMap::ApplyPrefab(
//    int32 PrefabIndex,
//    int32 SectionIndex,
//    FVector Center,
//    bool bApplyHeightMap,
//    bool bInverseHeightMap,
//    FPMUMeshSection& OutSection
//    )
//{
//    int32 LODIndex = 0;
//
//    // Check prefab validity
//    if (! IsPrefabValid(PrefabIndex, LODIndex, SectionIndex))
//    {
//        return false;
//    }
//
//    UStaticMesh* InMesh = MeshPrefabs[PrefabIndex];
//    const FStaticMeshLODResources& LOD(InMesh->RenderData->LODResources[LODIndex]);
//
//    // Place prefab only if the prefab would not intersect with applied prefabs
//    if (! TryPlacePrefabAt(PrefabIndex, FVector2D(Center)))
//    {
//        return false;
//    }
//
//    bool bApplySuccess = false;
//
//    // Map from vert buffer for whole mesh to vert buffer for section of interest
//    TMap<int32, int32> MeshToSectionVertMap;
//    TSet<int32> BorderVertSet;
//
//    const FStaticMeshSection& Section = LOD.Sections[SectionIndex];
//    const uint32 OnePastLastIndex = Section.FirstIndex + Section.NumTriangles * 3;
//
//    // Empty output buffers
//    FIndexArrayView Indices = LOD.IndexBuffer.GetArrayView();
//    const int32 VertexCountOffset = OutSection.GetVertexCount();
//
//    // Iterate over section index buffer, copying verts as needed
//    for (uint32 i = Section.FirstIndex; i < OnePastLastIndex; i++)
//    {
//        uint32 MeshVertIndex = Indices[i];
//
//        // See if we have this vert already in our section vert buffer,
//        // copy vert in if not 
//        int32 SectionVertIndex = GetNewIndexForOldVertIndex(
//            MeshVertIndex,
//            MeshToSectionVertMap,
//            BorderVertSet,
//            Center,
//            ! bApplyHeightMap,
//            &LOD.PositionVertexBuffer,
//            &LOD.VertexBuffer,
//            &LOD.ColorVertexBuffer,
//            OutSection
//            );
//
//        // Add to index buffer
//        OutSection.IndexBuffer.Emplace(SectionVertIndex);
//    }
//
//    // Mark success
//    bApplySuccess = true;
//
//    if (! bApplyHeightMap || ! GridData)
//    {
//        return bApplySuccess;
//    }
//
//    const FPMUGridData* GD = GridData;
//    int32 ShapeHeightMapId   = -1;
//    int32 SurfaceHeightMapId = -1;
//    int32 ExtrudeHeightMapId = -1;
//    uint8 HeightMapType = 0;
//
//    if (GD)
//    {
//        ShapeHeightMapId   = GridData->GetNamedHeightMapId(TEXT("PMU_VOXEL_SHAPE_HEIGHT_MAP"));
//        SurfaceHeightMapId = GridData->GetNamedHeightMapId(TEXT("PMU_VOXEL_SURFACE_HEIGHT_MAP"));
//        ExtrudeHeightMapId = GridData->GetNamedHeightMapId(TEXT("PMU_VOXEL_EXTRUDE_HEIGHT_MAP"));
//        const bool bHasShapeHeightMap   = ShapeHeightMapId   >= 0;
//        const bool bHasSurfaceHeightMap = SurfaceHeightMapId >= 0;
//        const bool bHasExtrudeHeightMap = ExtrudeHeightMapId >= 0;
//        HeightMapType = (bHasShapeHeightMap ? 1 : 0) | ((bHasSurfaceHeightMap ? 1 : 0) << 1) | ((bHasExtrudeHeightMap ? 1 : 0) << 2);
//    }
//
//    float voxelSize     = 1.0f;
//    float voxelSizeInv  = 1.0f;
//    float voxelSizeHalf = voxelSize / 2.f;
//    const int32 VertexCount = OutSection.GetVertexCount();
//
//    const FIntPoint& Dim(GridData->Dimension);
//    FVector2D GridRangeMin = FVector2D(1.f, 1.f);
//    FVector2D GridRangeMax = FVector2D(Dim.X-2, Dim.Y-2);
//
//    for (int32 i=VertexCountOffset; i<VertexCount; ++i)
//    {
//        FVector& Position(OutSection.VertexBuffer[i].Position);
//        FVector& VNormal(OutSection.VertexBuffer[i].Normal);
//
//        const float PX = Position.X + voxelSizeHalf;
//        const float PY = Position.Y + voxelSizeHalf;
//        const float GridX = FMath::Clamp(PX, GridRangeMin.X, GridRangeMax.X);
//        const float GridY = FMath::Clamp(PY, GridRangeMin.Y, GridRangeMax.Y);
//        const int32 IX = GridX;
//        const int32 IY = GridY;
//
//        const float Z = Position.Z;
//        float Height = Z;
//        FVector Normal(0.f, 0.f, bInverseHeightMap ? -1.f : 1.f);
//
//        if (HeightMapType != 0)
//        {
//            check(GD != nullptr);
//
//            if (bInverseHeightMap)
//            {
//                switch (HeightMapType & 5)
//                {
//                    case 1:
//                        UPMUGridHeightMapUtility::GetHeightNormal(GridX, GridY, IX, IY, *GD, ShapeHeightMapId, Height, Normal);
//                        break;
//
//                    case 4:
//                    case 5:
//                        UPMUGridHeightMapUtility::GetHeightNormal(GridX, GridY, IX, IY, *GD, ExtrudeHeightMapId, Height, Normal);
//                        break;
//                }
//
//                Height += Z;
//                Normal = -Normal;
//            }
//            else
//            {
//                switch (HeightMapType & 3)
//                {
//                    case 1:
//                        UPMUGridHeightMapUtility::GetHeightNormal(GridX, GridY, IX, IY, *GD, ShapeHeightMapId, Height, Normal);
//                        break;
//
//                    case 2:
//                    case 3:
//                        UPMUGridHeightMapUtility::GetHeightNormal(GridX, GridY, IX, IY, *GD, SurfaceHeightMapId, Height, Normal);
//                        break;
//                }
//            }
//        }
//
//        Position.Z += Height;
//        VNormal = BorderVertSet.Contains(i)
//            ? Normal
//            : (VNormal+Normal).GetSafeNormal();
//
//        OutSection.LocalBox += Position;
//    }
//
//    return bApplySuccess;
//}
