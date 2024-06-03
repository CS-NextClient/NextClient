#pragma once

#include <vector>
#include <functional>
#include <mutex>

class CompletionObserver
{
	std::mutex mutex_;
	std::vector<bool> results_;
	using callback_type = std::function<void(std::vector<bool>)>;

	callback_type callback_;
	bool calback_invoked_;
	size_t num_workers_;

public:
	CompletionObserver(size_t num_workers, callback_type callback);
	~CompletionObserver() {
		int a = 1;

	}

	void Commit(bool result);
};
