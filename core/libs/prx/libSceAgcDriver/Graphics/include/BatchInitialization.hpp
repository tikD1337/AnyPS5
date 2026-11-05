#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_BATCHINITIALIZATION_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_BATCHINITIALIZATION_HPP

#include <atomic>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <utility>

namespace AgcDriver::Graphics {

struct BatchQueueEpoch final {};

class BatchSubmission final {
public:
    enum class Outcome : std::uint8_t { Recording, Submitted, Discarded };

    explicit BatchSubmission(std::shared_ptr<const BatchQueueEpoch> queue) : queue(std::move(queue)) {
        if (this->queue == nullptr) throw std::invalid_argument("batch submission requires a queue epoch");
    }

    Outcome State() const { return outcome.load(std::memory_order_acquire); }
    const std::shared_ptr<const BatchQueueEpoch>& Queue() const { return queue; }

    bool Submitted() { return finish(Outcome::Submitted); }
    bool Discarded() { return finish(Outcome::Discarded); }

private:
    bool finish(Outcome finalState) {
        auto expected = Outcome::Recording;
        return outcome.compare_exchange_strong(expected, finalState, std::memory_order_acq_rel);
    }

    std::shared_ptr<const BatchQueueEpoch> queue;
    std::atomic<Outcome> outcome{Outcome::Recording};
};

class BatchInitialization final {
public:
    enum class Action : std::uint8_t { Record, Reuse, Replace };

    BatchInitialization() = default;
    BatchInitialization(const BatchInitialization&) = delete;
    BatchInitialization& operator=(const BatchInitialization&) = delete;

    Action Plan(const std::shared_ptr<const BatchSubmission>& current) const {
        if (current == nullptr || current->State() != BatchSubmission::Outcome::Recording) return Action::Replace;
        if (owner == nullptr || owner->State() == BatchSubmission::Outcome::Discarded) return Action::Record;
        if (owner == current) return Action::Reuse;
        if (owner->State() == BatchSubmission::Outcome::Submitted && owner->Queue() == current->Queue()) return Action::Reuse;
        return Action::Replace;
    }

    bool Recorded(const std::shared_ptr<const BatchSubmission>& batch) {
        if (Plan(batch) != Action::Record) return false;
        owner = batch;
        return true;
    }

private:
    std::shared_ptr<const BatchSubmission> owner;
};

} // namespace AgcDriver::Graphics

#endif
