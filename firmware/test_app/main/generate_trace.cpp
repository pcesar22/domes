/**
 * @file generate_trace.cpp
 * @brief Standalone trace generator for Perfetto visualization
 *
 * Runs a realistic 5-pod, 15-round drill session and exports a
 * Chrome Trace Event Format JSON file for viewing in https://ui.perfetto.dev
 *
 * Build: cmake .. -DBUILD_TRACE_GENERATOR=ON && make trace_generator
 * Run:   ./trace_generator [output.json]
 */

#include "esp_timer.h"
#include "sim/drillOrchestrator.hpp"
#include "sim/perfettoExporter.hpp"
#include "sim/simEspNowBus.hpp"
#include "sim/simOrchestrator.hpp"
#include "sim/podCommandHandler.hpp"
#include "trace/traceRecorder.hpp"
#include "trace/traceApi.hpp"

#include <cstdio>
#include <string>

using namespace sim;
using namespace domes::game;
using namespace domes::config;
using namespace domes::trace;

int main(int argc, char* argv[]) {
    std::string outputPath = "sim_trace.json";
    if (argc > 1) outputPath = argv[1];

    // Reset mock time
    test_stubs::mock_time_us.store(0);
    sim::globalTraceEvents().clear();

    // Initialize trace recorder for firmware TRACE_* events
    Recorder::init();
    Recorder::setEnabled(true);
    Recorder::registerTask(xTaskGetCurrentTaskHandle(), "sim");

    // --- Set up 5-pod environment ---
    constexpr size_t kNumPods = 5;
    SimOrchestrator sim;
    auto bus = std::make_unique<SimEspNowBus>(sim.log());
    std::vector<std::unique_ptr<PodCommandHandler>> handlers;

    for (size_t i = 0; i < kNumPods; i++) {
        auto& pod = sim.addPod(static_cast<uint16_t>(i));
        handlers.push_back(std::make_unique<PodCommandHandler>(pod, *bus, sim.log()));
        auto* handler = handlers.back().get();
        bus->registerPod(pod.podId(), [handler](const SimMessage& msg) {
            handler->onMessage(msg);
        });
    }

    DrillOrchestrator drill(sim, *bus, sim.log());

    // --- Define 15-round drill program ---
    // Realistic drill: varying targets, delays, timeouts, mix of hits and misses
    struct Round {
        uint16_t target;
        uint32_t delayMs;
        uint32_t touchMs;  // 0 = miss
        uint8_t pad;
        uint8_t r, g, b;
    };

    Round rounds[] = {
        // Phase 1: Warm-up (easy, long timeout, all hits)
        {1, 500,  200, 0, 0, 255, 0},     // Pod 1, green, hit at 200ms
        {2, 600,  180, 0, 0, 255, 0},     // Pod 2, green, hit at 180ms
        {3, 550,  250, 1, 0, 255, 0},     // Pod 3, green, hit at 250ms
        {4, 700,  150, 0, 0, 255, 0},     // Pod 4, green, hit at 150ms
        {0, 400,  120, 0, 0, 255, 0},     // Pod 0 (master), hit at 120ms

        // Phase 2: Speed round (shorter delays, some misses)
        {2, 300,  100, 0, 255, 255, 0},   // Pod 2, yellow, hit at 100ms
        {4, 250,    0, 0, 255, 255, 0},   // Pod 4, yellow, MISS
        {1, 200,   80, 2, 255, 255, 0},   // Pod 1, yellow, hit at 80ms (pad 2)
        {3, 350,    0, 0, 255, 255, 0},   // Pod 3, yellow, MISS
        {0, 300,   90, 0, 255, 255, 0},   // Pod 0 (master), hit at 90ms

        // Phase 3: Sprint (rapid fire)
        {1, 150,   60, 0, 255, 0, 0},     // Pod 1, red, hit at 60ms
        {2, 100,   55, 1, 255, 0, 0},     // Pod 2, red, hit at 55ms
        {3, 120,    0, 0, 255, 0, 0},     // Pod 3, red, MISS
        {4, 100,   45, 0, 255, 0, 0},     // Pod 4, red, hit at 45ms
        {0, 150,   70, 3, 255, 0, 0},     // Pod 0 (master), hit at 70ms (pad 3)
    };

    constexpr size_t kNumRounds = sizeof(rounds) / sizeof(rounds[0]);

    std::vector<DrillStep> steps;
    std::vector<TouchScenario> touches;

    for (size_t i = 0; i < kNumRounds; i++) {
        const auto& r = rounds[i];
        steps.push_back({r.target, r.delayMs, 3000, 0x03,
                         domes::Color::rgb(r.r, r.g, r.b)});
        touches.push_back({r.target, r.touchMs, r.pad});
    }

    // --- Execute drill ---
    TRACE_BEGIN(TRACE_ID("Drill.Execute"), Category::kGame);
    DrillResult result = drill.execute(steps, touches);
    TRACE_END(TRACE_ID("Drill.Execute"), Category::kGame);

    // --- Print results ---
    printf("=== Drill Results ===\n");
    printf("Rounds:    %zu\n", result.rounds.size());
    printf("Hits:      %zu\n", result.hitCount());
    printf("Misses:    %zu\n", result.missCount());
    printf("Avg react: %u us (%.1f ms)\n", result.avgReactionUs(),
           result.avgReactionUs() / 1000.0);
    printf("Total:     %llu us (%.1f ms)\n",
           static_cast<unsigned long long>(result.totalTimeUs),
           result.totalTimeUs / 1000.0);
    printf("\nPer-round:\n");
    for (size_t i = 0; i < result.rounds.size(); i++) {
        const auto& r = result.rounds[i];
        printf("  [%2zu] Pod %u: %s", i, r.targetPodId,
               r.hit ? "HIT " : "MISS");
        if (r.hit) {
            printf(" react=%u us (%.1f ms) pad=%u",
                   r.reactionTimeUs, r.reactionTimeUs / 1000.0, r.padIndex);
        }
        printf("\n");
    }

    // --- Export Perfetto trace ---
    printf("\n--- Trace Stats ---\n");
    printf("Trace events: %zu\n", sim::globalTraceEvents().size());
    printf("SimLog entries: %zu\n", sim.log().entries().size());
    printf("Flow events: %zu\n", bus->flowEvents().size());

    bool ok = PerfettoExporter::exportToFile(
        outputPath, sim::globalTraceEvents(), sim.log(),
        bus->flowEvents(), sim.podCount());

    if (ok) {
        printf("\nPerfetto trace written to: %s\n", outputPath.c_str());
        printf("Open in https://ui.perfetto.dev to visualize\n");
    } else {
        fprintf(stderr, "ERROR: Failed to write trace to %s\n", outputPath.c_str());
        return 1;
    }

    // Clean up
    Recorder::shutdown();
    return 0;
}
