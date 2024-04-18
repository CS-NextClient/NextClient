#include "CompletionObserver.h"

CompletionObserver::CompletionObserver(
	size_t num_workers, 
	CompletionObserver::callback_type callback
): callback_(callback), num_workers_(num_workers) { }

void CompletionObserver::Commit(bool result) {
	results_.push_back(result);

	if(results_.size() >= num_workers_) {
		callback_(results_);
		delete this;
	}
}