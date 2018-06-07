#pragma once

#include <atomic>

class spin_mutex {
	std::atomic<bool> flag = ATOMIC_VAR_INIT(false);
public:
	spin_mutex() = default;
	void lock() {
		bool expected = false;
		while (!flag.compare_exchange_strong(expected, true))
			expected = false;
	}

	bool islocked()
	{
		return flag.load();
	}

	void unlock() {
		flag.store(false);
	}
};