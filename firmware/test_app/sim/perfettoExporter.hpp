#pragma once

#include "trace/traceEvent.hpp"
#include "sim/simLog.hpp"
#include "sim/simEspNowBus.hpp"
#include "sim/simProtocol.hpp"

#include <string>
#include <vector>
#include <sstream>
#include <fstream>

namespace sim {

// Declared in hostTraceBuffer.cpp
std::vector<domes::trace::TraceEvent>& globalTraceEvents();

class PerfettoExporter {
public:
    static std::string exportJson(
        const std::vector<domes::trace::TraceEvent>& traceEvents,
        const SimLog& simLog,
        const std::vector<FlowEvent>& flowEvents,
        size_t podCount) {

        std::ostringstream json;
        json << "{\"traceEvents\":[";
        bool first = true;

        auto comma = [&]() { if (!first) json << ","; first = false; };

        // Process metadata: name each pod
        for (size_t i = 0; i < podCount; i++) {
            comma();
            json << "{\"ph\":\"M\",\"pid\":" << i << ",\"name\":\"process_name\","
                 << "\"args\":{\"name\":\"Pod " << i << "\"}}";
        }

        // Firmware trace events -> B/E/i/C
        for (const auto& event : traceEvents) {
            comma();
            char ph;
            auto type = event.type();
            switch (type) {
                case domes::trace::EventType::kSpanBegin: ph = 'B'; break;
                case domes::trace::EventType::kSpanEnd:   ph = 'E'; break;
                case domes::trace::EventType::kInstant:   ph = 'i'; break;
                case domes::trace::EventType::kCounter:   ph = 'C'; break;
                default: continue;
            }

            json << "{\"ph\":\"" << ph << "\",\"pid\":" << event.taskId
                 << ",\"tid\":" << static_cast<int>(event.category())
                 << ",\"ts\":" << event.timestamp
                 << ",\"name\":\"trace_" << event.arg1 << "\"";

            if (ph == 'C') {
                json << ",\"args\":{\"value\":" << event.arg2 << "}";
            }
            if (ph == 'i') {
                json << ",\"s\":\"t\"";
            }
            json << "}";
        }

        // SimLog entries -> instant events
        for (const auto& entry : simLog.entries()) {
            comma();
            std::string escaped = escapeJson(entry.message);
            json << "{\"ph\":\"i\",\"pid\":" << entry.podId
                 << ",\"tid\":100"
                 << ",\"ts\":" << entry.timestampUs
                 << ",\"name\":\"" << entry.category << "\""
                 << ",\"s\":\"t\""
                 << ",\"args\":{\"msg\":\"" << escaped << "\"}}";
        }

        // Flow events (ESP-NOW messages) -> s/f arrows
        for (const auto& flow : flowEvents) {
            comma();
            json << "{\"ph\":\"s\",\"pid\":" << flow.srcPod
                 << ",\"tid\":200"
                 << ",\"ts\":" << flow.timestampUs
                 << ",\"name\":\"" << messageTypeName(flow.type) << "\""
                 << ",\"id\":" << flow.sequence << "}";
            comma();
            json << "{\"ph\":\"f\",\"pid\":" << flow.dstPod
                 << ",\"tid\":200"
                 << ",\"ts\":" << flow.timestampUs
                 << ",\"name\":\"" << messageTypeName(flow.type) << "\""
                 << ",\"id\":" << flow.sequence
                 << ",\"bp\":\"e\"}";
        }

        json << "]}";
        return json.str();
    }

    static bool exportToFile(const std::string& path,
                              const std::vector<domes::trace::TraceEvent>& traceEvents,
                              const SimLog& simLog,
                              const std::vector<FlowEvent>& flowEvents,
                              size_t podCount) {
        std::string content = exportJson(traceEvents, simLog, flowEvents, podCount);
        std::ofstream file(path);
        if (!file.is_open()) return false;
        file << content;
        return true;
    }

private:
    static std::string escapeJson(const std::string& s) {
        std::string result;
        for (char c : s) {
            switch (c) {
                case '"':  result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\n': result += "\\n"; break;
                default:   result += c;
            }
        }
        return result;
    }
};

}  // namespace sim
