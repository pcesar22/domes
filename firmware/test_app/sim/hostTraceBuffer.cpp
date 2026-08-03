#include "trace/traceBuffer.hpp"

#include <deque>
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
    std::deque<TraceEvent> events;
    size_t acquiredCount = 0;
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
    : ringBuf_(nullptr),
      bufferSize_(bufferSize),
      initialized_(false),
      paused_(false),
      droppedCount_(0),
      eventCount_(0) {}

TraceBuffer::~TraceBuffer() {
    initialized_.store(false);
}

esp_err_t TraceBuffer::init() {
    if (initialized_.load())
        return ESP_ERR_INVALID_STATE;
    state().events.clear();
    state().acquiredCount = 0;
    state().initialized = true;
    initialized_.store(true);
    return ESP_OK;
}

bool TraceBuffer::record(const TraceEvent& event) {
    if (!initialized_.load() || paused_.load())
        return false;
    state().events.push_back(event);
    sim::globalTraceEvents().push_back(event);
    return true;
}

bool TraceBuffer::recordFromIsr(const TraceEvent& event) {
    return record(event);  // No ISR distinction on host
}

bool TraceBuffer::read(TraceEvent* event, uint32_t) {
    if (event == nullptr)
        return false;

    const TraceEvent* acquired = acquire(0);
    if (acquired == nullptr)
        return false;
    *event = *acquired;
    release(acquired);
    return true;
}

const TraceEvent* TraceBuffer::acquire(uint32_t) {
    if (!initialized_.load())
        return nullptr;
    auto& s = state();
    if (s.acquiredCount >= s.events.size())
        return nullptr;
    return &s.events[s.acquiredCount++];
}

void TraceBuffer::release(const TraceEvent* event) {
    auto& s = state();
    if (!initialized_.load() || event == nullptr || s.acquiredCount == 0 || s.events.empty() ||
        event != &s.events.front()) {
        return;
    }
    s.events.pop_front();
    s.acquiredCount--;
}

size_t TraceBuffer::count() const {
    if (!initialized_.load())
        return 0;
    return state().events.size();
}

void TraceBuffer::clear() {
    if (!initialized_.load())
        return;
    state().events.clear();
    state().acquiredCount = 0;
    droppedCount_.store(0);
}

void TraceBuffer::pause() {
    paused_.store(true);
    state().paused = true;
}

}  // namespace domes::trace
