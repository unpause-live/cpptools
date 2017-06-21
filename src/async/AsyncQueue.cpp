/* Copyright (c) 2017 Unpause, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 3.0 of the License, or (at your option) any later version.
 *  See the file LICENSE included with this distribution for more
 *  information.
 */

#include <unpause/tools/async/AsyncQueue.h>

namespace unpause { namespace async {
    
    std::unique_ptr<SharedAsyncQueue> SharedAsyncQueue::s_saq;
    int AsyncQueue::s_threadCount = 8;
}
}
