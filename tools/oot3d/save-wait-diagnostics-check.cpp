// SPDX-License-Identifier: GPL-2.0-or-later
#include <cassert>
#include <iostream>
#include "../../src/oot3d/save_wait_diagnostics.h"

int main() {
    using OOT3D::SaveWaitDiagnostics::Monitor;
    Monitor monitor;
    assert(!monitor.Poll(false, 0));
    unsigned samples = 0;
    for (Monitor::Time now = 0; now <= 60000000; now += 10000) {
        if (const auto sample = monitor.Poll(true, now)) {
            assert(!sample->exited);
            assert(sample->elapsed == Monitor::Delays[samples]);
            ++samples;
        }
    }
    assert(samples == 5);
    const auto exit = monitor.Poll(false, 60010000);
    assert(exit && exit->exited && exit->elapsed == 60010000);
    assert(!monitor.Poll(false, 60020000));
    assert(monitor.Poll(true, 70000000)->elapsed == 0);
    // Backwards time must reset sampling; a long frame coalesces missed samples.
    assert(monitor.Poll(true, 0)->elapsed == 0);
    assert(monitor.Poll(true, 40000000)->elapsed == 40000000);
    assert(!monitor.Poll(true, 40010000));
    assert(monitor.Poll(false, 40020000)->exited);
    assert(monitor.Poll(true, 40030000)->elapsed == 0);
    std::cout << "Save-wait diagnostics: bounded reports, exit, reentry and time rollback passed\n";
}
