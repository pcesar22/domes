/**
 * @file trace_stubs.cpp
 * @brief No-op trace recorder stubs for host unit tests
 *
 * gameEngine.cpp includes traceApi.hpp which calls Recorder methods.
 * On the host, tracing is a no-op.
 */

#include "trace/traceRecorder.hpp"

namespace domes::trace {

// TraceBuffer stub
TraceBuffer::TraceBuffer(size_t) : ringBuf_(nullptr), bufferSize_(0), initialized_(false), paused_(false), droppedCount_(0) {}
TraceBuffer::~TraceBuffer() = default;

// Static member definitions
std::unique_ptr<TraceBuffer> Recorder::buffer_;
std::atomic<bool> Recorder::enabled_{false};
std::atomic<bool> Recorder::initialized_{false};
std::array<TaskNameEntry, kMaxRegisteredTasks> Recorder::taskNames_{};
size_t Recorder::taskNameCount_{0};

bool Recorder::isEnabled() { return false; }
bool Recorder::isInitialized() { return false; }
void Recorder::record(const TraceEvent&) {}
void Recorder::recordFromIsr(const TraceEvent&) {}
void Recorder::setEnabled(bool) {}

}  // namespace domes::trace
