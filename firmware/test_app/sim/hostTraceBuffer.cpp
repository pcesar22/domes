#include "trace/traceBuffer.hpp"
#include <vector>

namespace sim {
// Global collector for Perfetto export
std::vector<domes::trace::TraceEvent>& globalTraceEvents() {
    static std::vector<domes::trace::TraceEvent> events;
    return events;
}
}  // namespace sim

namespace domes::trace {

// Internal storage
namespace {
struct BufferState {
    std::vector<TraceEvent> events;
    size_t readIndex = 0;
    bool initialized = false;
    bool paused = false;
    uint32_t dropped = 0;
};

BufferState& state() {
    static BufferState s;
    return s;
}
}  // anonymous namespace

TraceBuffer::TraceBuffer(size_t bufferSize)
    : ringBuf_(nullptr), bufferSize_(bufferSize), initialized_(false), paused_(false), droppedCount_(0) {}

TraceBuffer::~TraceBuffer() {
    initialized_.store(false);
}

esp_err_t TraceBuffer::init() {
    if (initialized_.load()) return ESP_ERR_INVALID_STATE;
    state().events.clear();
    state().readIndex = 0;
    state().initialized = true;
    initialized_.store(true);
    return ESP_OK;
}

bool TraceBuffer::record(const TraceEvent& event) {
    if (!initialized_.load() || paused_.load()) return false;
    state().events.push_back(event);
    sim::globalTraceEvents().push_back(event);
    return true;
}

bool TraceBuffer::recordFromIsr(const TraceEvent& event) {
    return record(event);  // No ISR distinction on host
}

bool TraceBuffer::read(TraceEvent* event, uint32_t) {
    if (!initialized_.load() || event == nullptr) return false;
    auto& s = state();
    if (s.readIndex >= s.events.size()) return false;
    *event = s.events[s.readIndex++];
    return true;
}

size_t TraceBuffer::count() const {
    if (!initialized_.load()) return 0;
    return state().events.size() - state().readIndex;
}

void TraceBuffer::clear() {
    if (!initialized_.load()) return;
    state().events.clear();
    state().readIndex = 0;
    droppedCount_.store(0);
}

}  // namespace domes::trace
