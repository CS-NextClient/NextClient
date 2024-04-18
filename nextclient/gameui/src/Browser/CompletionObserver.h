#pragma once

#include <vector>
#include <functional>

class CompletionObserver {
	std::vector<bool> results_;
	using callback_type = std::function<void(std::vector<bool>&)>;

	callback_type callback_;
	size_t num_workers_;
public:
	CompletionObserver(size_t num_workers, callback_type callback);

	void Commit(bool result);
};