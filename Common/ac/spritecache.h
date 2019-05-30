//=============================================================================
//
// Adventure Game Studio (AGS)
//
// Copyright (C) 1999-2011 Chris Jones and 2011-20xx others
// The full list of copyright holders can be found in the Copyright.txt
// file, which is part of this source code distribution.
//
// The AGS source code is provided under the Artistic License 2.0.
// A copy of this license can be found in the file License.txt and at
// http://www.opensource.org/licenses/artistic-license-2.0.php
//
//=============================================================================
//
// Sprite caching system.
//
// TODO: split out sprite serialization into something like "SpriteFile" class.
// SpriteCache should only manage caching and provide bitmaps by demand.
// There should be a separate unit which manages streaming and decompressing.
// Perhaps an interface which would allow multiple implementation depending
// on compression type.
//
// TODO: store sprite data in a specialized container type that is optimized
// for having most keys allocated in large continious sequences by default.
//
// Only for the reference: one of the ideas is for container to have a table
// of arrays of fixed size internally. When getting an item the hash would be
// first divided on array size to find the array the item resides in, then the
// item is taken from item from slot index = (hash - arrsize * arrindex).
// TODO: find out if there is already a hash table kind that follows similar
// principle.
//
//=============================================================================

#ifndef __SPRCACHE_H
#define __SPRCACHE_H

#include <memory>
#include <vector>
#include <list>
#include <map>
#include "core/platform.h"
#include "util/error.h"
#include "util/asset_loader.h"

namespace AGS { namespace Common { class Stream; class Bitmap; } }
using namespace AGS; // FIXME later
typedef AGS::Common::HError HAGSError;

struct SpriteInfo;

// Max size of the sprite cache, in number of entries
#define DEFAULTCACHESIZE 8192


typedef int32_t sprkey_t;

class SpriteCache
{
public:
    static const sprkey_t MIN_SPRITE_INDEX = 1; // 0 is reserved for "empty sprite"
    static const sprkey_t MAX_SPRITE_INDEX = INT32_MAX - 1;
    static const size_t   MAX_SPRITE_SLOTS = INT32_MAX;

    SpriteCache(std::vector<SpriteInfo> &sprInfos);
    ~SpriteCache();

    // Tells if there is a sprite registered for the given index
    bool        DoesSpriteExist(sprkey_t index) const;
    // Tells if sprite was added externally, not loaded from game resources
    bool        IsExternalSprite(sprkey_t index) const;
    // Makes sure sprite cache has registered slots for all sprites up to the given exclusive limit
    sprkey_t    EnlargeTo(sprkey_t newsize);
    // Finds a free slot index, if all slots are occupied enlarges sprite bank; returns index
    sprkey_t    AddNewSprite();
    // Returns current size of the cache, in bytes
    size_t      GetCacheSize() const;
    // Gets the total size of the locked sprites, in bytes
    size_t      GetLockedSize() const;
    // Returns maximal size limit of the cache, in bytes
    size_t      GetMaxCacheSize() const;
    // Returns number of sprite slots in the bank (this includes both actual sprites and free slots)
    sprkey_t    GetSpriteSlotCount() const;
    // Finds the topmost occupied slot index. Warning: may be slow.
    sprkey_t    FindTopmostSprite() const;
    // Loads sprite and and locks in memory (so it cannot get removed implicitly)
    void        Precache(sprkey_t index);
    // Unregisters sprite from the bank and optionally deletes bitmap
    void        RemoveSprite(sprkey_t index, bool freeMemory);
    // Removes all loaded images from the cache
    void        RemoveAll();
    // Deletes all data and resets cache to the clear state
    void        Reset();
    // Assigns new bitmap for the given sprite index
    void        Set(sprkey_t index, Common::Bitmap *);
    // Sets max cache size in bytes
    void        SetMaxCacheSize(size_t size);

    void RemoveLeastRecentlyUsed();

    // Loads sprite reference information and inits sprite stream
    HAGSError   InitFile(const char *filename);

    // Loads (if it's not in cache yet) and returns bitmap by the sprite index
    Common::Bitmap *operator[] (sprkey_t index);


    void PreloadSprite(sprkey_t index, int priority);

    bool IsLoaded(sprkey_t index);


private:
    void LoadSprite(sprkey_t index);
    void LoadSprite(AGS::Common::Stream *stream);
    void        Init();
    void        RemoveOldest();


    struct loading_entry {
        int priority;
        AGS::Common::AssetLoader::future_result_t f;
    };

    std::map<sprkey_t, loading_entry> futureByKey;


    // Information required for the sprite streaming
    // TODO: split into sprite cache and sprite stream data
    struct SpriteData
    {

        // Tells that the sprite does not exist in the game resources.
        bool DoesNotExist = false;
        // Locked sprites are ones that should not be freed when clearing cache space.
        bool Locked = false;
        // Tells that the sprite index was remapped to another existing sprite.
        bool Remapped = false;
        sprkey_t RemappedTo = 0;

        // TODO: investigate if we may safely use unique_ptr here
        // (some of these bitmaps may be assigned from outside of the cache)
        Common::Bitmap *Image = nullptr; // actual bitmap

        // used to add/remove
        bool in_lru = false;
        std::list<sprkey_t>::iterator lru_it;
    };

    // Provided map of sprite infos, to fill in loaded sprite properties
    std::vector<SpriteInfo> &_sprInfos;
    // Array of sprite references
    std::vector<SpriteData> _spriteData;

    std::list<sprkey_t> lru_list;

    size_t _maxCacheSize;  // cache size limit

    // implemented in engine...
    void initFile_initNullSpriteParams(sprkey_t index);
};

extern SpriteCache spriteset;

#endif // __SPRCACHE_H
