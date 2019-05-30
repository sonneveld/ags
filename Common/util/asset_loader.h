#ifndef __AGS_CN_UTIL_ASSET_LOADER_H
#define __AGS_CN_UTIL_ASSET_LOADER_H

#include <array>
#include <condition_variable>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <deque>
#include <list>

#include "util/string.h"
#include "util/stream.h"

namespace AGS { namespace Common {



struct LoadAssetResult
{
    AGS::Common::String filename;
    bool success;
    std::vector<char> buffer;
};




class AssetLoader
{
public:
    using result_t = std::vector<uint8_t>;
    using future_result_t = std::future<result_t>;
    using task_t = std::packaged_task<result_t()>;


    AssetLoader();
    virtual ~AssetLoader();

    void StartBackgroundThread();
    void StopBackgroundThread();

    //void PreloadFile(AGS::Common::String &filename);
    //LoadAssetResult AssetLoader::LoadFile(AGS::Common::String &filename);

    future_result_t LoadFileAsync(AGS::Common::String filename, int priority=0);

    //result_t LoadFile(AGS::Common::String filename);

    AGS::Common::Stream *ResolveAsStream(future_result_t &future_file);
    result_t ResolveAsBuffer(future_result_t &future_file);


private:
    //std::future<std::vector<char>> LoadFileAsync(AGS::Common::String &filename);
    void BackgroundThreadMain();

    bool thread_running_ = false;
    std::thread bgThread_ {};

    //std::mutex queue_mtx_ {};
    //std::condition_variable queue_cv_;
    //std::queue<AGS::Common::String> queue_;

    //std::mutex loaded_mtx_{};
    //std::condition_variable loaded_cv_;
    //std::map<AGS::Common::String, LoadAssetResult> loaded_;



    struct TaskEntry {
        int priority;
        task_t task;
    };


    std::mutex tasks_mtx_{};
    std::list< TaskEntry > tasks_;
    using taskiter_t = std::list< TaskEntry >::iterator;


    struct entry_cmp
    {
        bool operator()(const taskiter_t &left, const taskiter_t &right) const
        {
            return left->priority > right->priority;
        }
    };

    std::mutex task_queue_mtx_{};
    std::condition_variable task_queue__cv_;
    std::priority_queue< taskiter_t, std::vector<taskiter_t>, entry_cmp > task_queue_;



    //std::priority_queue < TaskQueueEntry,  decltype(task_compare) > task_queue_;


};


extern AssetLoader gameAssetLoader;


}}
#endif