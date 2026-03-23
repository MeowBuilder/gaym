#include "pch.h"
#include "JobQueue.h"
#include "GlobalQueue.h"

/*----------------
     JobQueue
-----------------*/

void JobQueue::Push(JobRef job, bool pushOnly)
{
<<<<<<< HEAD
    const int32 prevCount = _jobCount.fetch_add(1); // ?? 移댁댄몃? 利媛????? ?≪ ?몄щ? ?怨 ?ㅽ? ?ㅼ?
    _jobs.Push(job); // WRITE_LOCK

    // 泥ル?吏?Job? ?ｌ ?곕?媛 ?ㅽ源吏 ?대?
    if (prevCount == 0)
    {
        // ?대??ㅽ以??JobQueue媛 ??쇰㈃ ?ㅽ
=======
    const int32 prevCount = _jobCount.fetch_add(1); // ??긽 移댁슫?몃? 利앷??쒗궓 ?꾩뿉 ?≪쓣 ?몄돩瑜??섍퀬 ?ㅽ뻾???ㅼ쓬??
    _jobs.Push(job); // WRITE_LOCK

    // 泥ル쾲吏?Job???ｌ? ?곕젅?쒓? ?ㅽ뻾源뚯? ?대떦
    if (prevCount == 0)
    {
        // ?대? ?ㅽ뻾以묒씤 JobQueue媛 ?놁쑝硫??ㅽ뻾
>>>>>>> 72bf515ecd449477261c8282b604b7db81a3a499
        if (LCurrentJobQueue == nullptr && pushOnly == false)
        {
            Execute();
        }
        else
        {
<<<<<<< HEAD
            // ?ъ ?? ?ㅻⅨ ?곕?媛 ?ㅽ??濡 GlobalQueue? ?湲대?
=======
            // ?ъ쑀 ?덈뒗 ?ㅻⅨ ?곕젅?쒓? ?ㅽ뻾?섎룄濡?GlobalQueue???섍릿??
>>>>>>> 72bf515ecd449477261c8282b604b7db81a3a499
            GGlobalQueue->Push(shared_from_this());
        }
    }
}

<<<<<<< HEAD
// 1) ?쇨????~臾?紐곕━硫?
// 2) DoAsync ?怨 ?怨 媛?~ ?? ??吏 ?? ???(?쇨???? ?곕??? 紐곕┝)
=======
// 1) ?쇨컧????臾?紐곕━硫?
// 2) DoAsync ?怨??怨?媛?? ?덈? ?앸굹吏 ?딅뒗 ?곹솴 (?쇨컧?????곕젅?쒗븳??紐곕┝)
>>>>>>> 72bf515ecd449477261c8282b604b7db81a3a499
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

<<<<<<< HEAD
        // ?⑥ ?쇨???0媛?쇰㈃ 醫猷
        if (_jobCount.fetch_sub(jobCount) == jobCount) // ???? ??移댁댄몃? 鍮쇱??쇳??
=======
        // ?⑥? ?쇨컧??0媛쒕씪硫?醫낅즺
        if (_jobCount.fetch_sub(jobCount) == jobCount) // ???쒗썑 ??移댁슫?몃? 鍮쇱쨾?쇳븳??
>>>>>>> 72bf515ecd449477261c8282b604b7db81a3a499
        {
            LCurrentJobQueue = nullptr;
            return;
        }

        const uint64 now = ::GetTickCount64();
        if (now >= LEndTickCount)
        {
            LCurrentJobQueue = nullptr;
<<<<<<< HEAD
            // ?ъ ?? ?ㅻⅨ ?곕?媛 ?ㅽ??濡 GlobalQueue? ?湲대?
=======
            // ?ъ쑀 ?덈뒗 ?ㅻⅨ ?곕젅?쒓? ?ㅽ뻾?섎룄濡?GlobalQueue???섍릿??
>>>>>>> 72bf515ecd449477261c8282b604b7db81a3a499
            GGlobalQueue->Push(shared_from_this());
            break;
        }
    } 
}
