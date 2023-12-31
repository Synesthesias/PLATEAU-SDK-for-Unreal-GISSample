// Copyright © 2023 Ministry of Land, Infrastructure and Transport


#include "PLATEAUBasemap.h"
#include "PLATEAUGeometry.h"
#include "PLATEAUTextureLoader.h"
#include "ExtentEditor/PLATEAUExtentEditorVPClient.h"
#include "Materials/MaterialInstanceDynamic.h"

#include <plateau/basemap/tile_projection.h>
#include <plateau/basemap/vector_tile_downloader.h>

#include <Async/Async.h>

namespace {
    /**
     * @brief コンポーネント追加間隔
     */
    constexpr float AddBaseMapComponentSleepTime = 0.03f;
}

FPLATEAUBasemap::FPLATEAUBasemap(
    const FPLATEAUGeoReference& InGeoReference,
    const TSharedPtr<FPLATEAUExtentEditorViewportClient> InViewportClient)
    : DeltaTime(0)
    , GeoReference(InGeoReference)
    , ViewportClient(InViewportClient)
    , VectorTilePipe{ TEXT("VectorTilePipe") } {}

FPLATEAUBasemap::~FPLATEAUBasemap() {
    if (VectorTilePipe.HasWork())
        VectorTilePipe.WaitUntilEmpty();
}

void FPLATEAUBasemap::UpdateAsync(const FPLATEAUExtent& InExtent, float DeltaSeconds) {
    int ZoomLevel = 18;
    std::shared_ptr<std::vector<TileCoordinate>> TileCoordinates;

    // 画面内のタイル数が一定数よりも低くなるようにズームレベルを設定
    while (true) {
        TileCoordinates = TileProjection::getTileCoordinates(InExtent.GetNativeData(), ZoomLevel);
        if (TileCoordinates->size() <= 16 || ZoomLevel == 1)
            break;
        --ZoomLevel;
    }

    for (auto& Entry : AsyncLoadedTiles) {
        const auto TileComponent = Entry.Value->GetComponent();
        if (TileComponent == nullptr)
            continue;
        if (Entry.Value->GetLoadPhase() != EVectorTileLoadingPhase::FullyLoaded)
            continue;

        // ここで一旦読み込み済みのタイルを非表示にする
        // 後述のSetVisibilityにより表示するべきタイル（TileCoordinates）を表示する
        Entry.Value->SetVisibility(false);

        if (TilesInScene.Contains(TileComponent))
            continue;
        
        DeltaTime = DeltaSeconds + DeltaTime;
        if (DeltaTime < AddBaseMapComponentSleepTime)
            break;
        if (!ViewportClient.IsValid())
            break;
        const auto PreviewScene = ViewportClient.Pin()->GetPreviewScene();
        if (PreviewScene == nullptr)
            break;

        DeltaTime = 0;

        //タイルの座標を取得
        auto TileExtent = TileProjection::unproject(Entry.Key.ToNativeData());
        const auto RawTileMax = GeoReference.GetData().project(TileExtent.max);
        const auto RawTileMin = GeoReference.GetData().project(TileExtent.min);
        FBox Box(FVector(RawTileMin.x, RawTileMin.y, RawTileMin.z), FVector(RawTileMax.x, RawTileMax.y, RawTileMax.z));
        auto Extent = Box.GetExtent();
        Extent.X = FMath::Abs(Extent.X);
        Extent.Y = FMath::Abs(Extent.Y);
        Extent.Z = 0.01;

        TileComponent->SetTranslucentSortPriority(plateau::dataset::SortPriority_BaseMap);
        PreviewScene->AddComponent(TileComponent, FTransform(FRotator(0, 0, 0), Box.GetCenter() - FVector::UpVector, Extent / 50.0));
        TilesInScene.Add(TileComponent);
    }

    for (const auto& RawTileCoordinate : *TileCoordinates) {
        auto TileCoordinate = FPLATEAUTileCoordinate::FromNativeData(RawTileCoordinate);
        if (!AsyncLoadedTiles.Find(TileCoordinate)) {
            const auto& AsyncLoadedTile = AsyncLoadedTiles.Add(TileCoordinate, MakeShared<FPLATEAUAsyncLoadedVectorTile>());
            AsyncLoadedTile->StartLoading(TileCoordinate, VectorTilePipe);
            continue;
        }

        const auto& AsyncLoadedTile = AsyncLoadedTiles[TileCoordinate];
        if (AsyncLoadedTile->GetLoadPhase() == EVectorTileLoadingPhase::FullyLoaded) {
            AsyncLoadedTile->SetVisibility(true);
        }
    }
}

FPLATEAUTileCoordinate FPLATEAUTileCoordinate::FromNativeData(const TileCoordinate& Data) {
    FPLATEAUTileCoordinate Result{};
    Result.Column = Data.column;
    Result.Row = Data.row;
    Result.ZoomLevel = Data.zoom_level;
    return Result;
}

TileCoordinate FPLATEAUTileCoordinate::ToNativeData() const {
    TileCoordinate Result;
    Result.column = Column;
    Result.row = Row;
    Result.zoom_level = ZoomLevel;
    return Result;
}

bool FPLATEAUTileCoordinate::operator==(const FPLATEAUTileCoordinate& Other) const {
    return Column == Other.Column
        && Row == Other.Row
        && ZoomLevel == Other.ZoomLevel;
}

bool FPLATEAUTileCoordinate::operator!=(const FPLATEAUTileCoordinate& Other) const {
    return !(*this == Other);
}

uint32 GetTypeHash(const FPLATEAUTileCoordinate& Value) {
    return Value.ZoomLevel * 100000000 + Value.Row * 10000 + Value.Column;
}

void FPLATEAUAsyncLoadedVectorTile::StartLoading(const FPLATEAUTileCoordinate& InTileCoordinate, FPipe& VectorTilePipe) {
    LoadPhase = EVectorTileLoadingPhase::Loading;
    Task = VectorTilePipe.Launch(TEXT("VectorTileTask"),
        [this, InTileCoordinate]() {

            const auto Destination = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() + TEXT("\\PLATEAU\\Basemap"));
            const FString TexturePath = UTF8_TO_TCHAR(VectorTileDownloader::calcDestinationPath(InTileCoordinate.ToNativeData(), TCHAR_TO_UTF8(*Destination), ".png").u8string().c_str());
            IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

            //テクスチャが存在する場合
            if (PlatformFile.FileExists(*TexturePath)) {
                //画像サイス0の場合
                if (PlatformFile.FileSize(*TexturePath) <= 0) {
                    UE_LOG(LogTemp, Error, TEXT("File size 0 : %s"), *TexturePath);
                    PlatformFile.DeleteFile(*TexturePath);
                    LoadPhase = EVectorTileLoadingPhase::Failed;
                    return;
                }
            }
            else {
                //画像ダウンロード
                const auto Tile = VectorTileDownloader::download(
                    VectorTileDownloader::getDefaultUrl(),
                    TCHAR_TO_UTF8(*Destination),
                    InTileCoordinate.ToNativeData());

                //読込エラー
                if (Tile->result != HttpResult::Success) {
                    UE_LOG(LogTemp, Error, TEXT("Image Load Error! %d : %s"), Tile->result, *TexturePath);
                    LoadPhase = EVectorTileLoadingPhase::Failed;
                    return;
                }
            }

            // テクスチャ読み込み
            const auto Texture = FPLATEAUTextureLoader::LoadTransient(*TexturePath);
            if (Texture == nullptr) {
                UE_LOG(LogTemp, Error, TEXT("Texture Load Error : %s"), *TexturePath);
                LoadPhase = EVectorTileLoadingPhase::Failed;
                return;
            }

            const auto& TempComponent = CreateTileComponentInGameThread(Texture);
            {
                FScopeLock Lock(&CriticalSection);
                TileComponent = TempComponent;
                LoadPhase = EVectorTileLoadingPhase::FullyLoaded;
            }

            check(LoadPhase != EVectorTileLoadingPhase::Loading);
        });
}

UStaticMeshComponent* FPLATEAUAsyncLoadedVectorTile::CreateTileComponentInGameThread(UTexture* Texture) {
    UStaticMeshComponent* NewTileComponent;
    FFunctionGraphTask::CreateAndDispatchWhenReady([&] {
        //mesh component作成，テクスチャを適用
        const FName MeshName = MakeUniqueObjectName(GetTransientPackage(), UStaticMeshComponent::StaticClass(), TEXT("Tile"));
        NewTileComponent = NewObject<UStaticMeshComponent>(GetTransientPackage(), MeshName, RF_Transient);
        const auto Mat = Cast<UMaterial>(StaticLoadObject(UMaterial::StaticClass(), nullptr, TEXT("/PLATEAU-SDK-for-Unreal/FeatureInfoPanel_PanelIcon")));
        const auto DynMat = UMaterialInstanceDynamic::Create(Mat, GetTransientPackage());
        DynMat->SetTextureParameterValue(TEXT("Texture"), Texture);
        TileMaterialInstanceDynamicsMap.Emplace(NewTileComponent, DynMat);
        NewTileComponent->SetMaterial(0, DynMat);
        const auto StaticMeshName = TEXT("/Engine/BasicShapes/Plane");
        const auto Mesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, StaticMeshName));
        Mesh->AddMaterial(DynMat);
        NewTileComponent->SetStaticMesh(Mesh);
    }, TStatId(), nullptr, ENamedThreads::GameThread)->Wait();

    return NewTileComponent;
}

void FPLATEAUAsyncLoadedVectorTile::SetVisibility(const bool InbVisibility) {
    if (bVisibility == InbVisibility)
        return;

    bVisibility = InbVisibility;
    ApplyVisibility();
}

void FPLATEAUAsyncLoadedVectorTile::ApplyVisibility() const {
    if (!TileMaterialInstanceDynamicsMap.Contains(TileComponent))
        return;
    
    const auto& MaterialInstanceDynamic = TileMaterialInstanceDynamicsMap[TileComponent];
    if (MaterialInstanceDynamic == nullptr)
        return;
    MaterialInstanceDynamic->SetScalarParameterValue(FName("Opacity"), bVisibility ? 1.0 : 0);
}
