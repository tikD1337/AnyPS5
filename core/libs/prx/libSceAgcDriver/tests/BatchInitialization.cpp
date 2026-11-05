#include "prx/libSceAgcDriver/Graphics/include/BatchInitialization.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>

using namespace AgcDriver::Graphics;

namespace {

using Action = BatchInitialization::Action;
using Outcome = BatchSubmission::Outcome;

void Expect(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "batch initialization: %s\n", message);
    std::abort();
}

void TestTransitions() {
    const auto queue = std::make_shared<BatchQueueEpoch>();
    const auto first = std::make_shared<BatchSubmission>(queue);
    const auto second = std::make_shared<BatchSubmission>(queue);
    BatchInitialization initialization;
    Expect(initialization.Plan(first) == Action::Record, "new allocation needs initialization");
    Expect(initialization.Plan(first) == Action::Record, "planning must not record a fill");
    Expect(initialization.Recorded(first), "recording initialization failed");
    Expect(initialization.Plan(first) == Action::Reuse, "one batch should initialize once");
    Expect(!initialization.Recorded(first), "duplicate initialization accepted");
    Expect(initialization.Plan(second) == Action::Replace, "another unsubmitted batch cannot reuse or clear the allocation");
    Expect(!initialization.Recorded(second), "unsubmitted owner replaced without replacing allocation");
    Expect(first->Submitted(), "successful submission not recorded");
    Expect(!first->Discarded() && !first->Submitted(), "terminal submission outcome changed");
    Expect(initialization.Plan(second) == Action::Reuse, "submitted initialization on same queue not reused");
    const auto otherQueue = std::make_shared<BatchSubmission>(std::make_shared<BatchQueueEpoch>());
    Expect(initialization.Plan(otherQueue) == Action::Replace, "unproven cross-queue ordering accepted");
    Expect(initialization.Plan(first) == Action::Replace, "submitted batch accepted as a new recording target");
    Expect(initialization.Plan(nullptr) == Action::Replace, "missing batch accepted");

    BatchInitialization discardedInitialization;
    Expect(discardedInitialization.Recorded(second), "new fill not recorded");
    Expect(second->Discarded(), "discard outcome not recorded");
    Expect(!second->Submitted(), "discarded batch resurrected");
    Expect(discardedInitialization.Plan(second) == Action::Replace, "discarded target accepted");
    Expect(discardedInitialization.Plan(otherQueue) == Action::Record, "discarded initialization was treated as executed");
    Expect(discardedInitialization.Recorded(otherQueue), "retry after cancellation not recorded");
}

void TestOwnership() {
    const auto queue = std::make_shared<BatchQueueEpoch>();
    const auto next = std::make_shared<BatchSubmission>(queue);
    BatchInitialization pending;
    BatchInitialization submitted;
    BatchInitialization discarded;
    {
        const auto batch = std::make_shared<BatchSubmission>(queue);
        Expect(pending.Recorded(batch), "pending setup");
    }
    Expect(pending.Plan(next) == Action::Replace, "destroying a recording handle falsely proved submission");
    {
        const auto batch = std::make_shared<BatchSubmission>(queue);
        Expect(submitted.Recorded(batch), "submitted setup");
        Expect(batch->Submitted(), "submission setup");
    }
    Expect(submitted.Plan(next) == Action::Reuse, "completed batch token lifetime lost proof");
    {
        const auto batch = std::make_shared<BatchSubmission>(queue);
        Expect(discarded.Recorded(batch), "discard setup");
        Expect(batch->Discarded(), "discard setup outcome");
    }
    Expect(discarded.Plan(next) == Action::Record, "discard proof lost with handle lifetime");
    bool rejected = false;
    try {
        const BatchSubmission invalid(nullptr);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Expect(rejected, "unknown queue accepted");
}

void TestSequences() {
    const std::array queues{std::make_shared<BatchQueueEpoch>(), std::make_shared<BatchQueueEpoch>()};
    std::mt19937 random(0xba7c4);
    std::size_t transitions = 0;
    for (unsigned sequence = 0; sequence < 2000; ++sequence) {
        std::vector<std::shared_ptr<BatchSubmission>> batches;
        std::vector<unsigned> queueIndices;
        std::vector<Outcome> states;
        for (unsigned i = 0; i < 12; ++i) {
            const auto queue = random() % queues.size();
            batches.push_back(std::make_shared<BatchSubmission>(queues[queue]));
            queueIndices.push_back(static_cast<unsigned>(queue));
            states.push_back(Outcome::Recording);
        }
        std::array<BatchInitialization, 8> allocations;
        std::array<int, 8> initializedBy;
        initializedBy.fill(-1);
        for (unsigned step = 0; step < 64; ++step) {
            const auto batch = random() % batches.size();
            const auto operation = random() % 4;
            if (operation <= 1) {
                const auto outcome = operation == 0 ? Outcome::Submitted : Outcome::Discarded;
                const bool expected = states[batch] == Outcome::Recording;
                const bool actual = operation == 0 ? batches[batch]->Submitted() : batches[batch]->Discarded();
                Expect(actual == expected, "terminal transition differs from reference");
                if (expected) states[batch] = outcome;
            } else {
                const auto allocation = random() % allocations.size();
                const int owner = initializedBy[allocation];
                auto expected = Action::Replace;
                if (states[batch] == Outcome::Recording) {
                    if (owner < 0 || states[owner] == Outcome::Discarded) expected = Action::Record;
                    else if (owner == static_cast<int>(batch) ||
                             (states[owner] == Outcome::Submitted && queueIndices[owner] == queueIndices[batch])) expected = Action::Reuse;
                }
                Expect(allocations[allocation].Plan(batches[batch]) == expected, "reuse proof differs from reference");
                if (operation == 3) {
                    const bool recorded = allocations[allocation].Recorded(batches[batch]);
                    Expect(recorded == (expected == Action::Record), "record differs from reference");
                    if (recorded) initializedBy[allocation] = static_cast<int>(batch);
                }
            }
            ++transitions;
        }
    }
    std::printf("Initialization transitions: %zu\n", transitions);
}

} // namespace

int main() {
    TestTransitions();
    TestOwnership();
    TestSequences();
    std::puts("Batch initialization model passed (no Vulkan integration tested)");
}
