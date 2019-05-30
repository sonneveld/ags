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
// sprite caching system
//
//=============================================================================

#ifdef _MANAGED
// ensure this doesn't get compiled to .NET IL
#pragma unmanaged
#pragma warning (disable: 4996 4312)  // disable deprecation warnings
#endif

#include "ac/spritecache.h"

#include <array>

#include "ac/common.h" // quit
#include "ac/gamestructdefines.h"
#include "core/assets.h"
#include "debug/out.h"
#include "gfx/bitmap.h"
#include "util/compress.h"
#include "util/file.h"
#include "util/stream.h"

using namespace AGS::Common;

// [IKM] We have to forward-declare these because their implementations are in the Engine
extern void initialize_sprite(int);
extern void pre_save_sprite(int);
extern void get_new_size_for_sprite(int, int, int, int &, int &);

#define START_OF_LIST -1
#define END_OF_LIST   -1

const char *spindexid = "SPRINDEX";
const char *spindexfilename = "index.dat";


SpriteInfo::SpriteInfo()
    : Flags(0)
    , Width(0)
    , Height(0)
{
}

SpriteCache::SpriteCache(std::vector<SpriteInfo> &sprInfos)
    : _sprInfos(sprInfos)
{
    Init();
}

SpriteCache::~SpriteCache()
{
    Reset();
}

size_t SpriteCache::GetMaxCacheSize() const
{
    return _maxCacheSize;
}

sprkey_t SpriteCache::GetSpriteSlotCount() const
{
    return _spriteData.size();
}

sprkey_t SpriteCache::FindTopmostSprite() const
{
    sprkey_t topmost = -1;
    for (sprkey_t i = 0; i < static_cast<sprkey_t>(_spriteData.size()); ++i)
        if (DoesSpriteExist(i))
            topmost = i;
    return topmost;
}

void SpriteCache::SetMaxCacheSize(size_t size)
{
    _maxCacheSize = size;
}

void SpriteCache::Init()
{
    _maxCacheSize = DEFAULTCACHESIZE;
}

void SpriteCache::Reset()
{
    // TODO: find out if it's safe to simply always delete _spriteData.Image with array element
    for (size_t i = 0; i < _spriteData.size(); ++i)
    {
        if (_spriteData[i].Image)
        {
            delete _spriteData[i].Image;
        }
        _spriteData[i].Image = nullptr;
        _spriteData[i].in_lru = false;
    }
    _spriteData.clear();

    lru_list.clear();

    Init();
}

// used by dynamic sprites
void SpriteCache::Set(sprkey_t index, Bitmap *sprite)
{
    EnlargeTo(index + 1);
    _spriteData[index].Image = sprite;
}

// sadly used all over
sprkey_t SpriteCache::EnlargeTo(sprkey_t newsize)
{
    if (newsize < 0 || (size_t)newsize <= _spriteData.size())
        return 0;
    if (newsize > MAX_SPRITE_INDEX + 1)
        newsize = MAX_SPRITE_INDEX + 1;

    sprkey_t elementsWas = (sprkey_t)_spriteData.size();
    _sprInfos.resize(newsize);
    _spriteData.resize(newsize);
    return elementsWas;
}

// dynamic sprite and screenshots
sprkey_t SpriteCache::AddNewSprite()
{
    if (_spriteData.size() == MAX_SPRITE_SLOTS)
        return -1; // no more sprite allowed
    for (size_t i = MIN_SPRITE_INDEX; i < _spriteData.size(); ++i)
    {
        if (_spriteData[i].Image != nullptr) { continue; }
        if (!_spriteData[i].DoesNotExist) { continue; }

        // slot empty
        _sprInfos[i] = SpriteInfo();
        _spriteData[i] = SpriteData();
        return i;
    }
    // enlarge the sprite bank to find a free slot and return the first new free slot
    return EnlargeTo(_spriteData.size() + 1); // we use +1 here to let std container decide the actual reserve size
}

bool SpriteCache::DoesSpriteExist(sprkey_t index) const
{
    return (_spriteData[index].Image != nullptr) || // HAS loaded bitmap
        !_spriteData[index].DoesNotExist || // OR found in the game resources
        _spriteData[index].Remapped; // OR was remapped to another sprite
}

bool SpriteCache::IsExternalSprite(sprkey_t index) const
{
    return (_spriteData[index].Image != nullptr) &&  // HAS loaded bitmap
        _spriteData[index].DoesNotExist && // AND NOT found in game resources
        !_spriteData[index].Remapped; // AND was NOT remapped to another sprite
}


void SpriteCache::RemoveLeastRecentlyUsed()
{
    auto it = lru_list.end();
    it--;

    if (_spriteData[*it].Image != nullptr) {
        delete _spriteData[*it].Image;
    }
    _spriteData[*it].Image = nullptr;
    _spriteData[*it].in_lru = false;
    lru_list.erase(it);
}

Bitmap *SpriteCache::operator [] (sprkey_t index)
{
    // invalid sprite slot
    if (index < 0 || (size_t)index >= _spriteData.size())
        return nullptr;

    // Dynamically added sprite, don't put it on the sprite list
    if (IsExternalSprite(index))
        return _spriteData[index].Image;

    // if sprite exists in file but is not in mem, load it
    if ((_spriteData[index].Image == nullptr) &&
            (!_spriteData[index].DoesNotExist || _spriteData[index].Remapped))
        LoadSprite(index);

    // Locked sprite, eg. mouse cursor, that shouldn't be discarded
    if (_spriteData[index].Locked)
        return _spriteData[index].Image;

    // move to start
    if (_spriteData[index].in_lru) {
        auto it = _spriteData[index].lru_it;
        if (it != lru_list.end()) {
            lru_list.splice(lru_list.begin(), lru_list, it);
        }
    }

    return _spriteData[index].Image;
}


void SpriteCache::RemoveAll()
{
    auto it = lru_list.begin();
    while (it != lru_list.end()) {
        if (!_spriteData[*it].Locked && // not locked
            !_spriteData[*it].DoesNotExist) // sprite from game resource
        {
            if (_spriteData[*it].Image != nullptr) {
                delete _spriteData[*it].Image;
            }
            _spriteData[*it].Image = nullptr;
            _spriteData[*it].in_lru = false;
            it = lru_list.erase(it);
        }
        else {
            ++it;
        }
    }
}

void SpriteCache::Precache(sprkey_t index)
{
    if (index < 0 || (size_t)index >= _spriteData.size())
        return;

    soff_t sprSize = 0;

    // ensure loaded at least.
    if (_spriteData[index].Image == nullptr)
        LoadSprite(index);

    _spriteData[index].Locked = true;
    
    if (_spriteData[index].in_lru) {
        lru_list.erase(_spriteData[index].lru_it);
        _spriteData[index].in_lru = false;
    }
}

void SpriteCache::LoadSprite(sprkey_t index)
{
    if (index < 0 || (size_t)index >= _spriteData.size()) {
        quit("sprite cache array index out of bounds");
    }

    if (lru_list.size() > _maxCacheSize) {
        RemoveLeastRecentlyUsed();
    }
    

    // TODO use the stream version

    if (_spriteData[index].Image == nullptr) {
        auto path = AGS::Common::String::FromFormat("sprite/%d", (int)index);

        if (_spriteData[index].Remapped) {
            path = AGS::Common::String::FromFormat("sprite/%d", (int)_spriteData[index].RemappedTo);
        }

        auto s = gameAssetLibrary->OpenAsset(path);
        if (s == nullptr) {
            quit("sprite file not found.");
        }

        std::array<int32_t, 5> header;
        s->ReadArrayOfInt32(header.data(), header.size());
        auto &index_in_file = header[0];
        auto &coldep = header[1];
        auto &wdd = header[2];
        auto &htt = header[3];
        auto &pixel_sz = header[4];

        assert(index_in_file == index);

        // if coldep == 0, it's a null sprite
        if (coldep != 0) {
            auto *image = BitmapHelper::CreateBitmap(wdd, htt, coldep * 8);
            if (image == nullptr) { quit("error creating sprite bitmap"); }

            _sprInfos[index].Width = wdd;
            _sprInfos[index].Height = htt;
            _spriteData[index].Image = image;
            _spriteData[index].in_lru = false;

            if (coldep == 1)
            {
                for (int hh = 0; hh < htt; hh++)
                    s->ReadArray(&image->GetScanLineForWriting(hh)[0], coldep, wdd);
            }
            else if (coldep == 2)
            {
                for (int hh = 0; hh < htt; hh++)
                    s->ReadArrayOfInt16((int16_t*)&image->GetScanLineForWriting(hh)[0], wdd);
            }
            else
            {
                for (int hh = 0; hh < htt; hh++)
                    s->ReadArrayOfInt32((int32_t*)&image->GetScanLineForWriting(hh)[0], wdd);
            }

            // TODO: this is ugly: asks the engine to convert the sprite using its own knowledge.
            // And engine assigns new bitmap using SpriteCache::Set().
            // Perhaps change to the callback function pointer?
            initialize_sprite(index);
        }

        delete s;

        if (!_spriteData[index].in_lru) {
            _spriteData[index].lru_it = lru_list.insert(lru_list.begin(), index);
            _spriteData[index].in_lru = true;
        }
    }

    // leave sprite 0 locked
    if (index == 0) {
        _spriteData[index].Locked = true;
    }
}


void SpriteCache::LoadSprite(AGS::Common::Stream *s)
{
    if (s == nullptr) { return; }

    if (lru_list.size() > _maxCacheSize) {
        RemoveLeastRecentlyUsed();
    }

    std::array<int32_t, 5> header;
    s->ReadArrayOfInt32(header.data(), header.size());
    auto &index = header[0];
    auto &coldep = header[1];
    auto &wdd = header[2];
    auto &htt = header[3];
    auto &pixel_sz = header[4];

    // if coldep == 0, it's a null sprite
    // if Image is set, then it's loaded (and could be modified so don't touch)

    if (coldep != 0 && _spriteData[index].Image == nullptr) {
        auto *image = BitmapHelper::CreateBitmap(wdd, htt, coldep * 8);
        if (image == nullptr) { return; }

        _sprInfos[index].Width = wdd;
        _sprInfos[index].Height = htt;
        _spriteData[index].Image = image;
        _spriteData[index].in_lru = false;

        if (coldep == 1)
        {
            for (int hh = 0; hh < htt; hh++)
                s->ReadArray(&image->GetScanLineForWriting(hh)[0], coldep, wdd);
        }
        else if (coldep == 2)
        {
            for (int hh = 0; hh < htt; hh++)
                s->ReadArrayOfInt16((int16_t*)&image->GetScanLineForWriting(hh)[0], wdd);
        }
        else
        {
            for (int hh = 0; hh < htt; hh++)
                s->ReadArrayOfInt32((int32_t*)&image->GetScanLineForWriting(hh)[0], wdd);
        }

        // TODO: this is ugly: asks the engine to convert the sprite using its own knowledge.
        // And engine assigns new bitmap using SpriteCache::Set().
        // Perhaps change to the callback function pointer?
        initialize_sprite(index);

        if (!_spriteData[index].in_lru) {
            _spriteData[index].lru_it = lru_list.insert(lru_list.begin(), index);
            _spriteData[index].in_lru = true;
        }
    }
    else {
        s->Seek(pixel_sz);
    }

    // leave sprite 0 locked
    if (index == 0) {
        _spriteData[index].Locked = true;
    }
}


bool SpriteCache::IsLoaded(sprkey_t index) {
    return _spriteData[index].Image != nullptr;
}


HError SpriteCache::InitFile(const char *filnam)
{
    auto s = gameAssetLibrary->OpenAsset("sprite/index.dat");
    assert(s != nullptr);

    std::array<int32_t, 2> header;
    s->ReadArrayOfInt32(header.data(), header.size());
    auto &index_file_id = header[0];
    auto &topmost = header[1];

    EnlargeTo(topmost + 1);

    // no sprite index file, manually index it
    std::array<int32_t, 5> entry;
    for (sprkey_t i = 0; i <= topmost; ++i)
    {
        s->ReadArrayOfInt32(entry.data(), entry.size());
        //auto &id = entry[0];
        auto &coldep = entry[1];
        auto &wdd = entry[2];
        auto &htt = entry[3];
        //auto &sizebytes = entry[4];

        _spriteData[i].DoesNotExist = false;
        _spriteData[i].Remapped = false;
        _spriteData[i].Locked = false;
        _spriteData[i].Image = nullptr;
        _spriteData[i].in_lru = false;

        if (coldep == 0)
        {
            initFile_initNullSpriteParams(i);
            continue;
        }

        _sprInfos[i].Width = wdd;
        _sprInfos[i].Height = htt;
        get_new_size_for_sprite(i, wdd, htt, _sprInfos[i].Width, _sprInfos[i].Height);
    }

    delete s;

    return HError::None();
}
