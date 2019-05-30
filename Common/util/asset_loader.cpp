#include "asset_loader.h"

#include "util/stream.h"
#include "debug/debugmanager.h"


// followed this tutorial https://thispointer.com/c11-multithreading-part-10-packaged_task-example-and-tutorial/






namespace AGS { namespace Common {

AssetLoader gameAssetLoader;


AssetLoader::result_t LoadFileFromPhysfs(AGS::Common::String filename)
{
    auto normalisedFilename = filename.Lower();

    auto ps = std::make_unique<AGS::Common::PhysfsStream>(normalisedFilename);
    auto phandle = ps->GetPhysfsHandle();

    auto sz = PHYSFS_fileLength(phandle);

    if (sz < 0) {
        throw std::runtime_error("unable to determine size");
    }

    auto buf = AssetLoader::result_t(sz);
    ps->Read(buf.data(), buf.size());

    AGS::Common::Debug::Printf(AGS::Common::kDbgMsg_Init,  "AssetLoader thread: loaded %s : %d bytes", filename.GetCStr(), (int)sz);

    return std::move(buf);
}


AssetLoader::AssetLoader()
{
}

AssetLoader::~AssetLoader()
{
    StopBackgroundThread();
}

void AssetLoader::StartBackgroundThread()
{
    thread_running_ = true;
    bgThread_ = std::thread(&AssetLoader::BackgroundThreadMain, this);
    // bgThread_.native_handle. set cpu
}

void AssetLoader::StopBackgroundThread()
{
    thread_running_ = false;
    bgThread_.join();
}

void AssetLoader::BackgroundThreadMain()
{
    while (thread_running_) 
    {
        taskiter_t task_it;

        {
            std::unique_lock<std::mutex> lock(task_queue_mtx_);
            if (task_queue_.empty()) {
                task_queue__cv_.wait(lock, [&] {return !task_queue_.empty() || !thread_running_; });
            }

            if (!thread_running_) { break; }

            task_it = task_queue_.top();
            task_queue_.pop();
        }

        task_it->task();

        {
            std::lock_guard<std::mutex> lock(tasks_mtx_);
            tasks_.erase(task_it);
        }

    }

}


AssetLoader::future_result_t AssetLoader::LoadFileAsync(AGS::Common::String filename, int priority)
{
    auto l = [=](){ return LoadFileFromPhysfs(filename); };
    auto task = task_t(l);
    auto result = task.get_future();

    {
        std::lock_guard<std::mutex> task_lock(tasks_mtx_);
        std::lock_guard<std::mutex> queue_lock(task_queue_mtx_);

        tasks_.push_back({ priority, std::move(task) });
        auto task_it = tasks_.end();
        --task_it;

        task_queue_.emplace(task_it);
    }
    task_queue__cv_.notify_one();

    return result;
}


//
//AssetLoader::result_t AssetLoader::LoadFile(AGS::Common::String filename)
//{
//    return std::move(LoadFileAsync(filename, true).get());
//}

AGS::Common::Stream *AssetLoader::ResolveAsStream(future_result_t &future_file)
{
    try 
    {
        auto buf = std::move(future_file.get());
        auto ms = std::make_unique<MemoryStream>(buf);
        return new DataStream(std::move(ms));
    }
    catch (std::runtime_error)
    {
        return nullptr;
    }
}

AssetLoader::result_t AssetLoader::ResolveAsBuffer(future_result_t &future_file)
{
    try
    {
        return std::move(future_file.get());
    }
    catch (std::runtime_error)
    {
        return AssetLoader::result_t(0);
    }
}




}}