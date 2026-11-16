// SPDX-License-Identifier: GPL-2.0-or-later
#include "common/atomic_shared_ptr.h"
#include <array>
#include <cassert>
#include <iostream>
#include <thread>
#include <type_traits>
#include <vector>

int main() {
    using Slot = Common::AtomicSharedPtr<const unsigned>;
    static_assert(!std::is_copy_constructible_v<Slot>);
    static_assert(!std::is_copy_assignable_v<Slot>);
    std::array<Slot, 3> slots{};
    for (const auto& slot : slots) assert(!slot.load());

    // Clearing publication must not invalidate a snapshot already held by a reader.
    auto original = std::make_shared<const unsigned>(42);
    std::weak_ptr<const unsigned> lifetime = original;
    slots[0].store(std::move(original));
    auto snapshot = slots[0].load();
    slots[0].store({});
    assert(!slots[0].load() && !lifetime.expired() && *snapshot == 42);
    snapshot.reset();
    assert(lifetime.expired());

    // A destructor may access the same slot: replacement must release it outside
    // the internal lock, matching the standard shared-pointer atomic semantics.
    bool destroyed = false;
    slots[0].store(std::shared_ptr<const unsigned>(new unsigned(7), [&](const unsigned* value) {
        assert(!slots[0].load());
        destroyed = true;
        delete value;
    }));
    slots[0].store({});
    assert(destroyed);

    slots[1].store(std::make_shared<const unsigned>(9));
    const auto exchanged = slots[1].exchange(std::make_shared<const unsigned>(10));
    assert(exchanged && *exchanged == 9 && *slots[1].load() == 10);
    assert(*slots[1].exchange({}) == 10 && !slots[1].load());

    struct Snapshot { unsigned sequence, inverse; };
    Common::AtomicSharedPtr<const Snapshot> publication;
    std::atomic<bool> begin{false};
    std::atomic<unsigned> writers{2};
    std::vector<std::thread> threads;
    for (unsigned writer = 0; writer < 2; ++writer) {
        threads.emplace_back([&, writer] {
            while (!begin.load()) std::this_thread::yield();
            for (unsigned i = 1; i <= 10000; ++i) {
                const unsigned sequence = i + writer * 10000;
                publication.store(std::make_shared<const Snapshot>(Snapshot{sequence, ~sequence}));
                if (i % 7 == 0) publication.store({});
            }
            --writers;
        });
    }
    for (unsigned reader = 0; reader < 3; ++reader) {
        threads.emplace_back([&] {
            while (!begin.load()) std::this_thread::yield();
            do {
                const auto current = publication.load();
                if (current) assert(current->inverse == ~current->sequence);
            } while (writers.load());
        });
    }
    begin = true;
    for (auto& thread : threads) thread.join();
    publication.store({});
    assert(!publication.load());
    std::cout << "Atomic shared-pointer publication and lifetime checks passed\n";
}
