//
// Created by 蔡国师 on 26-10-1.
//

#ifndef GUESSCOLLECT_H
#define GUESSCOLLECT_H

namespace scenarios
{
    //执行对弈竞猜采集任务
    bool executeGuessCollect();

    // 任务结束/停止后由 TaskRunner 调用重置，保证下次启动从初始状态开始
    void resetGuessCollectStatus();
}

#endif //GUESSCOLLECT_H
