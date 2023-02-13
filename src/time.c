#include <shnet/time.h>
#include <shnet/error.h>

#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <stdatomic.h>


uint64_t
time_sec_to_ms(const uint64_t sec)
{
	return sec * 1000;
}


uint64_t
time_sec_to_us(const uint64_t sec)
{
	return sec * 1000000;
}


uint64_t
time_sec_to_ns(const uint64_t sec)
{
	return sec * 1000000000;
}



uint64_t
time_ms_to_sec(const uint64_t ms)
{
	return ms / 1000;
}


uint64_t
time_ms_to_us(const uint64_t ms)
{
	return ms * 1000;
}


uint64_t
time_ms_to_ns(const uint64_t ms)
{
	return ms * 1000000;
}



uint64_t
time_us_to_sec(const uint64_t us)
{
	return us / 1000000;
}


uint64_t
time_us_to_ms(const uint64_t us)
{
	return us / 1000;
}


uint64_t
time_us_to_ns(const uint64_t us)
{
	return us * 1000;
}



uint64_t
time_ns_to_sec(const uint64_t ns)
{
	return ns / 1000000000;
}


uint64_t
time_ns_to_ms(const uint64_t ns)
{
	return ns / 1000000;
}


uint64_t
time_ns_to_us(const uint64_t ns)
{
	return ns / 1000;
}



uint64_t
time_get_sec(const uint64_t sec)
{
	return time_get_ns(time_sec_to_ns(sec));
}


uint64_t
time_get_ms(const uint64_t ms)
{
	return time_get_ns(time_ms_to_ns(ms));
}


uint64_t
time_get_us(const uint64_t us)
{
	return time_get_ns(time_us_to_ns(us));
}


uint64_t
time_get_ns(const uint64_t ns)
{
	struct timespec tp;

	(void) clock_gettime(CLOCK_REALTIME, &tp);

	return ns + tp.tv_sec * 1000000000 + tp.tv_nsec;
}


uint64_t
time_get_time()
{
	return time_get_ns(0);
}



static uint64_t
time_peak_timeouts(const struct time_timers* const timers)
{
	return timers->timeouts[1].time;
}


#define timeof(idx)						\
(										\
	timers->intervals[idx].base_time +	\
	timers->intervals[idx].interval *	\
	timers->intervals[idx].count		\
)


static uint64_t
time_peak_intervals(const struct time_timers* const timers)
{
	return timeof(1);
}


static uint64_t
time_get_latest(const struct time_timers* const timers)
{
	return atomic_load_explicit(&timers->latest, memory_order_acquire);
}


static int
time_set_latest_raw(struct time_timers* const timers)
{
	const uint64_t old = time_get_latest(timers);

	uint64_t latest;

	if(timers->timeouts_used > 1)
	{
		const uint64_t timeouts = time_peak_timeouts(timers);

		if(timers->intervals_used > 1)
		{
			const uint64_t intervals = time_peak_intervals(timers);

			if(timeouts <= intervals)
			{
				latest = timeouts & (~1);
			}
			else
			{
				latest = intervals | 1;
			}
		}
		else
		{
			latest = timeouts & (~1);
		}
	}
	else
	{
		if(timers->intervals_used > 1)
		{
			latest = time_peak_intervals(timers) | 1;
		}
		else
		{
			latest = 0;
		}
	}

	atomic_store_explicit(&timers->latest, latest, memory_order_release);

	return old != latest;
}



/*
 * TIMEOUTS
 */

static void
time_timeouts_swap(const struct time_timers* const timers,
	const uint32_t one, const uint32_t two)
{
	timers->timeouts[one] = timers->timeouts[two];

	if(timers->timeouts[one].ref != NULL)
	{
		timers->timeouts[one].ref->ref = one;
	}
}


static void
time_timeouts_down(const struct time_timers* const timers, uint32_t timer)
{
	const uint32_t save = timer;

	timers->timeouts[0] = timers->timeouts[timer];

	while(1)
	{
		uint32_t lchild = timer << 1;
		uint32_t rchild = lchild + 1;

		if(rchild >= timers->timeouts_used)
		{
			if(
				lchild < timers->timeouts_used &&
				timers->timeouts[0].time > timers->timeouts[lchild].time
			)
			{
				time_timeouts_swap(timers, timer, lchild);

				timer = lchild;
			}
			else
			{
				break;
			}
		}
		else
		{
			if(timers->timeouts[lchild].time < timers->timeouts[rchild].time)
			{
				if(timers->timeouts[0].time > timers->timeouts[lchild].time)
				{
					time_timeouts_swap(timers, timer, lchild);

					timer = lchild;
				}
				else
				{
					break;
				}
			}
			else if(timers->timeouts[0].time > timers->timeouts[rchild].time)
			{
				time_timeouts_swap(timers, timer, rchild);

				timer = rchild;
			}
			else
			{
				break;
			}
		}
	}

	if(save != timer)
	{
		time_timeouts_swap(timers, timer, 0);
	}
}


static int
time_timeouts_up(const struct time_timers* const timers, uint32_t timer)
{
	const uint32_t save = timer;
	uint32_t parent = timer >> 1;

	timers->timeouts[0] = timers->timeouts[timer];

	while(
		parent > 0 &&
		timers->timeouts[parent].time > timers->timeouts[0].time
	)
	{
		time_timeouts_swap(timers, timer, parent);

		timer = parent;
		parent >>= 1;
	}

	const int ret = save != timer;

	if(ret)
	{
		time_timeouts_swap(timers, timer, 0);
	}

	return ret;
}


static void
time_free_timeouts(struct time_timers* const timers)
{
	free(timers->timeouts);

	timers->timeouts = NULL;
	timers->timeouts_used = 1;
	timers->timeouts_size = 0;
}


static int
time_resize_timeouts_raw(struct time_timers* const timers,
	const uint32_t new_size)
{
	if(new_size == timers->timeouts_size)
	{
		return 0;
	}

	if(new_size == 0)
	{
		time_free_timeouts(timers);

		return 0;
	}

	void* const ptr =
		shnet_realloc(timers->timeouts, sizeof(struct time_timeout) * new_size);

	if(ptr == NULL)
	{
		return -1;
	}

	timers->timeouts = ptr;
	timers->timeouts_size = new_size;

	return 0;
}


static int
time_add_timeout_common(struct time_timers* const timers,
	const struct time_timeout* const timeout, const int lock)
{
	if(lock)
	{
		time_lock(timers);
	}

	const uint32_t new_size = timers->timeouts_used + 1;

	if(
		new_size > timers->timeouts_size &&
		time_resize_timeouts_raw(timers, (new_size << 1) | 1) &&
		time_resize_timeouts_raw(timers, new_size)
	)
	{
		if(lock)
		{
			time_unlock(timers);
		}

		return -1;
	}

	if(timeout->ref != NULL)
	{
		timeout->ref->ref = timers->timeouts_used;
	}

	timers->timeouts[timers->timeouts_used++] = *timeout;

	(void) time_timeouts_up(timers, timers->timeouts_used - 1);

	if(time_set_latest_raw(timers))
	{
		(void) sem_post(&timers->updates);
	}

	if(lock)
	{
		time_unlock(timers);
	}

	(void) sem_post(&timers->work);

	return 0;
}


int
time_add_timeout_raw(struct time_timers* const timers,
	const struct time_timeout* const timeout)
{
	return time_add_timeout_common(timers, timeout, 0);
}


int
time_add_timeout(struct time_timers* const timers,
	const struct time_timeout* const timeout)
{
	return time_add_timeout_common(timers, timeout, 1);
}


int
time_cancel_timeout_raw(struct time_timers* const timers,
	struct time_timer* const timer)
{
	if(timer->ref == 0)
	{
		return -1;
	}

	timers->timeouts[timer->ref] = timers->timeouts[--timers->timeouts_used];

	if(!time_timeouts_up(timers, timer->ref))
	{
		time_timeouts_down(timers, timer->ref);
	}

	(void) time_set_latest_raw(timers);

	timer->ref = 0;

	return 0;
}


int
time_cancel_timeout(struct time_timers* const timers,
	struct time_timer* const timer)
{
	time_lock(timers);

	const int ret = time_cancel_timeout_raw(timers, timer);

	time_unlock(timers);

	return ret;
}


struct time_timeout*
time_open_timeout_raw(struct time_timers* const timers,
	struct time_timer* const timer)
{
	if(timer->ref != 0)
	{
		return timers->timeouts + timer->ref;
	}

	return NULL;
}


struct time_timeout*
time_open_timeout(struct time_timers* const timers,
	struct time_timer* const timer)
{
	time_lock(timers);

	struct time_timeout* const ret = time_open_timeout_raw(timers, timer);

	if(ret == NULL)
	{
		time_unlock(timers);
	}

	return ret;
}


void
time_close_timeout_raw(struct time_timers* const timers,
	struct time_timer* const timer)
{
	if(timer->ref == 0)
	{
		return;
	}

	if(!time_timeouts_up(timers, timer->ref))
	{
		time_timeouts_down(timers, timer->ref);
	}

	(void) time_set_latest_raw(timers);
}


void
time_close_timeout(struct time_timers* const timers,
	struct time_timer* const timer)
{
	time_close_timeout_raw(timers, timer);

	time_unlock(timers);
}



/*
 * INTERVALS
 */

static void
time_intervals_swap(const struct time_timers* const timers,
	const uint32_t one, const uint32_t two)
{
	timers->intervals[one] = timers->intervals[two];

	if(timers->intervals[one].ref != NULL)
	{
		timers->intervals[one].ref->ref = one;
	}
}


static void
time_intervals_down(const struct time_timers* const timers, uint32_t timer)
{
	const uint32_t save = timer;

	timers->intervals[0] = timers->intervals[timer];

	while(1)
	{
		uint32_t lchild = timer << 1;
		uint32_t rchild = lchild + 1;

		if(rchild >= timers->intervals_used)
		{
			if(lchild < timers->intervals_used && timeof(0) > timeof(lchild))
			{
				time_intervals_swap(timers, timer, lchild);

				timer = lchild;
			}
			else
			{
				break;
			}
		}
		else
		{
			const uint64_t lchild_t = timeof(lchild);
			const uint64_t rchild_t = timeof(rchild);

			if(lchild_t < rchild_t)
			{
				if(timeof(0) > lchild_t)
				{
					time_intervals_swap(timers, timer, lchild);

					timer = lchild;
				}
				else
				{
					break;
				}
			}
			else if(timeof(0) > rchild_t)
			{
				time_intervals_swap(timers, timer, rchild);

				timer = rchild;
			}
			else
			{
				break;
			}
		}
	}

	if(save != timer)
	{
		time_intervals_swap(timers, timer, 0);
	}
}


static int
time_intervals_up(const struct time_timers* const timers, uint32_t timer)
{
	const uint32_t save = timer;
	uint32_t parent = timer >> 1;

	timers->intervals[0] = timers->intervals[timer];

	while(parent > 0 && timeof(parent) > timeof(0))
	{
		time_intervals_swap(timers, timer, parent);

		timer = parent;
		parent >>= 1;
	}

	const int ret = save != timer;

	if(ret)
	{
		time_intervals_swap(timers, timer, 0);
	}

	return ret;
}


#undef timeof


static void
time_free_intervals(struct time_timers* const timers)
{
	free(timers->intervals);

	timers->intervals = NULL;
	timers->intervals_used = 1;
	timers->intervals_size = 0;
}


static int
time_resize_intervals_raw(struct time_timers* const timers,
	const uint32_t new_size)
{
	if(new_size == timers->intervals_size)
	{
		return 0;
	}

	if(new_size == 0)
	{
		time_free_intervals(timers);

		return 0;
	}

	void* const ptr = shnet_realloc(timers->intervals,
		sizeof(struct time_interval) * new_size);

	if(ptr == NULL)
	{
		return -1;
	}

	timers->intervals = ptr;
	timers->intervals_size = new_size;

	return 0;
}


static int
time_add_interval_common(struct time_timers* const timers,
	const struct time_interval* const interval, const int lock)
{
	if(lock)
	{
		time_lock(timers);
	}

	const uint32_t new_size = timers->intervals_used + 1;

	if(
		new_size >= timers->intervals_size &&
		time_resize_intervals_raw(timers, (new_size << 1) | 1) &&
		time_resize_intervals_raw(timers, new_size)
	)
	{
		if(lock)
		{
			time_unlock(timers);
		}

		return -1;
	}

	if(interval->ref != NULL)
	{
		interval->ref->ref = timers->intervals_used;
	}

	timers->intervals[timers->intervals_used++] = *interval;

	(void) time_intervals_up(timers, timers->intervals_used - 1);

	if(time_set_latest_raw(timers))
	{
		(void) sem_post(&timers->updates);
	}

	if(lock)
	{
		time_unlock(timers);
	}

	(void) sem_post(&timers->work);

	return 0;
}


int
time_add_interval_raw(struct time_timers* const timers,
	const struct time_interval* const interval)
{
	return time_add_interval_common(timers, interval, 0);
}


int
time_add_interval(struct time_timers* const timers,
	const struct time_interval* const interval)
{
	return time_add_interval_common(timers, interval, 1);
}


int
time_cancel_interval_raw(struct time_timers* const timers,
	struct time_timer* const timer)
{
	if(timer->ref == 0)
	{
		return -1;
	}

	timers->intervals[timer->ref] = timers->intervals[--timers->intervals_used];

	if(!time_intervals_up(timers, timer->ref))
	{
		time_intervals_down(timers, timer->ref);
	}

	(void) time_set_latest_raw(timers);

	timer->ref = 0;

	return 0;
}


int
time_cancel_interval(struct time_timers* const timers,
	struct time_timer* const timer)
{
	time_lock(timers);

	const int ret = time_cancel_interval_raw(timers, timer);

	time_unlock(timers);

	return ret;
}


struct time_interval*
time_open_interval_raw(struct time_timers* const timers,
	struct time_timer* const timer)
{
	if(timer->ref != 0)
	{
		return timers->intervals + timer->ref;
	}

	return NULL;
}


struct time_interval*
time_open_interval(struct time_timers* const timers,
	struct time_timer* const timer)
{
	struct time_interval* const ret = time_open_interval_raw(timers, timer);

	if(ret == NULL)
	{
		time_unlock(timers);
	}

	return ret;
}


void
time_close_interval_raw(struct time_timers* const timers,
	struct time_timer* const timer)
{
	if(timer->ref == 0)
	{
		return;
	}

	if(!time_intervals_up(timers, timer->ref))
	{
		time_intervals_down(timers, timer->ref);
	}

	(void) time_set_latest_raw(timers);
}


void
time_close_interval(struct time_timers* const timers,
	struct time_timer* const timer)
{
	time_close_interval_raw(timers, timer);

	time_unlock(timers);
}



void*
time_thread(void* time_thread_data)
{
	struct time_timers* const timers = time_thread_data;

	while(1)
	{
		start:

		while(sem_wait(&timers->work) != 0);

		uint64_t time;

		while(1)
		{
			time = time_get_latest(timers);

			if(time == 0)
			{
				goto start;
			}

			if(time_get_time() >= time)
			{
				break;
			}

			(void) sem_timedwait(&timers->updates, &(
			(struct timespec)
			{
				.tv_sec = time_ns_to_sec(time),
				.tv_nsec = time % 1000000000
			}
			));
		}

		void (*func)(void*) = NULL;
		void* data;

		time_lock(timers);

		time = time_get_latest(timers);

		if(time == 0 || time_get_time() < time)
		{
			time_unlock(timers);

			goto start;
		}

		if(time & 1)
		{
			func = timers->intervals[1].func;
			data = timers->intervals[1].data;
			++timers->intervals[1].count;

			time_intervals_down(timers, 1);
			(void) sem_post(&timers->work);
		}
		else
		{
			func = timers->timeouts[1].func;
			data = timers->timeouts[1].data;

			if(timers->timeouts[1].ref != NULL)
			{
				timers->timeouts[1].ref->ref = 0;
			}

			if(--timers->timeouts_used > 1)
			{
				time_timeouts_swap(timers, 1, timers->timeouts_used);
				time_timeouts_down(timers, 1);
			}
		}

		(void) time_set_latest_raw(timers);

		time_unlock(timers);

		if(func != NULL)
		{
			pthread_cancel_off();

			func(data);

			pthread_cancel_on();
		}
	}

	return NULL;
}


int
time_timers(struct time_timers* const timers)
{
	int err;

	safe_execute(err = sem_init(&timers->work, 0, 0), err, errno);

	if(err)
	{
		return -1;
	}

	safe_execute(err = sem_init(&timers->updates, 0, 0), err, errno);

	if(err)
	{
		goto err_work;
	}

	safe_execute(err = pthread_mutex_init(&timers->mutex, NULL), err, err);

	if(err)
	{
		errno = err;

		goto err_updates;
	}

	timers->timeouts_used = 1;
	timers->intervals_used = 1;

	return 0;

	err_updates:

	(void) sem_destroy(&timers->updates);

	err_work:

	(void) sem_destroy(&timers->work);

	return -1;
}


int
time_start(struct time_timers* const timers)
{
	return pthread_start(&timers->thread, time_thread, timers);
}


void
time_stop_sync(struct time_timers* const timers)
{
	pthread_cancel_sync(timers->thread);
}


void
time_stop_async(struct time_timers* const timers)
{
	pthread_cancel_async(timers->thread);
}


void
time_free(struct time_timers* const timers)
{
	(void) sem_destroy(&timers->work);
	(void) sem_destroy(&timers->updates);
	(void) pthread_mutex_destroy(&timers->mutex);
	time_free_timeouts(timers);
	time_free_intervals(timers);
}


void
time_lock(struct time_timers* const timers)
{
	(void) pthread_mutex_lock(&timers->mutex);
}


void
time_unlock(struct time_timers* const timers)
{
	(void) pthread_mutex_unlock(&timers->mutex);
}
