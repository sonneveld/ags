/*
!!TODO
- replace std::runtime_error with something asset related

*/

#include "core/assets.h"

#include "util/stdio_compat.h"
#include "util/file.h"
#include "util/stream.h"

#include "debug/debugmanager.h"

std::unique_ptr<AGS::Common::AssetManager> gameAssetLibrary = nullptr;


namespace AGS {
namespace Common {

    const AssetLibInfo *AssetManager::GetLibraryTOC() {
        throw std::runtime_error("not implemented");
    }

    bool AssetManager::DoesAssetExist(const String &asset_name) {
        auto normalisedAssetName = asset_name.Lower();
        //auto normalisedAssetName = asset_name;
        //return entryByName_.find(normalisedAssetName) != entryByName_.end();
        return PHYSFS_exists(normalisedAssetName.GetCStr());
    }

    Stream *AssetManager::OpenAsset(const String &asset_name,
        FileOpenMode open_mode ,
        FileWorkMode work_mode ) 
    {

        auto normalisedAssetName = asset_name.Lower();
        //auto normalisedAssetName = asset_name;

        try {
            auto ps = std::make_unique<PhysfsStream>(asset_name);
            auto bs = std::make_unique<BufferedStream>(std::move(ps));
            return new DataStream(std::move(bs));
        } catch (std::runtime_error) {
            return nullptr;
        }
    }

    std::shared_ptr<std::vector<uint8_t>> AssetManager::LoadAsset(const String &asset_name)
    {
        auto normalisedAssetName = asset_name.Lower();
        auto stream = OpenAsset(normalisedAssetName);
        if (stream == nullptr) {
            return std::shared_ptr<std::vector<uint8_t>>(nullptr);
        }

        auto buf = std::make_shared<std::vector<uint8_t>>(16*1024*1024);
        auto sz = stream->Read(buf->data(), buf->size());
        buf->resize(sz);

        delete stream;

        return buf;
    }

}
}