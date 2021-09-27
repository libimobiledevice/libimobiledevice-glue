/*
 * thread.c
 *
 * Copyright (c) 2012-2019 Nikias Bassen, All Rights Reserved.
 * Copyright (c) 2012 Martin Szulecki, All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "common.h"
#include "libimobiledevice-glue/thread.h"

LIBIMOBILEDEVICE_GLUE_API int thread_new(THREAD_T *thread, thread_func_t thread_func, void* data)
{
#ifdef WIN32
	HANDLE th = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)thread_func, data, 0, NULL);
	if (th == NULL) {
		return -1;
	}
	*thread = th;
	return 0;
#else
	int res = pthread_create(thread, NULL, thread_func, data);
	return res;
#endif
}

LIBIMOBILEDEVICE_GLUE_API void thread_detach(THREAD_T thread)
{
#ifdef WIN32
	CloseHandle(thread);
#else
	pthread_detach(thread);
#endif
}

LIBIMOBILEDEVICE_GLUE_API void thread_free(THREAD_T thread)
{
#ifdef WIN32
	CloseHandle(thread);
#endif
}

LIBIMOBILEDEVICE_GLUE_API int thread_join(THREAD_T thread)
{
	/* wait for thread to complete */
#ifdef WIN32
	return (int)WaitForSingleObject(thread, INFINITE);
#else
	return pthread_join(thread, NULL);
#endif
}

LIBIMOBILEDEVICE_GLUE_API int thread_alive(THREAD_T thread)
{
	if (!thread)
		return 0;
#ifdef WIN32
	return TerminateThread(thread, 0);
#else
	return pthread_kill(thread, 0) == 0;
#endif
}

LIBIMOBILEDEVICE_GLUE_API int thread_cancel(THREAD_T thread)
{
#ifdef WIN32
	return -1;
#else
#ifdef HAVE_PTHREAD_CANCEL
	return pthread_cancel(thread);
#else
	return -1;
#endif
#endif
}

LIBIMOBILEDEVICE_GLUE_API void mutex_init(mutex_t* mutex)
{
#ifdef WIN32
	InitializeCriticalSection(mutex);
#else
	pthread_mutex_init(mutex, NULL);
#endif
}

LIBIMOBILEDEVICE_GLUE_API void mutex_destroy(mutex_t* mutex)
{
#ifdef WIN32
	DeleteCriticalSection(mutex);
#else
	pthread_mutex_destroy(mutex);
#endif
}

LIBIMOBILEDEVICE_GLUE_API void mutex_lock(mutex_t* mutex)
{
#ifdef WIN32
	EnterCriticalSection(mutex);
#else
	pthread_mutex_lock(mutex);
#endif
}

LIBIMOBILEDEVICE_GLUE_API void mutex_unlock(mutex_t* mutex)
{
#ifdef WIN32
	LeaveCriticalSection(mutex);
#else
	pthread_mutex_unlock(mutex);
#endif
}

LIBIMOBILEDEVICE_GLUE_API void thread_once(thread_once_t *once_control, void (*init_routine)(void))
{
#ifdef WIN32
	while (InterlockedExchange(&(once_control->lock), 1) != 0) {
		Sleep(1);
	}
	if (!once_control->state) {
		once_control->state = 1;
		init_routine();
	}
	InterlockedExchange(&(once_control->lock), 0);
#else
	pthread_once(once_control, init_routine);
#endif
}

LIBIMOBILEDEVICE_GLUE_API void cond_init(cond_t* cond)
{
#ifdef WIN32
	cond->sem = CreateSemaphore(NULL, 0, 32767, NULL);
#else
	pthread_cond_init(cond, NULL);
#endif
}

LIBIMOBILEDEVICE_GLUE_API void cond_destroy(cond_t* cond)
{
#ifdef WIN32
	CloseHandle(cond->sem);
#else
	pthread_cond_destroy(cond);
#endif
}

LIBIMOBILEDEVICE_GLUE_API int cond_signal(cond_t* cond)
{
#ifdef WIN32
	int result = 0;
	if (!ReleaseSemaphore(cond->sem, 1, NULL)) {
		result = -1;
	}
	return result;
#else
	return pthread_cond_signal(cond);
#endif
}

LIBIMOBILEDEVICE_GLUE_API int cond_wait(cond_t* cond, mutex_t* mutex)
{
#ifdef WIN32
	mutex_unlock(mutex);
	DWORD res = WaitForSingleObject(cond->sem, INFINITE);
	switch (res) {
		case WAIT_OBJECT_0:
			return 0;
		default:
			return -1;
	}
#else
	return pthread_cond_wait(cond, mutex);
#endif
}

LIBIMOBILEDEVICE_GLUE_API int cond_wait_timeout(cond_t* cond, mutex_t* mutex, unsigned int timeout_ms)
{
#ifdef WIN32
	mutex_unlock(mutex);
	DWORD res = WaitForSingleObject(cond->sem, timeout_ms);
	switch (res) {
		case WAIT_OBJECT_0:
		case WAIT_TIMEOUT:
			return 0;
		default:
			return -1;
	}
#else
	struct timespec ts;
	struct timeval now;
	gettimeofday(&now, NULL);

	ts.tv_sec = now.tv_sec + timeout_ms / 1000;
	ts.tv_nsec = now.tv_usec * 1000 + 1000 * 1000 * (timeout_ms % 1000);
	ts.tv_sec += ts.tv_nsec / (1000 * 1000 * 1000);
	ts.tv_nsec %= (1000 * 1000 * 1000);

	return pthread_cond_timedwait(cond, mutex, &ts);
#endif
}
