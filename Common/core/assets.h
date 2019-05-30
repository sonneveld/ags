
#ifndef AGS_COMMON_CORE_ASSETS_H_
#define AGS_COMMON_CORE_ASSETS_H_

#include <memory>
#include <vector>
#include <utility>
#include <map>

#include "util/string.h"
#include "util/file.h" // TODO: extract filestream mode constants or introduce generic ones

// main path
// ResPaths.GamePak.Path


// TODO:
// buffer caching should be done in another thread safe layer, that can be shared amongst asset libs.
// need to be able to flush this buffer at start of rooms.
// maybe a "get asset info" and implement "get all assets"
// gruop buffered objects by type.. 

// 1 game data (keep all) (8 meg)
// 20 fonts (keep all) (4meg)
// 1 sync data (keep all) (0?)

// 125 rooms (keep 16?) (70meg) -> 10meg
// 71 music (keep 16? 1 per room?) (164 meg)->40meg?

// 1200 general sounds (70 meg) -> 10 meg?
// 12,000 speech (500meg)  -> 10meg???
// 17,000 sprites  (4 gig) -> 1900 left


/*


    class Asset
    {
        filename
        full path?
        compressed?
        size?
        const data vector
    }

    constructor( list of sources(dir, lib, lib) )

        add directory
        add libfile <-- not needed
            cast to lowercase
            - check if compressed or not
    need to be able to list files in a lib
    asset exist?
    get asset bufer or null
    get asset stream or null




sprite loader
init(assetlibrary)
load sprite
load bundle(view)
load bundle(char)
...



*/


typedef std::pair<AGS::Common::String, AGS::Common::String> AssetPath;

namespace AGS {
namespace Common {

class Stream;

// Information on single asset
struct AssetInfo
{
    // A pair of filename and libuid is assumed to be unique in game scope
    String      FileName;   // filename associated with asset
    int32_t     LibUid;     // uid of library, containing this asset
    soff_t      Offset;     // asset's position in library file (in bytes)
    soff_t      Size;       // asset's size (in bytes)

    AssetInfo();
};

typedef std::vector<AssetInfo> AssetVec;

// Information on multifile asset library
struct AssetLibInfo
{
    String BaseFileName;               // library's base (head) filename
    String BaseFilePath;               // full path to the base filename
    std::vector<String> LibFileNames;  // filename for each library part

    // Library contents
    AssetVec AssetInfos; // information on contained assets

    void Unload();
};

enum AssetSearchPriority
{
    // TODO: rename this to something more obvious
    kAssetPriorityUndefined,
    kAssetPriorityLib,
    kAssetPriorityDir
};

enum AssetError
{
    kAssetNoError           =  0,
    kAssetErrNoLibFile      = -1, // library file not found or can't be read
    kAssetErrLibParse       = -2, // bad library file format or read error
    kAssetErrNoManager      = -6, // asset manager not initialized
};

// Explicit location of asset data
struct AssetLocation
{
    String      FileName;   // file where asset is located
    soff_t      Offset;     // asset's position in file (in bytes)
    soff_t      Size;       // asset's size (in bytes)

    AssetLocation();
};

struct AssetManagerEntry final {
    AGS::Common::String name;
    AGS::Common::String path;
    bool compressed;
    bool inBundle;
};


class AssetManager final
{
public:

    using AssetPtr = std::shared_ptr<std::vector<uint8_t>>;

    void IncludeDirectory(const AGS::Common::String &path);


    void register_asset_for_bundle(const AGS::Common::String &asset_name, const AGS::Common::String &bundle_name);

    void Setup(const AGS::Common::String &path);

    const AssetLibInfo *GetLibraryTOC();

    bool         DoesAssetExist(const String &asset_name);

    // return stream or null if error
    Stream       *OpenAsset(const String &asset_name,
                                   FileOpenMode open_mode = kFile_Open,
                                   FileWorkMode work_mode = kFile_Read);

    AssetPtr LoadAsset(const String &asset_name);


    void LoadBundle(const String &bundle_name);



private:

    std::map<AGS::Common::String, AssetManagerEntry> entryByName_;
    std::map<AGS::Common::String, AssetPtr> loadedAssets_;



    std::map<AGS::Common::String, AGS::Common::String> bundleByAssetName_;

};


} // namespace Common
} // namespace AGS


extern std::unique_ptr<AGS::Common::AssetManager> gameAssetLibrary;

#endif  // AGS_COMMON_CORE_ASSETS_H_