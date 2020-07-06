#pragma once

#include <atomic>
#include <stdint.h>
#include <assert.h>
#include <thread>
#include <algorithm>

template<typename T>
class wait_free_queue
{
	enum class wait_for_state : uint32_t { free, vaild, copying };
public:
	wait_free_queue(uint64_t capacity) :
		m_queue(nullptr),
		m_states(nullptr),
		m_enqueue_count(0),
		m_dequeue_count(0),
		m_size(0),
		m_capacity(capacity),
		m_enqueue(0),
		m_dequeue(0),
		m_offset(0),
		m_resize(false)
	{
		m_queue = new T[capacity]();
		assert(m_queue);
		m_states = new std::atomic<wait_for_state>[capacity]();
		assert(m_states);
		std::for_each(m_states, m_states + capacity,
			[](std::atomic<wait_for_state>& v)
		{
			v = wait_for_state::free;
		}
		);
	}

	~wait_free_queue()
	{
		delete[] m_queue;
		delete[] m_states;
	}

	uint64_t enqueue(const T& elem)
	{
		int64_t old_size(0);
		int64_t new_size(0);
		uint64_t old_count(0);
		uint64_t en_pos(0);
		wait_for_state before_state(wait_for_state::free);
		wait_for_state after_state(wait_for_state::copying);
		bool guard(true);

		m_enqueue++;
		while (true)
		{
			if (m_resize)
			{
				m_enqueue--;
				while (m_resize)
				{
					std::this_thread::yield();
				}
				m_enqueue++;
			}

			do
			{
				old_size = m_size;
				new_size = std::min(old_size + 1, static_cast<int64_t>(m_capacity));
			} while (!m_size.compare_exchange_strong(old_size, new_size));

			if (new_size > old_size)
			{
				break;
			}
		}

		if (new_size == m_capacity)
		{
			guard = m_resize.exchange(true);
		}

		do
		{
			old_count = m_enqueue_count;
			en_pos = (old_count + m_offset) % m_capacity;
		} while (!m_enqueue_count.compare_exchange_strong(old_count, old_count + 1));

		while (!m_states[en_pos].compare_exchange_strong(before_state, wait_for_state::copying))
		{
			before_state = wait_for_state::free;
		}
		m_queue[en_pos] = elem;
		m_states[en_pos].compare_exchange_strong(after_state, wait_for_state::vaild);

		m_enqueue--;

		if (!guard)
		{
			m_capacity = resize(new_size * 2);
			m_resize = false;
		}

		return old_count;
	}

	uint64_t enqueue_range(T* arr_elem, uint64_t count)
	{
		int64_t old_size(0);
		int64_t new_size(0);
		uint64_t old_count(0);
		uint64_t en_pos(0);
		uint64_t fill_count(0);
		uint64_t remain_count(0);
		wait_for_state before_state(wait_for_state::free);
		wait_for_state after_state(wait_for_state::copying);
		bool guard(true);

		m_enqueue++;
		while (true)
		{
			if (m_resize)
			{
				m_enqueue--;
				while (m_resize)
				{
					std::this_thread::yield();
				}
				m_enqueue++;
			}

			do
			{
				old_size = m_size;
				new_size = std::min(old_size + static_cast<int64_t>(count), static_cast<int64_t>(m_capacity));
			} while (!m_size.compare_exchange_strong(old_size, new_size));

			if (new_size > old_size)
			{
				break;
			}

		}

		if (new_size == m_capacity)
		{
			guard = m_resize.exchange(true);
		}

		fill_count = new_size - old_size;
		remain_count = count - fill_count;

		if (!guard)
		{
			m_enqueue--;
			while (m_enqueue)
			{
				std::this_thread::yield();
			}
		}

		do
		{
			old_count = m_enqueue_count;
			en_pos = (old_count + m_offset) % m_capacity;
		} while (!m_enqueue_count.compare_exchange_strong(old_count, old_count + fill_count));

		for (uint64_t i = 0; i < fill_count; i++)
		{
			while (!m_states[en_pos].compare_exchange_strong(before_state, wait_for_state::copying))
			{
				before_state = wait_for_state::free;
			}
			m_queue[en_pos] = arr_elem[i];
			m_states[en_pos].compare_exchange_strong(after_state, wait_for_state::vaild);
			en_pos = (en_pos + 1) % m_capacity;
		}

		if (guard)
		{
			m_enqueue--;
		}
		else
		{
			uint64_t new_capacity = resize((new_size + remain_count) * 2);
			for (uint64_t i = 0; i < remain_count; i++)
			{
				en_pos = (m_enqueue_count + m_offset) % new_capacity;
				while (!m_states[en_pos].compare_exchange_strong(before_state, wait_for_state::copying))
				{
					before_state = wait_for_state::free;
				}
				m_queue[en_pos] = arr_elem[i + fill_count];
				m_states[en_pos].compare_exchange_strong(after_state, wait_for_state::vaild);
				m_enqueue_count++;
			}

			m_size += remain_count;
			m_capacity = new_capacity;
			m_resize = false;
		}

		return old_count;
	}

	bool dequeue(T& elem, uint64_t* index = nullptr)
	{
		int64_t old_size(0);
		int64_t new_size(0);
		uint64_t old_count(0);
		uint64_t de_pos(0);
		wait_for_state before_state(wait_for_state::vaild);
		wait_for_state after_state(wait_for_state::copying);

		if (m_size <= 0)
		{
			return 0;
		}

		m_dequeue++;
		while (true)
		{
			if (m_resize)
			{
				m_dequeue--;
				while (m_resize)
				{
					std::this_thread::yield();
				}
				m_dequeue++;
			}

			do
			{
				old_size = m_size;
				new_size = std::max(old_size - 1, 0ll);
			} while (!m_size.compare_exchange_strong(old_size, new_size));

			if (old_size > new_size)
			{
				break;
			}
			else
			{
				m_dequeue--;
				return false;
			}
		}

		do
		{
			old_count = m_dequeue_count;
			de_pos = (old_count + m_offset) % m_capacity;
		} while (!m_dequeue_count.compare_exchange_strong(old_count, old_count + 1));

		while (!m_states[de_pos].compare_exchange_strong(before_state, wait_for_state::copying))
		{
			before_state = wait_for_state::vaild;
		}
		elem = std::move(m_queue[de_pos]);
		m_states[de_pos].compare_exchange_strong(after_state, wait_for_state::free);

		m_dequeue--;

		if (index)
		{
			*index = old_count;
		}

		return true;
	}

	uint64_t dequeue_range(T* arr_elem, uint64_t arr_size, uint64_t* index = nullptr)
	{
		uint64_t count(arr_size / sizeof(T));
		int64_t old_size(0);
		int64_t new_size(0);
		uint64_t old_count(0);
		uint64_t de_pos(0);
		wait_for_state before_state(wait_for_state::vaild);
		wait_for_state after_state(wait_for_state::copying);

		if (m_size <= 0)
		{
			return 0;
		}

		m_dequeue++;
		while (true)
		{
			if (m_resize)
			{
				m_dequeue--;
				while (m_resize)
				{
					std::this_thread::yield();
				}
				m_dequeue++;
			}

			do
			{
				old_size = m_size;
				new_size = std::max(old_size - static_cast<int64_t>(count), 0ll);
			} while (!m_size.compare_exchange_strong(old_size, new_size));

			if (old_size > new_size)
			{
				break;
			}
			else
			{
				m_dequeue--;
				return false;
			}
		}

		count = old_size - new_size;

		do
		{
			old_count = m_dequeue_count;
			de_pos = (old_count + m_offset) % m_capacity;
		} while (!m_dequeue_count.compare_exchange_strong(old_count, old_count + count));

		for (uint64_t i = 0; i < count; i++)
		{
			while (!m_states[de_pos].compare_exchange_strong(before_state, wait_for_state::copying))
			{
				before_state = wait_for_state::vaild;
			}
			arr_elem[i] = std::move(m_queue[de_pos]);
			m_states[de_pos].compare_exchange_strong(after_state, wait_for_state::free);
			de_pos = (de_pos + 1) % m_capacity;
		}

		m_dequeue--;

		if (index)
		{
			*index = old_count;
		}

		return count;
	}

	size_t size()
	{
		return m_size;
	}

private:
	T* m_queue;
	std::atomic<wait_for_state>* m_states;
	std::atomic<uint64_t>				m_enqueue_count;
	std::atomic<uint64_t>				m_dequeue_count;
	std::atomic<int64_t>				m_size;
	std::atomic<uint64_t>				m_capacity;
	std::atomic<uint64_t>				m_enqueue;
	std::atomic<uint64_t>				m_dequeue;
	std::atomic<uint64_t>				m_offset;
	std::atomic<bool>					m_resize;


	uint64_t resize(uint64_t length)
	{
		assert(length > m_capacity);

		while (m_dequeue | m_enqueue)
		{
			std::this_thread::yield();
		}

		T* new_queue = new T[length];
		assert(new_queue);
		std::atomic<wait_for_state>* new_states = new std::atomic<wait_for_state>[length];
		assert(new_states);


		uint64_t head_pos((m_dequeue_count + m_offset) % m_capacity);
		uint64_t tail_pos((m_enqueue_count + m_offset) % m_capacity);

		for (uint64_t i = 0; i < m_size; i++)
		{
			new_queue[i] = m_queue[head_pos];
			head_pos = (head_pos + 1) % m_capacity;
			new_states[i] = wait_for_state::vaild;
		}

		for (uint64_t i = m_size; i < length; i++)
		{
			new_states[i] = wait_for_state::free;
		}

		assert(head_pos == tail_pos);

		delete m_queue;
		delete m_states;
		m_queue = new_queue;
		m_states = new_states;
		m_offset = length - (m_dequeue_count % length);

		return length;
	}
};

