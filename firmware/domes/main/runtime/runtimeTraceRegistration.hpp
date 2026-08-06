#pragma once

#include "infra/taskTopology.hpp"
#include "trace/traceRecorder.hpp"

namespace domes::runtime {

inline size_t registerRuntimeTraceTasks() {
    size_t registered = 0;
    // Register every task that is present in this runtime, including tasks whose
    // profile presence is conditional. xTaskGetHandle naturally skips disabled
    // services while retaining their stable IDs when a feature starts them.
    for (const auto& expected : runtime_profile::kTraceTasks) {
        const auto& config = *expected.config;
        // These tasks can self-delete after creation. They register themselves
        // from their entry functions, where the current handle is owned and
        // cannot become a dangling name-lookup result.
        if (&config == &infra::task::kOtaCheck || &config == &infra::task::kTraceStream) {
            continue;
        }
        TaskHandle_t handle = xTaskGetHandle(config.name);
        if (handle != nullptr &&
            trace::Recorder::registerTask(handle, config.name, config.traceId, config.priority,
                                          config.coreAffinity)) {
            ++registered;
        }
    }
    return registered;
}

}  // namespace domes::runtime
