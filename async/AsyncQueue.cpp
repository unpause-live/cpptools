//
//  AsyncQueue.cpp
//  unpause.tools
//
//  Created by James Hurley on 6/18/17.
//  Copyright © 2017 Unpause, Inc. All rights reserved.
//

#include "AsyncQueue.h"

namespace unpause { namespace async {
    
    std::unique_ptr<SharedAsyncQueue> SharedAsyncQueue::s_saq;
    int AsyncQueue::s_threadCount = 8;
}
}
