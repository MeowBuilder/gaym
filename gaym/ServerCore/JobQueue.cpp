#include "pch.h"
#include "JobQueue.h"
#include "GlobalQueue.h"

/*----------------
     JobQueue
-----------------*/

void JobQueue::Push(JobRef job, bool pushOnly)
{
    const int32 prevCount = _jobCount.fetch_add(1); // ??긽 移댁슫?몃? 利앷??쒗궓 ?꾩뿉 ?≪쓣 ?몄돩瑜??섍퀬 ?ㅽ뻾???ㅼ쓬??
    _jobs.Push(job); // WRITE_LOCK

    // 泥ル쾲吏?Job???ｌ? ?곕젅?쒓? ?ㅽ뻾源뚯? ?대떦
    if (prevCount == 0)
    {
        // ?대? ?ㅽ뻾以묒씤 JobQueue媛 ?놁쑝硫??ㅽ뻾
        if (LCurrentJobQueue == nullptr && pushOnly == false)
        {
            Execute();
        }
        else
        {
            // ?ъ쑀 ?덈뒗 ?ㅻⅨ ?곕젅?쒓? ?ㅽ뻾?섎룄濡?GlobalQueue???섍릿??
            GGlobalQueue->Push(shared_from_this());
        }
    }
}

// 1) ?쇨컧????臾?紐곕━硫?
// 2) DoAsync ?怨??怨?媛?? ?덈? ?앸굹吏 ?딅뒗 ?곹솴 (?쇨컧?????곕젅?쒗븳??紐곕┝)
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

        // ?⑥? ?쇨컧??0媛쒕씪硫?醫낅즺
        if (_jobCount.fetch_sub(jobCount) == jobCount) // ???쒗썑 ??移댁슫?몃? 鍮쇱쨾?쇳븳??
        {
            LCurrentJobQueue = nullptr;
            return;
        }

        const uint64 now = ::GetTickCount64();
        if (now >= LEndTickCount)
        {
            LCurrentJobQueue = nullptr;
            // ?ъ쑀 ?덈뒗 ?ㅻⅨ ?곕젅?쒓? ?ㅽ뻾?섎룄濡?GlobalQueue???섍릿??
            GGlobalQueue->Push(shared_from_this());
            break;
        }
    } 
}
