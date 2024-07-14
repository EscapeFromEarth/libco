/*
* Tencent is pleased to support the open source community by making Libco available.

* Copyright (C) 2014 THL A29 Limited, a Tencent company. All rights reserved.
*
* Licensed under the Apache License, Version 2.0 (the "License"); 
* you may not use this file except in compliance with the License. 
* You may obtain a copy of the License at
*
*	http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, 
* software distributed under the License is distributed on an "AS IS" BASIS, 
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
* See the License for the specific language governing permissions and 
* limitations under the License.
*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <queue>
#include "co_routine.h"
using namespace std;
struct stTask_t
{
	int id;
};
struct stEnv_t
{
	stCoCond_t* cond;
	queue<stTask_t*> task_queue;
};
void* Producer(void* args)
{
	co_enable_hook_sys();
	stEnv_t* env=  (stEnv_t*)args;
	int id = 0;
	while (true)
	{
		stTask_t* task = (stTask_t*)calloc(1, sizeof(stTask_t));
		task->id = id++;
		env->task_queue.push(task);
		printf("%s:%d produce task %d\n", __func__, __LINE__, task->id);
		co_cond_signal(env->cond);
		poll(NULL, 0, 600);
		// printf("why ...\n");
	}
	return NULL;
}
void* Consumer(void* args)
{
	// 放在协程的任务头，表示本协程支持。
	co_enable_hook_sys();
	stEnv_t* env = (stEnv_t*)args;
	while (true)
	{
		if (env->task_queue.empty())
		{
			// 这东西的实现妙啊，暂不看那个超时事件
			co_cond_timedwait(env->cond, -1);
			continue; // 这里真的有必要 continue ？
		}
		stTask_t* task = env->task_queue.front();
		env->task_queue.pop();
		printf("%s:%d consume task %d\n", __func__, __LINE__, task->id);
		free(task);
	}
	return NULL;
}
int main()
{
	stEnv_t* env = new stEnv_t;
	env->cond = co_cond_alloc();

	stCoRoutine_t* consumer_routine;
	co_create(&consumer_routine, NULL, Consumer, env);
	co_resume(consumer_routine);

	stCoRoutine_t* producer_routine;
	co_create(&producer_routine, NULL, Producer, env);
	co_resume(producer_routine);
	
	co_eventloop(co_get_epoll_ct(), NULL, NULL);
	return 0;
}

/*

Producer:44 produce task 0
Consumer:63 consume task 0
Producer:44 produce task 1
Consumer:63 consume task 1
Producer:44 produce task 2
Consumer:63 consume task 2
Producer:44 produce task 3
Consumer:63 consume task 3
Producer:44 produce task 4
Consumer:63 consume task 4
Producer:44 produce task 5
Consumer:63 consume task 5

以上就是一个线程做出来的事情！里面有生产者协程、消费者协程，以及调度器协程（即主协程）。
其中的调度器协程专门去响应 IO 和超时事件，然后把获得的

协程就是：
  - 把需要陷入内核、开销较大的线程切换换成用户态的、轻量级的协程切换。
  - 把 poll 这种造成线程阻塞的系统调用 hook 掉，变成只是阻塞协程。
    - 原理应该是把东西注册到 epoll 上，然后就走了，总之换了个方法。
  - 把 cond、mutex 这种也是造成线程阻塞、至少会陷入内核的同步元语实现成用户态的、只阻塞协程的。
    - 这是因为只是同一个线程里面的竞争，不可能不安全。
    - 如果你要不同线程之间的竞争了，那肯定还是要用系统的那些锁。

通过以上的三大方法，实现了用户级别的调度！！！

*/