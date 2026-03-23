#include "pch.h"
#include "JobQueue.h"
#include "GlobalQueue.h"

/*----------------
     JobQueue
-----------------*/

void JobQueue::Push(JobRef job, bool pushOnly)
{
    const int32 prevCount = _jobCount.fetch_add(1); // ?? 移댁댄몃? 利媛????? ?≪ ?몄щ? ?怨 ?ㅽ? ?ㅼ?
    _jobs.Push(job); // WRITE_LOCK

    // 泥ル?吏?Job? ?ｌ ?곕?媛 ?ㅽ源吏 ?대?
    if (prevCount == 0)
    {
        // ?대??ㅽ以??JobQueue媛 ??쇰㈃ ?ㅽ
        if (LCurrentJobQueue == nullptr && pushOnly == false)
        {
            Execute();
        }
        else
        {
            // ?ъ ?? ?ㅻⅨ ?곕?媛 ?ㅽ??濡 GlobalQueue? ?湲대?
            GGlobalQueue->Push(shared_from_this());
        }
    }
}

// 1) ?쇨????~臾?紐곕━硫?
// 2) DoAsync ?怨 ?怨 媛?~ ?? ??吏 ?? ???(?쇨???? ?곕??? 紐곕┝)
void JobQueue::Execute()
{
    LCurrentJobQueue = this;

    while (true)
    {
        Vector<JobRef> jobs;
        _jobs.PopAll(OUT jobs);

        const int32 jobCount = static_cast<int32>(jobs.size());
        for (int32 i = 0; i < jobCount; i++)
            jobs[i]->Execute();

        // ?⑥ ?쇨???0媛?쇰㈃ 醫猷
        if (_jobCount.fetch_sub(jobCount) == jobCount) // ???? ??移댁댄몃? 鍮쇱??쇳??
        {
            LCurrentJobQueue = nullptr;
            return;
        }

        const uint64 now = ::GetTickCount64();
        if (now >= LEndTickCount)
        {
            LCurrentJobQueue = nullptr;
            // ?ъ ?? ?ㅻⅨ ?곕?媛 ?ㅽ??濡 GlobalQueue? ?湲대?
            GGlobalQueue->Push(shared_from_this());
            break;
        }
    } 
}
