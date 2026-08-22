//! Trace/perfetto commands
//!
//! Uses protobuf-encoded messages for all control/metadata (prost).
//! TraceEvent data is 16-byte binary carried in protobuf 'bytes' fields.

use crate::proto::trace::{
    AckResponse, Category, EventType, MsgType as TraceMsgType, Status as TraceStatus, StreamBatch,
    TraceDataChunk, TraceDumpComplete, TraceSessionInfo, TraceStatusResponse,
};
use crate::transport::frame::Frame;
use crate::transport::Transport;
use anyhow::{Context, Result};
use prost::Message;
use sha2::{Digest, Sha256};
use std::collections::{HashMap, HashSet};
use std::fs::File;
use std::io::Write;
use std::path::Path;

/// Compact trace event (16 bytes, binary)
#[derive(Debug, Clone, Copy)]
struct TraceEvent {
    timestamp: u32,
    task_id: u16,
    event_type: u8,
    flags: u8,
    arg1: u32,
    arg2: u32,
}

const TRACE_EVENT_SIZE: usize = 16;
const MAX_TRACE_DUMP_BYTES: usize = 32 * 1024;
const TRACE_EVENT_FORMAT_VERSION: u32 = 1;
const ESP_IMAGE_HEADER_SIZE: usize = 24;
const ESP_SEGMENT_HEADER_SIZE: usize = 8;
const ESP_APP_DESC_SIZE: usize = 256;
const ESP_APP_DESC_MAGIC: u32 = 0xABCD_5432;

pub struct TraceEvidenceContext<'a> {
    pub device_name: &'a str,
    pub transport_type: &'a str,
    pub address: &'a str,
}

#[derive(Debug)]
struct CandidateImage {
    path: std::path::PathBuf,
    file_sha256: [u8; 32],
    app_image_sha256: [u8; 32],
    app_elf_sha256: [u8; 32],
    firmware_version: String,
}

fn fixed_c_string(bytes: &[u8], field: &str) -> Result<String> {
    let end = bytes
        .iter()
        .position(|byte| *byte == 0)
        .context(format!("Candidate app image {field} is not NUL terminated"))?;
    let value = std::str::from_utf8(&bytes[..end])
        .with_context(|| format!("Candidate app image {field} is not UTF-8"))?;
    if value.is_empty() {
        anyhow::bail!("Candidate app image {field} is empty");
    }
    Ok(value.to_string())
}

fn inspect_candidate_image(path: &Path) -> Result<CandidateImage> {
    let bytes = std::fs::read(path)
        .with_context(|| format!("Failed to read candidate app image {}", path.display()))?;
    let minimum_size = ESP_IMAGE_HEADER_SIZE + ESP_SEGMENT_HEADER_SIZE + ESP_APP_DESC_SIZE + 32;
    if bytes.len() < minimum_size || bytes[0] != 0xE9 || bytes[1] == 0 || bytes[1] > 16 {
        anyhow::bail!("Candidate file is not a bounded ESP application image");
    }
    if bytes[23] != 1 {
        anyhow::bail!("Candidate ESP application image has no appended SHA-256");
    }

    let segment_data = ESP_IMAGE_HEADER_SIZE + ESP_SEGMENT_HEADER_SIZE;
    let first_segment_size = u32::from_le_bytes(
        bytes[ESP_IMAGE_HEADER_SIZE + 4..segment_data]
            .try_into()
            .expect("fixed ESP segment size"),
    ) as usize;
    let first_segment_end = segment_data
        .checked_add(first_segment_size)
        .context("Candidate ESP application image first segment overflows")?;
    if first_segment_size < ESP_APP_DESC_SIZE || first_segment_end > bytes.len() {
        anyhow::bail!("Candidate ESP application image has an invalid first segment");
    }
    if u32::from_le_bytes(
        bytes[segment_data..segment_data + 4]
            .try_into()
            .expect("fixed app descriptor magic"),
    ) != ESP_APP_DESC_MAGIC
    {
        anyhow::bail!("Candidate ESP application image has no valid app descriptor");
    }

    let firmware_version = fixed_c_string(
        &bytes[segment_data + 16..segment_data + 48],
        "firmware version",
    )?;
    let mut app_elf_sha256 = [0u8; 32];
    app_elf_sha256.copy_from_slice(&bytes[segment_data + 144..segment_data + 176]);

    let appended_hash_offset = bytes.len() - 32;
    let calculated_image_hash = Sha256::digest(&bytes[..appended_hash_offset]);
    if calculated_image_hash.as_slice() != &bytes[appended_hash_offset..] {
        anyhow::bail!("Candidate ESP application image has an invalid appended SHA-256");
    }
    let mut app_image_sha256 = [0u8; 32];
    app_image_sha256.copy_from_slice(&bytes[appended_hash_offset..]);
    let mut file_sha256 = [0u8; 32];
    file_sha256.copy_from_slice(&Sha256::digest(&bytes));

    Ok(CandidateImage {
        path: path.to_path_buf(),
        file_sha256,
        app_image_sha256,
        app_elf_sha256,
        firmware_version,
    })
}

fn validate_evidence_identity(session: &TraceSessionInfo) -> Result<()> {
    if session.firmware_version.is_empty() || session.firmware_version.len() >= 32 {
        anyhow::bail!("Trace session has an invalid firmware version");
    }
    for (name, digest) in [
        ("ELF", session.app_elf_sha256.as_slice()),
        ("app image", session.app_image_sha256.as_slice()),
    ] {
        if digest.len() != 32 || digest.iter().all(|byte| *byte == 0) {
            anyhow::bail!("Trace session has an invalid {name} SHA-256");
        }
    }
    if session.device_uid.len() != 6
        || session.device_uid.iter().all(|byte| *byte == 0)
        || session.device_uid.iter().all(|byte| *byte == 0xFF)
        || session.device_uid[0] & 0x01 != 0
    {
        anyhow::bail!("Trace session has an invalid factory base MAC");
    }
    Ok(())
}

fn validate_candidate_binding(session: &TraceSessionInfo, image: &CandidateImage) -> Result<()> {
    if image.firmware_version != session.firmware_version {
        anyhow::bail!(
            "Candidate firmware version '{}' does not match running version '{}'",
            image.firmware_version,
            session.firmware_version
        );
    }
    if image.app_elf_sha256.as_slice() != session.app_elf_sha256.as_slice() {
        anyhow::bail!("Candidate ELF SHA-256 does not match the running firmware");
    }
    if image.app_image_sha256.as_slice() != session.app_image_sha256.as_slice() {
        anyhow::bail!("Candidate app image SHA-256 does not match the running firmware");
    }
    Ok(())
}

fn trace_dump_byte_count(event_count: u32, buffer_size_bytes: u32) -> Result<usize> {
    let buffer_size =
        usize::try_from(buffer_size_bytes).context("Trace buffer size does not fit this host")?;
    if buffer_size == 0 || buffer_size > MAX_TRACE_DUMP_BYTES {
        anyhow::bail!(
            "Trace buffer size {} is outside the supported range 1..={}",
            buffer_size,
            MAX_TRACE_DUMP_BYTES
        );
    }
    let count = usize::try_from(event_count).context("Trace event count does not fit this host")?;
    let bytes = count
        .checked_mul(TRACE_EVENT_SIZE)
        .context("Trace event byte count overflow")?;
    if bytes == 0 || bytes > buffer_size {
        anyhow::bail!(
            "Trace session declares {} event bytes for a {}-byte buffer",
            bytes,
            buffer_size
        );
    }
    Ok(bytes)
}

fn decode_trace_events(event_bytes: &[u8]) -> Result<Vec<TraceEvent>> {
    if !event_bytes.len().is_multiple_of(TRACE_EVENT_SIZE) {
        anyhow::bail!(
            "Trace event payload has {} bytes; expected a multiple of {}",
            event_bytes.len(),
            TRACE_EVENT_SIZE
        );
    }

    Ok(event_bytes
        .chunks_exact(TRACE_EVENT_SIZE)
        .map(|bytes| TraceEvent {
            timestamp: u32::from_le_bytes(bytes[0..4].try_into().expect("fixed trace timestamp")),
            task_id: u16::from_le_bytes(bytes[4..6].try_into().expect("fixed trace task ID")),
            event_type: bytes[6],
            flags: bytes[7],
            arg1: u32::from_le_bytes(bytes[8..12].try_into().expect("fixed trace argument")),
            arg2: u32::from_le_bytes(bytes[12..16].try_into().expect("fixed trace argument")),
        })
        .collect())
}

fn validate_timestamp_order(events: &[TraceEvent]) -> Result<()> {
    let mut wraps = 0u8;
    for pair in events.windows(2) {
        let previous = pair[0].timestamp;
        let current = pair[1].timestamp;
        let delta = current.wrapping_sub(previous);
        if delta > i32::MAX as u32 {
            anyhow::bail!(
                "Trace timestamp regression: {} followed by {}",
                previous,
                current
            );
        }
        if current < previous {
            wraps += 1;
            if wraps > 1 {
                anyhow::bail!("Trace timestamps wrap more than once");
            }
        }
    }
    Ok(())
}

fn validate_session_binding(session: &TraceSessionInfo, events: &[TraceEvent]) -> Result<()> {
    validate_evidence_identity(session)?;
    let first = events.first().context("Trace session contains no events")?;
    let last = events.last().context("Trace session contains no events")?;
    if first.timestamp != session.start_timestamp_us || last.timestamp != session.end_timestamp_us {
        anyhow::bail!(
            "Trace session timestamp bounds do not match raw events: session {}..{}, raw {}..{}",
            session.start_timestamp_us,
            session.end_timestamp_us,
            first.timestamp,
            last.timestamp
        );
    }
    validate_timestamp_order(events)?;

    let mut task_catalog = HashMap::new();
    let mut task_names = HashSet::new();
    if session.tasks.len() > 16 {
        anyhow::bail!("Trace task catalog exceeds the firmware limit of 16 entries");
    }
    for task in &session.tasks {
        if task.task_id == 0
            || task.task_id > 31
            || task.name.is_empty()
            || task.name.len() >= 16
            || task.priority > u32::from(u8::MAX)
            || !task_names.insert(task.name.as_str())
        {
            anyhow::bail!("Invalid trace task catalog entry for ID {}", task.task_id);
        }
        if !matches!(task.core_affinity_mask, 1..=3) {
            anyhow::bail!(
                "Invalid core affinity mask {} for trace task {}",
                task.core_affinity_mask,
                task.task_id
            );
        }
        if task_catalog.insert(task.task_id, task).is_some() {
            anyhow::bail!("Duplicate trace task ID {}", task.task_id);
        }
    }

    let referenced_tasks: HashSet<u32> = events
        .iter()
        .filter_map(|event| (event.task_id != 0).then_some(u32::from(event.task_id)))
        .collect();
    let catalog_tasks: HashSet<u32> = task_catalog.keys().copied().collect();
    if referenced_tasks != catalog_tasks {
        anyhow::bail!(
            "Trace task catalog does not exactly match raw task references: catalog={:?}, raw={:?}",
            catalog_tasks,
            referenced_tasks
        );
    }

    let first_non_create = events
        .iter()
        .position(|event| event.event_type != 0x10)
        .unwrap_or(events.len());
    if events[first_non_create..]
        .iter()
        .any(|event| event.event_type == 0x10)
    {
        anyhow::bail!("Trace task-create preamble is not ordered first");
    }
    let mut task_creates = HashSet::new();
    for event in events.iter().filter(|event| event.event_type == 0x10) {
        let task_id = u32::from(event.task_id);
        let task = task_catalog
            .get(&task_id)
            .with_context(|| format!("Task-create event references unknown task {}", task_id))?;
        if !task_creates.insert(task_id) {
            anyhow::bail!("Duplicate task-create preamble for task {}", task_id);
        }
        if event.arg1 != task.priority || event.arg2 != task.core_affinity_mask {
            anyhow::bail!(
                "Task-create metadata mismatch for task {}: raw priority/affinity={}/{}, session={}/{}",
                task_id,
                event.arg1,
                event.arg2,
                task.priority,
                task.core_affinity_mask
            );
        }
    }
    if task_creates != catalog_tasks {
        anyhow::bail!(
            "Trace task-create preamble does not exactly match the session catalog: creates={:?}, catalog={:?}",
            task_creates,
            catalog_tasks
        );
    }

    let mut object_catalog = HashMap::new();
    let mut object_names = HashSet::new();
    if session.objects.len() > 10 {
        anyhow::bail!("Trace object catalog exceeds the firmware limit of 10 entries");
    }

    const ACCEPTANCE_OBJECTS: &[(u32, i32, &str)] = &[
        (1, 1, "probe_queue"),
        (2, 2, "probe_sem"),
        (3, 3, "probe_irq"),
        (4, 4, "probe_callback"),
        (5, 5, "probe_action"),
        (6, 7, "probe_timeout"),
        (0x59FB_7823, 1, "espnow_queue"),
        (0xEF2D_D8BB, 2, "espnow_ready"),
        (0x3580_0DA2, 4, "espnow_cb"),
        (0xF1F4_511E, 5, "espnow_done"),
    ];
    let has_acceptance_object = session.objects.iter().any(|object| {
        ACCEPTANCE_OBJECTS
            .iter()
            .any(|(_, _, expected_name)| object.name == *expected_name)
    });
    if has_acceptance_object {
        let expected_objects = match session.objects.len() {
            6 => &ACCEPTANCE_OBJECTS[..6],
            10 => ACCEPTANCE_OBJECTS,
            count => anyhow::bail!(
                "Trace acceptance object catalog must contain exactly 6 or 10 entries, got {}",
                count
            ),
        };
        for (expected_id, expected_kind, expected_name) in expected_objects {
            if !session.objects.iter().any(|object| {
                object.object_id == *expected_id
                    && object.kind == *expected_kind
                    && object.name == *expected_name
            }) {
                anyhow::bail!(
                    "Trace acceptance object mapping is missing or invalid for {}",
                    expected_name
                );
            }
        }
    }
    for object in &session.objects {
        if object.object_id == 0
            || !(1..=7).contains(&object.kind)
            || object.name.is_empty()
            || object.name.len() >= 16
            || !object_names.insert(object.name.as_str())
        {
            anyhow::bail!(
                "Invalid trace object catalog entry for ID {} (kind {})",
                object.object_id,
                object.kind
            );
        }
        if object_catalog
            .insert(object.object_id, object.kind)
            .is_some()
        {
            anyhow::bail!("Duplicate trace object ID {}", object.object_id);
        }
    }

    for event in events {
        let core = event.flags & 0x03;
        let context = (event.flags >> 2) & 0x03;
        let category = (event.flags >> 4) & 0x0F;
        if matches!(
            EventType::try_from(i32::from(event.event_type)),
            Ok(EventType::Unknown) | Err(_)
        ) || !matches!(core, 1 | 2)
            || context > 2
            || category > Category::Sync as u8
        {
            anyhow::bail!(
                "Trace event has invalid type/category/core/context at timestamp {}",
                event.timestamp
            );
        }
        if matches!(event.event_type, 0x10..=0x1E) && category != Category::Kernel as u8 {
            anyhow::bail!(
                "Scheduler event type 0x{:02X} does not use the kernel category",
                event.event_type
            );
        }
        let allowed_kinds: &[i32] = match event.event_type {
            0x0C | 0x0D if category == Category::Kernel as u8 => &[2],
            0x0C | 0x0D if category == Category::Sync as u8 => continue,
            0x0C | 0x0D => {
                anyhow::bail!(
                    "Semaphore event type 0x{:02X} uses incompatible category {}",
                    event.event_type,
                    category
                );
            }
            0x13 => &[1, 2, 6],
            0x16 | 0x17 => &[3],
            0x19 | 0x1A => &[1],
            0x1B => &[7],
            0x1C | 0x1D => &[4],
            0x1E => &[5],
            _ => continue,
        };
        let kind = object_catalog.get(&event.arg1).with_context(|| {
            format!(
                "Trace event type 0x{:02X} references unknown object {}",
                event.event_type, event.arg1
            )
        })?;
        if !allowed_kinds.contains(kind) {
            anyhow::bail!(
                "Trace event type 0x{:02X} references object {} with incompatible kind {}",
                event.event_type,
                event.arg1,
                kind
            );
        }
    }

    validate_esp_now_correlation(events, &object_catalog)?;

    Ok(())
}

fn validate_esp_now_correlation(events: &[TraceEvent], objects: &HashMap<u32, i32>) -> Result<()> {
    const QUEUE_ID: u32 = 0x59FB_7823;
    const READY_ID: u32 = 0xEF2D_D8BB;
    const CALLBACK_ID: u32 = 0x3580_0DA2;
    const COMPLETE_ID: u32 = 0xF1F4_511E;
    if objects.get(&QUEUE_ID) != Some(&1)
        || objects.get(&READY_ID) != Some(&2)
        || objects.get(&CALLBACK_ID) != Some(&4)
        || objects.get(&COMPLETE_ID) != Some(&5)
    {
        return Ok(());
    }

    let expected_object = |event_type: u8| match event_type {
        0x19 | 0x1A => Some(QUEUE_ID),
        0x0C | 0x0D => Some(READY_ID),
        0x1C | 0x1D => Some(CALLBACK_ID),
        0x1E => Some(COMPLETE_ID),
        _ => None,
    };
    let mut by_token: HashMap<u32, Vec<&TraceEvent>> = HashMap::new();
    for event in events {
        if expected_object(event.event_type) != Some(event.arg1) {
            continue;
        }
        if event.arg2 == 0 {
            anyhow::bail!("ESP-NOW causal event has a zero token");
        }
        by_token.entry(event.arg2).or_default().push(event);
    }

    for (token, correlated) in by_token {
        let mut submissions = Vec::new();
        let mut rx_callbacks = Vec::new();
        let mut tx_callbacks = Vec::new();
        let mut rx_completions = Vec::new();
        let mut tx_completions = Vec::new();
        let mut index = 0;
        while index < correlated.len() {
            let event = correlated[index];
            let context = (event.flags >> 2) & 0x03;
            if event.event_type == 0x19 && context == 0 {
                if event.task_id == 0 {
                    anyhow::bail!(
                        "ESP-NOW task boundary has no task ownership for token {}",
                        token
                    );
                }
                submissions.push(index);
                index += 1;
                continue;
            }
            if event.event_type == 0x1C {
                let Some(end) = correlated[index + 1..]
                    .iter()
                    .position(|candidate| candidate.event_type == 0x1D)
                    .map(|offset| index + 1 + offset)
                else {
                    anyhow::bail!("ESP-NOW callback chain is incomplete for token {}", token);
                };
                let types: Vec<u8> = correlated[index..=end]
                    .iter()
                    .map(|candidate| candidate.event_type)
                    .collect();
                if correlated[index..=end]
                    .iter()
                    .any(|candidate| ((candidate.flags >> 2) & 0x03) != 2 || candidate.task_id != 0)
                {
                    anyhow::bail!(
                        "ESP-NOW callback chain has invalid context or task ownership for token {}",
                        token
                    );
                }
                if types == [0x1C, 0x19, 0x0D, 0x1D] {
                    rx_callbacks.push(index);
                } else if types == [0x1C, 0x0D, 0x1D] {
                    tx_callbacks.push(index);
                } else {
                    anyhow::bail!("ESP-NOW callback chain is malformed for token {}", token);
                }
                index = end + 1;
                continue;
            }
            if event.event_type == 0x0C {
                let Some(end) = correlated[index + 1..]
                    .iter()
                    .position(|candidate| candidate.event_type == 0x1E)
                    .map(|offset| index + 1 + offset)
                else {
                    anyhow::bail!("ESP-NOW task chain is incomplete for token {}", token);
                };
                let types: Vec<u8> = correlated[index..=end]
                    .iter()
                    .map(|candidate| candidate.event_type)
                    .collect();
                if correlated[index..=end]
                    .iter()
                    .any(|candidate| ((candidate.flags >> 2) & 0x03) != 0 || candidate.task_id == 0)
                {
                    anyhow::bail!(
                        "ESP-NOW task chain has invalid context or task ownership for token {}",
                        token
                    );
                }
                if types == [0x0C, 0x1A, 0x1E] {
                    rx_completions.push(index);
                } else if types == [0x0C, 0x1E] {
                    tx_completions.push(index);
                } else {
                    anyhow::bail!("ESP-NOW task chain is malformed for token {}", token);
                }
                index = end + 1;
                continue;
            }
            anyhow::bail!(
                "ESP-NOW causal chain has an unexpected boundary for token {}",
                token
            );
        }
        if submissions.len() != tx_callbacks.len()
            || tx_callbacks.len() != tx_completions.len()
            || rx_callbacks.len() != rx_completions.len()
            || submissions
                .iter()
                .zip(&tx_callbacks)
                .zip(&tx_completions)
                .any(|((start, callback), complete)| !(start < callback && callback < complete))
            || rx_callbacks
                .iter()
                .zip(&rx_completions)
                .any(|(callback, complete)| callback >= complete)
        {
            anyhow::bail!("ESP-NOW RX/TX correlation chain is incomplete or reordered");
        }
    }
    Ok(())
}

#[derive(Debug, Default)]
struct TraceDumpIntegrity {
    next_offset: u32,
    checksum: u32,
}

impl TraceDumpIntegrity {
    fn accept_chunk(&mut self, chunk: &TraceDataChunk) -> Result<()> {
        if chunk.offset != self.next_offset {
            anyhow::bail!(
                "Trace chunk offset mismatch: expected {}, got {}",
                self.next_offset,
                chunk.offset
            );
        }

        if chunk.events.is_empty() {
            anyhow::bail!("Trace chunk at offset {} is empty", chunk.offset);
        }
        if !chunk.events.len().is_multiple_of(TRACE_EVENT_SIZE) {
            anyhow::bail!(
                "Trace event payload has {} bytes; expected a multiple of {}",
                chunk.events.len(),
                TRACE_EVENT_SIZE
            );
        }
        let event_count = u32::try_from(chunk.events.len() / TRACE_EVENT_SIZE)
            .context("Trace chunk event count overflow")?;
        if chunk.count != event_count {
            anyhow::bail!(
                "Trace chunk count mismatch at offset {}: declared {}, decoded {}",
                chunk.offset,
                chunk.count,
                event_count
            );
        }

        self.next_offset = self
            .next_offset
            .checked_add(event_count)
            .context("Trace event offset overflow")?;
        for byte in &chunk.events {
            self.checksum = self.checksum.wrapping_add(u32::from(*byte));
        }

        Ok(())
    }

    fn finish(&self, complete: &TraceDumpComplete, session_event_count: u32) -> Result<()> {
        if complete.total_events != self.next_offset {
            anyhow::bail!(
                "Trace dump total mismatch: end marker declares {}, received {}",
                complete.total_events,
                self.next_offset
            );
        }
        if session_event_count != self.next_offset {
            anyhow::bail!(
                "Trace dump session mismatch: session declared {}, received {}",
                session_event_count,
                self.next_offset
            );
        }
        if complete.checksum != self.checksum {
            anyhow::bail!(
                "Trace dump checksum mismatch: expected 0x{:08X}, calculated 0x{:08X}",
                complete.checksum,
                self.checksum
            );
        }

        Ok(())
    }
}

#[derive(Debug, Default)]
struct StreamSequenceTracker {
    next_sequence: u32,
}

impl StreamSequenceTracker {
    fn accept(&mut self, sequence: u32) -> Result<()> {
        if sequence != self.next_sequence {
            anyhow::bail!(
                "Trace stream sequence gap: expected {}, got {}",
                self.next_sequence,
                sequence
            );
        }

        self.next_sequence = sequence.wrapping_add(1);
        Ok(())
    }
}

/// Trace status information
#[derive(Debug)]
pub struct TraceStatusInfo {
    pub initialized: bool,
    pub enabled: bool,
    pub streaming: bool,
    pub event_count: u32,
    pub dropped_count: u32,
    pub discontinuity_count: u32,
    pub buffer_size: u32,
}

/// Helper to decode a protobuf AckResponse and check status
fn decode_ack(payload: &[u8]) -> Result<TraceStatus> {
    let ack = AckResponse::decode(payload).context("Failed to decode AckResponse")?;
    TraceStatus::try_from(ack.status).map_err(|_| anyhow::anyhow!("Unknown status: {}", ack.status))
}

/// Start tracing
pub fn trace_start(transport: &mut dyn Transport) -> Result<()> {
    let frame = transport
        .send_command(TraceMsgType::Start.as_u8(), &[])
        .context("Failed to send trace start command")?;

    if frame.msg_type != TraceMsgType::Ack.as_u8() {
        anyhow::bail!(
            "Unexpected response type: 0x{:02X}, expected ACK 0x{:02X}",
            frame.msg_type,
            TraceMsgType::Ack.as_u8()
        );
    }

    let status = decode_ack(&frame.payload)?;
    match status {
        TraceStatus::Ok | TraceStatus::AlreadyOn => Ok(()),
        TraceStatus::NotInit => anyhow::bail!("Trace system not initialized"),
        _ => anyhow::bail!("Trace start failed: {}", status),
    }
}

/// Stop tracing
pub fn trace_stop(transport: &mut dyn Transport) -> Result<()> {
    let frame = transport
        .send_command(TraceMsgType::Stop.as_u8(), &[])
        .context("Failed to send trace stop command")?;

    if frame.msg_type != TraceMsgType::Ack.as_u8() {
        anyhow::bail!(
            "Unexpected response type: 0x{:02X}, expected ACK 0x{:02X}",
            frame.msg_type,
            TraceMsgType::Ack.as_u8()
        );
    }

    let status = decode_ack(&frame.payload)?;
    match status {
        TraceStatus::Ok | TraceStatus::AlreadyOff => Ok(()),
        TraceStatus::NotInit => anyhow::bail!("Trace system not initialized"),
        _ => anyhow::bail!("Trace stop failed: {}", status),
    }
}

/// Clear trace buffer
pub fn trace_clear(transport: &mut dyn Transport) -> Result<()> {
    let frame = transport
        .send_command(TraceMsgType::Clear.as_u8(), &[])
        .context("Failed to send trace clear command")?;

    if frame.msg_type != TraceMsgType::Ack.as_u8() {
        anyhow::bail!(
            "Unexpected response type: 0x{:02X}, expected ACK 0x{:02X}",
            frame.msg_type,
            TraceMsgType::Ack.as_u8()
        );
    }

    let status = decode_ack(&frame.payload)?;
    match status {
        TraceStatus::Ok => Ok(()),
        TraceStatus::NotInit => anyhow::bail!("Trace system not initialized"),
        _ => anyhow::bail!("Trace clear failed: {}", status),
    }
}

/// Get trace status
pub fn trace_status(transport: &mut dyn Transport) -> Result<TraceStatusInfo> {
    let frame = transport
        .send_command(TraceMsgType::StatusReq.as_u8(), &[])
        .context("Failed to send trace status command")?;

    // Check for ACK with error first
    if frame.msg_type == TraceMsgType::Ack.as_u8() {
        let status = decode_ack(&frame.payload)?;
        if status == TraceStatus::NotInit {
            anyhow::bail!("Trace system not initialized");
        }
        anyhow::bail!("Trace status failed: {}", status);
    }

    if frame.msg_type != TraceMsgType::StatusResp.as_u8() {
        anyhow::bail!(
            "Unexpected response type: 0x{:02X}, expected STATUS_RESP 0x{:02X}",
            frame.msg_type,
            TraceMsgType::StatusResp.as_u8()
        );
    }

    let resp = TraceStatusResponse::decode(frame.payload.as_slice())
        .context("Failed to decode TraceStatusResponse")?;

    Ok(TraceStatusInfo {
        initialized: resp.initialized,
        enabled: resp.enabled,
        streaming: resp.streaming,
        event_count: resp.event_count,
        dropped_count: resp.dropped_count,
        discontinuity_count: resp.discontinuity_count,
        buffer_size: resp.buffer_size,
    })
}

/// Result of a trace dump operation
pub struct DumpResult {
    pub event_count: u32,
    pub dropped_count: u32,
    pub duration_us: u32,
    pub pod_id: u32,
    pub output_path: std::path::PathBuf,
    pub raw_path: std::path::PathBuf,
    pub session_path: std::path::PathBuf,
    pub raw_sha256: String,
    pub firmware_version: String,
    pub device_uid: String,
    pub app_image_sha256: String,
    pub candidate_file_sha256: Option<String>,
}

struct RawEvidence {
    raw_path: std::path::PathBuf,
    session_path: std::path::PathBuf,
    raw_sha256: String,
}

fn write_raw_trace_evidence(
    output_path: &Path,
    raw_events: &[u8],
    session_info: &TraceSessionInfo,
    integrity_error: Option<&str>,
) -> Result<RawEvidence> {
    let raw_path = std::path::PathBuf::from(format!("{}.raw", output_path.display()));
    let raw_sha256 = format!("{:x}", Sha256::digest(raw_events));
    File::create(&raw_path)
        .context("Failed to create raw trace file")?
        .write_all(raw_events)
        .context("Failed to write raw trace file")?;
    let sha_path = std::path::PathBuf::from(format!("{}.sha256", raw_path.display()));
    File::create(&sha_path)
        .context("Failed to create raw trace hash file")?
        .write_all(format!("{}  {}\n", raw_sha256, raw_path.display()).as_bytes())
        .context("Failed to write raw trace hash file")?;
    let session_path = std::path::PathBuf::from(format!("{}.session.json", raw_path.display()));
    let session_evidence = serde_json::json!({
        "format_version": session_info.trace_event_format_version,
        "pod_id": session_info.pod_id,
        "event_count": session_info.event_count,
        "dropped_count": session_info.dropped_count,
        "discontinuity_count": session_info.discontinuity_count,
        "start_timestamp_us": session_info.start_timestamp_us,
        "end_timestamp_us": session_info.end_timestamp_us,
        "buffer_size_bytes": session_info.buffer_size_bytes,
        "firmware_version": session_info.firmware_version,
        "app_elf_sha256": hex::encode(&session_info.app_elf_sha256),
        "app_image_sha256": hex::encode(&session_info.app_image_sha256),
        "device_uid": hex::encode(&session_info.device_uid),
        "received_raw_bytes": raw_events.len(),
        "raw_sha256": raw_sha256,
        "integrity_error": integrity_error,
        "tasks": session_info.tasks.iter().map(|task| serde_json::json!({
            "task_id": task.task_id,
            "name": task.name.as_str(),
            "priority": task.priority,
            "core_affinity_mask": task.core_affinity_mask,
        })).collect::<Vec<_>>(),
        "objects": session_info.objects.iter().map(|object| serde_json::json!({
            "object_id": object.object_id,
            "kind": object.kind,
            "name": object.name.as_str(),
        })).collect::<Vec<_>>(),
    });
    File::create(&session_path)
        .context("Failed to create raw trace session file")?
        .write_all(serde_json::to_string_pretty(&session_evidence)?.as_bytes())
        .context("Failed to write raw trace session file")?;

    Ok(RawEvidence {
        raw_path,
        session_path,
        raw_sha256,
    })
}

fn augment_successful_evidence(
    session_path: &Path,
    context: Option<&TraceEvidenceContext<'_>>,
    candidate: Option<&CandidateImage>,
) -> Result<()> {
    let mut evidence: serde_json::Value = serde_json::from_slice(
        &std::fs::read(session_path).context("Failed to reread raw trace session file")?,
    )?;
    let object = evidence
        .as_object_mut()
        .context("Raw trace session evidence must be a JSON object")?;
    object.insert(
        "transport".into(),
        context.map_or(serde_json::Value::Null, |value| {
            serde_json::json!({
                "device_name": value.device_name,
                "type": value.transport_type,
                "address": value.address,
            })
        }),
    );
    object.insert(
        "candidate_image".into(),
        candidate.map_or(serde_json::Value::Null, |value| {
            serde_json::json!({
                "path": value.path.display().to_string(),
                "file_sha256": hex::encode(value.file_sha256),
                "app_image_sha256": hex::encode(value.app_image_sha256),
                "app_elf_sha256": hex::encode(value.app_elf_sha256),
                "firmware_version": value.firmware_version,
                "binding_verified": true,
            })
        }),
    );
    File::create(session_path)
        .context("Failed to update raw trace session file")?
        .write_all(serde_json::to_string_pretty(&evidence)?.as_bytes())
        .context("Failed to write bound raw trace session file")
}

fn fail_with_raw_evidence<T>(
    output_path: &Path,
    raw_events: &[u8],
    session_info: &TraceSessionInfo,
    error: anyhow::Error,
) -> Result<T> {
    let error_text = format!("{error:#}");
    match write_raw_trace_evidence(output_path, raw_events, session_info, Some(&error_text)) {
        Ok(evidence) => Err(error.context(format!(
            "Bounded raw trace evidence retained at {}",
            evidence.raw_path.display()
        ))),
        Err(write_error) => Err(anyhow::anyhow!(
            "{error_text}; additionally failed to retain raw trace evidence: {write_error:#}"
        )),
    }
}

fn trace_duration_us(start_timestamp_us: u32, end_timestamp_us: u32) -> u32 {
    end_timestamp_us.wrapping_sub(start_timestamp_us)
}

/// Dump traces to a JSON file compatible with Perfetto
pub fn trace_dump(
    transport: &mut dyn Transport,
    output_path: &Path,
    names_path: Option<&Path>,
    evidence_context: Option<&TraceEvidenceContext<'_>>,
    firmware_bin: Option<&Path>,
) -> Result<DumpResult> {
    let span_names = load_span_names(names_path)?;
    let candidate_image = firmware_bin.map(inspect_candidate_image).transpose()?;

    let frame = transport
        .send_command(TraceMsgType::Dump.as_u8(), &[])
        .context("Failed to send trace dump command")?;
    if frame.msg_type == TraceMsgType::Ack.as_u8() {
        let status = decode_ack(&frame.payload)?;
        match status {
            TraceStatus::NotInit => anyhow::bail!("Trace system not initialized"),
            TraceStatus::BufferEmpty => anyhow::bail!("Trace buffer is empty"),
            _ => anyhow::bail!("Trace dump failed: {}", status),
        }
    }
    if frame.msg_type != TraceMsgType::SessionInfo.as_u8() {
        anyhow::bail!(
            "Expected SESSION_INFO (0x{:02X}), got: 0x{:02X}",
            TraceMsgType::SessionInfo.as_u8(),
            frame.msg_type
        );
    }

    let session_info = TraceSessionInfo::decode(frame.payload.as_slice())
        .context("Failed to decode TraceSessionInfo")?;
    let expected_raw_bytes =
        match trace_dump_byte_count(session_info.event_count, session_info.buffer_size_bytes) {
            Ok(bytes) => bytes,
            Err(error) => return fail_with_raw_evidence(output_path, &[], &session_info, error),
        };
    let task_names: HashMap<u32, String> = session_info
        .tasks
        .iter()
        .map(|task| (task.task_id, task.name.clone()))
        .collect();

    let mut raw_events = Vec::with_capacity(expected_raw_bytes);
    let mut integrity = TraceDumpIntegrity::default();
    let dump_complete;
    loop {
        let frame = match transport.receive_frame(5000) {
            Ok(frame) => frame,
            Err(error) => {
                return fail_with_raw_evidence(
                    output_path,
                    &raw_events,
                    &session_info,
                    error.context("Failed to receive trace data"),
                );
            }
        };
        if frame.msg_type == TraceMsgType::Data.as_u8() {
            let chunk = match TraceDataChunk::decode(frame.payload.as_slice()) {
                Ok(chunk) => chunk,
                Err(error) => {
                    return fail_with_raw_evidence(
                        output_path,
                        &raw_events,
                        &session_info,
                        anyhow::Error::new(error).context("Failed to decode TraceDataChunk"),
                    );
                }
            };
            let retained_bytes = raw_events.len().saturating_add(chunk.events.len());
            if retained_bytes > MAX_TRACE_DUMP_BYTES {
                let remaining = MAX_TRACE_DUMP_BYTES.saturating_sub(raw_events.len());
                raw_events.extend_from_slice(&chunk.events[..remaining.min(chunk.events.len())]);
                return fail_with_raw_evidence(
                    output_path,
                    &raw_events,
                    &session_info,
                    anyhow::anyhow!(
                        "Trace data exceeds the absolute {}-byte retention bound",
                        MAX_TRACE_DUMP_BYTES
                    ),
                );
            }
            // Retain bounded bytes before trusting offset/count/alignment.
            raw_events.extend_from_slice(&chunk.events);
            if let Err(error) = integrity
                .accept_chunk(&chunk)
                .context("Invalid trace data chunk")
            {
                return fail_with_raw_evidence(output_path, &raw_events, &session_info, error);
            }
            if integrity.next_offset > session_info.event_count {
                return fail_with_raw_evidence(
                    output_path,
                    &raw_events,
                    &session_info,
                    anyhow::anyhow!(
                        "Trace data exceeds declared session count {}",
                        session_info.event_count
                    ),
                );
            }
            if raw_events.len() > expected_raw_bytes {
                return fail_with_raw_evidence(
                    output_path,
                    &raw_events,
                    &session_info,
                    anyhow::anyhow!(
                        "Trace data exceeds declared session size {} bytes",
                        expected_raw_bytes
                    ),
                );
            }
        } else if frame.msg_type == TraceMsgType::End.as_u8() {
            dump_complete = match TraceDumpComplete::decode(frame.payload.as_slice()) {
                Ok(complete) => complete,
                Err(error) => {
                    return fail_with_raw_evidence(
                        output_path,
                        &raw_events,
                        &session_info,
                        anyhow::Error::new(error).context("Failed to decode TraceDumpComplete"),
                    );
                }
            };
            break;
        } else {
            return fail_with_raw_evidence(
                output_path,
                &raw_events,
                &session_info,
                anyhow::anyhow!(
                    "Unexpected message type during dump: 0x{:02X}",
                    frame.msg_type
                ),
            );
        }
    }

    if let Err(error) = integrity
        .finish(&dump_complete, session_info.event_count)
        .context("Trace dump integrity check failed")
    {
        return fail_with_raw_evidence(output_path, &raw_events, &session_info, error);
    }
    let evidence = write_raw_trace_evidence(output_path, &raw_events, &session_info, None)?;

    if session_info.trace_event_format_version != TRACE_EVENT_FORMAT_VERSION {
        return fail_with_raw_evidence(
            output_path,
            &raw_events,
            &session_info,
            anyhow::anyhow!(
                "Unsupported trace event format version: {}",
                session_info.trace_event_format_version
            ),
        );
    }
    if session_info.dropped_count != 0 || session_info.discontinuity_count != 0 {
        return fail_with_raw_evidence(
            output_path,
            &raw_events,
            &session_info,
            anyhow::anyhow!(
                "Trace evidence is incomplete: dropped={}, discontinuities={}",
                session_info.dropped_count,
                session_info.discontinuity_count
            ),
        );
    }
    let events = decode_trace_events(&raw_events).context("Invalid raw trace events")?;
    if let Err(error) = validate_session_binding(&session_info, &events)
        .context("Trace session metadata is not bound to the raw event stream")
    {
        return fail_with_raw_evidence(output_path, &raw_events, &session_info, error);
    }
    if let Some(candidate) = &candidate_image {
        if let Err(error) = validate_candidate_binding(&session_info, candidate)
            .context("Candidate image is not bound to the trace-producing firmware")
        {
            return fail_with_raw_evidence(output_path, &raw_events, &session_info, error);
        }
    }
    augment_successful_evidence(
        &evidence.session_path,
        evidence_context,
        candidate_image.as_ref(),
    )?;
    let json = convert_to_perfetto_json(&events, &task_names, &span_names, session_info.pod_id)?;

    let mut file = File::create(output_path).context("Failed to create output file")?;
    file.write_all(json.as_bytes())
        .context("Failed to write trace file")?;

    Ok(DumpResult {
        event_count: integrity.next_offset,
        dropped_count: session_info.dropped_count,
        duration_us: trace_duration_us(
            session_info.start_timestamp_us,
            session_info.end_timestamp_us,
        ),
        pod_id: session_info.pod_id,
        output_path: output_path.to_path_buf(),
        raw_path: evidence.raw_path,
        session_path: evidence.session_path,
        raw_sha256: evidence.raw_sha256,
        firmware_version: session_info.firmware_version.clone(),
        device_uid: hex::encode(&session_info.device_uid),
        app_image_sha256: hex::encode(&session_info.app_image_sha256),
        candidate_file_sha256: candidate_image
            .as_ref()
            .map(|image| hex::encode(image.file_sha256)),
    })
}

/// Load span name mappings from a JSON file
///
/// Format: { "hash_decimal": "Module.SpanName", ... }
/// Auto-discovers tools/trace/trace_names.json if no path given.
fn load_span_names(names_path: Option<&Path>) -> Result<HashMap<u32, String>> {
    let path = match names_path {
        Some(p) => {
            if !p.exists() {
                anyhow::bail!("Span names file not found: {}", p.display());
            }
            p.to_path_buf()
        }
        None => {
            // Auto-discover trace_names.json relative to the binary
            let candidates = [
                Path::new("tools/trace/trace_names.json").to_path_buf(),
                Path::new("trace_names.json").to_path_buf(),
            ];
            match candidates.iter().find(|p| p.exists()) {
                Some(p) => p.clone(),
                None => return Ok(HashMap::new()), // No names file, use defaults
            }
        }
    };

    let content = std::fs::read_to_string(&path)
        .with_context(|| format!("Failed to read span names from {}", path.display()))?;

    let raw: HashMap<String, String> =
        serde_json::from_str(&content).context("Failed to parse span names JSON")?;

    let mut names = HashMap::new();
    for (key, value) in raw {
        if let Ok(hash) = key.parse::<u32>() {
            names.insert(hash, value);
        }
    }

    Ok(names)
}

/// Convert trace events to Perfetto-compatible Chrome JSON format
fn convert_to_perfetto_json(
    events: &[TraceEvent],
    task_names: &HashMap<u32, String>,
    span_names: &HashMap<u32, String>,
    pod_id: u32,
) -> Result<String> {
    let mut trace_events = Vec::with_capacity(task_names.len() + events.len());

    let mut tasks: Vec<_> = task_names.iter().collect();
    tasks.sort_unstable_by_key(|(task_id, _)| **task_id);
    for (task_id, task_name) in tasks {
        trace_events.push(serde_json::json!({
            "name": "thread_name",
            "cat": "__metadata",
            "ph": "M",
            "pid": pod_id,
            "tid": task_id,
            "args": {"name": task_name},
        }));
    }

    for event in events {
        // Copy packed struct fields to local variables to avoid unaligned access
        let timestamp = { event.timestamp };
        let task_id = { event.task_id };
        let event_type = EventType::try_from(i32::from(event.event_type)).ok();
        let flags = { event.flags };
        let arg1 = { event.arg1 };
        let arg2 = { event.arg2 };

        let category = category_name((flags >> 4) & 0x0F);

        // Chrome trace event format
        let phase = match event_type {
            Some(EventType::SpanBegin) => "B",
            Some(EventType::SpanEnd) => "E",
            Some(EventType::Counter) => "C",
            Some(EventType::Complete) => "X",
            Some(EventType::MutexLock) => "B",
            Some(EventType::MutexUnlock) => "E",
            _ => "i",
        };

        // Resolve span name from hash
        let name = match event_type {
            Some(EventType::MutexLock | EventType::MutexUnlock) => {
                // Mutex lock/unlock: resolve name from hash
                span_names
                    .get(&arg1)
                    .cloned()
                    .unwrap_or_else(|| format!("mutex:{}", arg1))
            }
            Some(EventType::MutexContention) => {
                // Mutex contention: resolve name, arg2 = wait time us
                span_names
                    .get(&arg1)
                    .cloned()
                    .unwrap_or_else(|| format!("mutex:{}", arg1))
            }
            Some(EventType::SemTake | EventType::SemGive) => {
                // Semaphore take/give: resolve name from hash
                span_names
                    .get(&arg1)
                    .cloned()
                    .unwrap_or_else(|| format!("sem:{}", arg1))
            }
            Some(EventType::Counter) => {
                // Counter: resolve name from hash
                span_names
                    .get(&arg1)
                    .cloned()
                    .unwrap_or_else(|| format!("counter:{}", arg1))
            }
            _ => {
                // Span/instant: resolve name from hash
                span_names
                    .get(&arg1)
                    .cloned()
                    .unwrap_or_else(|| format!("span:{}", arg1))
            }
        };

        let mut trace_event = serde_json::json!({
            "name": name,
            "cat": category,
            "ph": phase,
            "ts": timestamp,
            "pid": pod_id,
            "tid": task_id,
        });
        let object = trace_event
            .as_object_mut()
            .context("Trace event must serialize as a JSON object")?;

        // Add duration for complete events
        if event_type == Some(EventType::Complete) {
            object.insert("dur".into(), arg2.into());
        }

        // Add counter value
        if event_type == Some(EventType::Counter) {
            object.insert("args".into(), serde_json::json!({"value": arg2}));
        }

        // Add mutex contention wait time
        if event_type == Some(EventType::MutexContention) {
            object.insert("args".into(), serde_json::json!({"wait_us": arg2}));
        }

        trace_events.push(trace_event);
    }

    serde_json::to_string(&trace_events).context("Failed to serialize Perfetto trace JSON")
}

fn decode_stream_batch(
    frame: &Frame,
    sequence_tracker: &mut StreamSequenceTracker,
) -> Result<(u32, Vec<TraceEvent>)> {
    if frame.msg_type != TraceMsgType::StreamData.as_u8() {
        anyhow::bail!(
            "Unexpected trace stream message type: 0x{:02X}, expected STREAM_DATA 0x{:02X}",
            frame.msg_type,
            TraceMsgType::StreamData.as_u8()
        );
    }

    let batch = StreamBatch::decode(frame.payload.as_slice())
        .context("Failed to decode trace StreamBatch")?;
    sequence_tracker
        .accept(batch.sequence)
        .context("Trace stream integrity check failed")?;
    let events = decode_trace_events(&batch.events).context("Invalid trace stream event batch")?;
    Ok((batch.dropped, events))
}

/// Stream trace events in real-time from a TCP connection
///
/// Connects to the trace stream port (5001) on the device and prints
/// events as they arrive. Runs until interrupted (Ctrl+C) or error.
pub fn trace_stream(addr: &str) -> Result<()> {
    use std::net::TcpStream;

    // Connect to trace stream port (5001)
    let stream_addr = if addr.contains(':') {
        // Replace port with 5001
        let parts: Vec<&str> = addr.splitn(2, ':').collect();
        format!("{}:5001", parts[0])
    } else {
        format!("{}:5001", addr)
    };

    eprintln!("Connecting to trace stream at {}...", stream_addr);
    let mut stream = TcpStream::connect(&stream_addr)
        .with_context(|| format!("Failed to connect to trace stream at {}", stream_addr))?;

    stream
        .set_read_timeout(Some(std::time::Duration::from_secs(5)))
        .context("Failed to configure trace stream read timeout")?;

    eprintln!("Connected. Streaming trace events (Ctrl+C to stop)...");
    eprintln!(
        "{:<12} {:<6} {:<12} {:<12} {:>10} {:>10}",
        "TIMESTAMP", "TASK", "TYPE", "CATEGORY", "ARG1", "ARG2"
    );
    eprintln!(
        "{:-<12} {:-<6} {:-<12} {:-<12} {:->10} {:->10}",
        "", "", "", "", "", ""
    );

    // Load span names for resolving
    let span_names = load_span_names(None).unwrap_or_default();

    let mut frame_decoder = crate::transport::frame::FrameDecoder::new();
    let mut sequence_tracker = StreamSequenceTracker::default();
    let mut buf = [0u8; 1024];

    loop {
        use std::io::Read;
        let n = match stream.read(&mut buf) {
            Ok(0) => {
                eprintln!("\nConnection closed by device");
                break;
            }
            Ok(n) => n,
            Err(e)
                if e.kind() == std::io::ErrorKind::TimedOut
                    || e.kind() == std::io::ErrorKind::WouldBlock =>
            {
                continue;
            }
            Err(e) => {
                return Err(anyhow::anyhow!("Read error: {}", e));
            }
        };

        // Feed bytes to frame decoder
        for &byte in &buf[..n] {
            if let Some(frame) = frame_decoder.feed_byte(byte) {
                frame_decoder.reset();
                let frame = frame.context("Invalid trace stream frame")?;
                let (dropped, events) = decode_stream_batch(&frame, &mut sequence_tracker)?;

                if dropped > 0 {
                    eprintln!("  [dropped {} events]", dropped);
                }

                for event in events {
                    let timestamp = { event.timestamp };
                    let task_id = { event.task_id };
                    let event_type = EventType::try_from(i32::from(event.event_type)).ok();
                    let flags = { event.flags };
                    let arg1 = { event.arg1 };
                    let arg2 = { event.arg2 };

                    let type_name = match event_type {
                        Some(EventType::SpanBegin) => "BEGIN",
                        Some(EventType::SpanEnd) => "END",
                        Some(EventType::Instant) => "INSTANT",
                        Some(EventType::Counter) => "COUNTER",
                        Some(EventType::Complete) => "COMPLETE",
                        _ => "UNKNOWN",
                    };

                    let cat = category_name((flags >> 4) & 0x0F);

                    let name = span_names.get(&arg1).map(|s| s.as_str()).unwrap_or("");

                    if event_type == Some(EventType::Counter) {
                        // Counter
                        println!(
                            "{:<12} {:<6} {:<12} {:<12} {} = {}",
                            timestamp, task_id, type_name, cat, name, arg2
                        );
                    } else {
                        println!(
                            "{:<12} {:<6} {:<12} {:<12} {:>10} {:>10}  {}",
                            timestamp, task_id, type_name, cat, arg1, arg2, name
                        );
                    }
                }
            }
        }
    }

    Ok(())
}

fn category_name(cat: u8) -> &'static str {
    Category::try_from(i32::from(cat))
        .map(|category| category.cli_name())
        .unwrap_or("unknown")
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::proto::trace::{ObjectEntry, TaskEntry};
    use std::collections::VecDeque;

    struct DumpTransport {
        session: Option<Frame>,
        frames: VecDeque<Frame>,
    }

    impl Transport for DumpTransport {
        fn send_frame(&mut self, _msg_type: u8, _payload: &[u8]) -> Result<()> {
            Ok(())
        }

        fn receive_frame(&mut self, _timeout_ms: u64) -> Result<Frame> {
            self.frames
                .pop_front()
                .context("mock trace transport exhausted")
        }

        fn send_command(&mut self, _msg_type: u8, _payload: &[u8]) -> Result<Frame> {
            self.session.take().context("mock trace session reused")
        }
    }

    fn event_bytes(value: u8, count: usize) -> Vec<u8> {
        vec![value; TRACE_EVENT_SIZE * count]
    }

    fn bound_session(start: u32, end: u32) -> TraceSessionInfo {
        TraceSessionInfo {
            pod_id: 1,
            event_count: 2,
            start_timestamp_us: start,
            end_timestamp_us: end,
            tasks: vec![TaskEntry {
                task_id: 7,
                name: "worker".into(),
                priority: 2,
                core_affinity_mask: 1,
            }],
            buffer_size_bytes: 16 * 1024,
            trace_event_format_version: TRACE_EVENT_FORMAT_VERSION,
            objects: vec![ObjectEntry {
                object_id: 100,
                kind: 1,
                name: "queue".into(),
            }],
            firmware_version: "host-test".into(),
            app_elf_sha256: vec![0xA5; 32],
            app_image_sha256: vec![0x5A; 32],
            device_uid: vec![0x02, 0, 0, 0, 0, 1],
            ..Default::default()
        }
    }

    fn bound_events(start: u32, end: u32) -> Vec<TraceEvent> {
        vec![
            TraceEvent {
                timestamp: start,
                task_id: 7,
                event_type: 0x10,
                flags: 1,
                arg1: 2,
                arg2: 1,
            },
            TraceEvent {
                timestamp: end,
                task_id: 7,
                event_type: 0x1A,
                flags: 1,
                arg1: 100,
                arg2: 0,
            },
        ]
    }

    fn encoded_events(events: &[TraceEvent]) -> Vec<u8> {
        let mut raw = Vec::with_capacity(events.len() * TRACE_EVENT_SIZE);
        for event in events {
            raw.extend_from_slice(&event.timestamp.to_le_bytes());
            raw.extend_from_slice(&event.task_id.to_le_bytes());
            raw.push(event.event_type);
            raw.push(event.flags);
            raw.extend_from_slice(&event.arg1.to_le_bytes());
            raw.extend_from_slice(&event.arg2.to_le_bytes());
        }
        raw
    }

    fn physical_correlation_session() -> (TraceSessionInfo, Vec<TraceEvent>) {
        const QUEUE: u32 = 0x59FB_7823;
        const READY: u32 = 0xEF2D_D8BB;
        const CALLBACK: u32 = 0x3580_0DA2;
        const COMPLETE: u32 = 0xF1F4_511E;
        let objects = [
            (1, 1, "probe_queue"),
            (2, 2, "probe_sem"),
            (3, 3, "probe_irq"),
            (4, 4, "probe_callback"),
            (5, 5, "probe_action"),
            (6, 7, "probe_timeout"),
            (QUEUE, 1, "espnow_queue"),
            (READY, 2, "espnow_ready"),
            (CALLBACK, 4, "espnow_cb"),
            (COMPLETE, 5, "espnow_done"),
        ];
        let mut events = vec![TraceEvent {
            timestamp: 100,
            task_id: 7,
            event_type: 0x10,
            flags: 1,
            arg1: 2,
            arg2: 1,
        }];
        let rx = [
            (0x1C, CALLBACK, 0, 9),
            (0x19, QUEUE, 0, 9),
            (0x0D, READY, 0, 9),
            (0x1D, CALLBACK, 0, 9),
            (0x0C, READY, 7, 1),
            (0x1A, QUEUE, 7, 1),
            (0x1E, COMPLETE, 7, 1),
        ];
        let tx = [
            (0x19, QUEUE, 7, 1),
            (0x1C, CALLBACK, 0, 9),
            (0x0D, READY, 0, 9),
            (0x1D, CALLBACK, 0, 9),
            (0x0C, READY, 7, 1),
            (0x1E, COMPLETE, 7, 1),
        ];
        for (event_type, arg1, task_id, flags) in rx {
            events.push(TraceEvent {
                timestamp: 100 + events.len() as u32,
                task_id,
                event_type,
                flags,
                arg1,
                arg2: 417,
            });
        }
        for (event_type, arg1, task_id, flags) in tx {
            events.push(TraceEvent {
                timestamp: 100 + events.len() as u32,
                task_id,
                event_type,
                flags,
                arg1,
                arg2: 418,
            });
        }
        let session = TraceSessionInfo {
            event_count: events.len() as u32,
            start_timestamp_us: 100,
            end_timestamp_us: events.last().unwrap().timestamp,
            tasks: vec![TaskEntry {
                task_id: 7,
                name: "worker".into(),
                priority: 2,
                core_affinity_mask: 1,
            }],
            buffer_size_bytes: 32 * 1024,
            trace_event_format_version: TRACE_EVENT_FORMAT_VERSION,
            objects: objects
                .into_iter()
                .map(|(object_id, kind, name)| ObjectEntry {
                    object_id,
                    kind,
                    name: name.into(),
                })
                .collect(),
            firmware_version: "host-test".into(),
            app_elf_sha256: vec![0xA5; 32],
            app_image_sha256: vec![0x5A; 32],
            device_uid: vec![0x02, 0, 0, 0, 0, 1],
            ..Default::default()
        };
        (session, events)
    }

    fn candidate_image_bytes(version: &str, elf_sha256: &[u8; 32]) -> Vec<u8> {
        let mut image =
            vec![0u8; ESP_IMAGE_HEADER_SIZE + ESP_SEGMENT_HEADER_SIZE + ESP_APP_DESC_SIZE];
        image[0] = 0xE9;
        image[1] = 1;
        image[23] = 1;
        image[ESP_IMAGE_HEADER_SIZE + 4..ESP_IMAGE_HEADER_SIZE + 8]
            .copy_from_slice(&(ESP_APP_DESC_SIZE as u32).to_le_bytes());
        let descriptor = ESP_IMAGE_HEADER_SIZE + ESP_SEGMENT_HEADER_SIZE;
        image[descriptor..descriptor + 4].copy_from_slice(&ESP_APP_DESC_MAGIC.to_le_bytes());
        image[descriptor + 16..descriptor + 16 + version.len()].copy_from_slice(version.as_bytes());
        image[descriptor + 144..descriptor + 176].copy_from_slice(elf_sha256);
        let appended = Sha256::digest(&image);
        image.extend_from_slice(&appended);
        image
    }

    #[test]
    fn candidate_image_binds_version_elf_and_running_image_hash() {
        let directory =
            std::env::temp_dir().join(format!("domes-image-bind-test-{}", std::process::id()));
        std::fs::create_dir_all(&directory).unwrap();
        let path = directory.join("domes.bin");
        let elf_sha256 = [0xA5; 32];
        std::fs::write(&path, candidate_image_bytes("host-test", &elf_sha256)).unwrap();

        let candidate = inspect_candidate_image(&path).unwrap();
        let mut session = bound_session(1, 2);
        session.app_image_sha256 = candidate.app_image_sha256.to_vec();
        validate_candidate_binding(&session, &candidate).unwrap();

        session.app_image_sha256[0] ^= 0xFF;
        assert!(validate_candidate_binding(&session, &candidate).is_err());
        std::fs::remove_file(path).unwrap();
        std::fs::remove_dir(directory).unwrap();
    }

    #[test]
    fn evidence_identity_rejects_missing_or_multicast_device_identity() {
        let mut session = bound_session(1, 2);
        session.app_elf_sha256.clear();
        assert!(validate_evidence_identity(&session).is_err());

        session.app_elf_sha256 = vec![0xA5; 32];
        session.device_uid = vec![0x03, 0, 0, 0, 0, 1];
        assert!(validate_evidence_identity(&session).is_err());
    }

    #[test]
    fn malformed_chunk_retains_bounded_raw_bytes_and_error_metadata() {
        let unique = format!(
            "domes-trace-test-{}-{}",
            std::process::id(),
            std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)
                .unwrap()
                .as_nanos()
        );
        let directory = std::env::temp_dir().join(unique);
        std::fs::create_dir(&directory).unwrap();
        let output_path = directory.join("trace.json");
        let session = TraceSessionInfo {
            event_count: 1,
            start_timestamp_us: 1,
            end_timestamp_us: 1,
            buffer_size_bytes: TRACE_EVENT_SIZE as u32,
            trace_event_format_version: TRACE_EVENT_FORMAT_VERSION,
            ..Default::default()
        };
        let offending_bytes = event_bytes(1, 1);
        let chunk = TraceDataChunk {
            offset: 1,
            count: 1,
            events: offending_bytes.clone(),
        };
        let mut transport = DumpTransport {
            session: Some(Frame {
                msg_type: TraceMsgType::SessionInfo.as_u8(),
                payload: session.encode_to_vec(),
            }),
            frames: VecDeque::from([Frame {
                msg_type: TraceMsgType::Data.as_u8(),
                payload: chunk.encode_to_vec(),
            }]),
        };

        assert!(trace_dump(&mut transport, &output_path, None, None, None).is_err());
        let raw_path = std::path::PathBuf::from(format!("{}.raw", output_path.display()));
        assert_eq!(std::fs::read(&raw_path).unwrap(), offending_bytes);
        let session_path = std::path::PathBuf::from(format!("{}.session.json", raw_path.display()));
        let evidence: serde_json::Value =
            serde_json::from_slice(&std::fs::read(&session_path).unwrap()).unwrap();
        assert!(evidence["integrity_error"]
            .as_str()
            .unwrap()
            .contains("offset mismatch"));

        std::fs::remove_file(format!("{}.sha256", raw_path.display())).unwrap();
        std::fs::remove_file(session_path).unwrap();
        std::fs::remove_file(raw_path).unwrap();
        std::fs::remove_dir(directory).unwrap();
    }

    #[test]
    fn session_binding_accepts_exact_catalogs_and_one_timestamp_wrap() {
        let start = u32::MAX - 5;
        let end = 4;
        validate_session_binding(&bound_session(start, end), &bound_events(start, end)).unwrap();
    }

    #[test]
    fn session_binding_decodes_complete_ten_object_rx_and_tx_chains() {
        let (session, events) = physical_correlation_session();
        let decoded = decode_trace_events(&encoded_events(&events)).unwrap();
        validate_session_binding(&session, &decoded).unwrap();

        let mut broken_token = decoded;
        broken_token[8].arg2 = 419;
        assert!(validate_session_binding(&session, &broken_token)
            .unwrap_err()
            .to_string()
            .contains("correlation chain"));

        let mut wrong_mapping = session;
        wrong_mapping.objects[0].name = "wrong_queue".into();
        assert!(validate_session_binding(&wrong_mapping, &events)
            .unwrap_err()
            .to_string()
            .contains("acceptance object mapping"));

        let (mut wrong_callback_context, events) = physical_correlation_session();
        let mut events = events;
        events[1].flags = 1;
        assert!(validate_session_binding(&wrong_callback_context, &events)
            .unwrap_err()
            .to_string()
            .contains("invalid context or task ownership"));

        let (_, mut events) = physical_correlation_session();
        events[1].task_id = 7;
        assert!(validate_session_binding(&wrong_callback_context, &events)
            .unwrap_err()
            .to_string()
            .contains("invalid context or task ownership"));

        wrong_callback_context.objects[6].kind = 2;
        assert!(validate_session_binding(
            &wrong_callback_context,
            &physical_correlation_session().1
        )
        .unwrap_err()
        .to_string()
        .contains("acceptance object mapping"));
    }

    #[test]
    fn session_binding_rejects_acceptance_catalog_hybrids() {
        let (session, events) = physical_correlation_session();
        let mut probe_only = session.clone();
        probe_only.objects.truncate(6);
        probe_only.event_count = 1;
        probe_only.end_timestamp_us = probe_only.start_timestamp_us;
        validate_session_binding(&probe_only, &events[..1]).unwrap();

        for count in 7..10 {
            let mut hybrid = session.clone();
            hybrid.objects.truncate(count);
            assert!(validate_session_binding(&hybrid, &events)
                .unwrap_err()
                .to_string()
                .contains("exactly 6 or 10 entries"));
        }
    }

    #[test]
    fn session_binding_rejects_timestamp_regressions_and_bounds_mismatch() {
        let events = bound_events(100, 90);
        let error = validate_session_binding(&bound_session(100, 90), &events)
            .unwrap_err()
            .to_string();
        assert!(error.contains("timestamp regression"));

        let error = validate_session_binding(&bound_session(99, 101), &bound_events(100, 101))
            .unwrap_err()
            .to_string();
        assert!(error.contains("bounds do not match"));
    }

    #[test]
    fn session_binding_rejects_catalog_and_object_mismatches() {
        let events = bound_events(100, 101);
        let mut missing_task = bound_session(100, 101);
        missing_task.tasks.clear();
        assert!(validate_session_binding(&missing_task, &events)
            .unwrap_err()
            .to_string()
            .contains("task catalog"));

        let mut wrong_object_kind = bound_session(100, 101);
        wrong_object_kind.objects[0].kind = 2;
        assert!(validate_session_binding(&wrong_object_kind, &events)
            .unwrap_err()
            .to_string()
            .contains("incompatible kind"));

        let mut invalid_priority = bound_session(100, 101);
        invalid_priority.tasks[0].priority = 256;
        assert!(validate_session_binding(&invalid_priority, &events).is_err());

        let mut unknown_event = events;
        unknown_event[1].event_type = EventType::Unknown as u8;
        assert!(validate_session_binding(&bound_session(100, 101), &unknown_event).is_err());
    }

    #[test]
    fn session_binding_keeps_legacy_application_semaphore_ids_separate() {
        let mut events = bound_events(100, 102);
        events.push(TraceEvent {
            timestamp: 102,
            task_id: 7,
            event_type: 0x0C,
            flags: (Category::Sync as u8) << 4 | 1,
            arg1: 0xDEAD_BEEF,
            arg2: 0,
        });
        let mut session = bound_session(100, 102);
        session.event_count = 3;
        validate_session_binding(&session, &events).unwrap();

        events[2].flags = (Category::Transport as u8) << 4 | 1;
        assert!(validate_session_binding(&session, &events)
            .unwrap_err()
            .to_string()
            .contains("incompatible category"));
    }

    #[test]
    fn trace_event_decode_is_explicitly_little_endian() {
        let bytes = [
            0x78, 0x56, 0x34, 0x12, 0xCD, 0xAB, 0x1E, 0x09, 0x04, 0x03, 0x02, 0x01, 0xD4, 0xC3,
            0xB2, 0xA1,
        ];
        let events = decode_trace_events(&bytes).unwrap();
        assert_eq!(events.len(), 1);
        assert_eq!(events[0].timestamp, 0x1234_5678);
        assert_eq!(events[0].task_id, 0xABCD);
        assert_eq!(events[0].event_type, 0x1E);
        assert_eq!(events[0].flags, 0x09);
        assert_eq!(events[0].arg1, 0x0102_0304);
        assert_eq!(events[0].arg2, 0xA1B2_C3D4);
    }

    #[test]
    fn dump_integrity_accepts_contiguous_chunks() {
        let mut integrity = TraceDumpIntegrity::default();
        let first = TraceDataChunk {
            offset: 0,
            count: 1,
            events: event_bytes(1, 1),
        };
        let second = TraceDataChunk {
            offset: 1,
            count: 2,
            events: event_bytes(2, 2),
        };

        integrity.accept_chunk(&first).unwrap();
        integrity.accept_chunk(&second).unwrap();

        let complete = TraceDumpComplete {
            total_events: 3,
            checksum: (TRACE_EVENT_SIZE as u32) + (TRACE_EVENT_SIZE as u32 * 2 * 2),
        };
        integrity.finish(&complete, 3).unwrap();
    }

    #[test]
    fn dump_integrity_rejects_offset_gap() {
        let mut integrity = TraceDumpIntegrity::default();
        let chunk = TraceDataChunk {
            offset: 1,
            count: 1,
            events: event_bytes(1, 1),
        };

        let error = integrity.accept_chunk(&chunk).unwrap_err().to_string();
        assert!(error.contains("offset mismatch"));
    }

    #[test]
    fn dump_integrity_rejects_count_and_alignment_mismatches() {
        let mut integrity = TraceDumpIntegrity::default();
        let wrong_count = TraceDataChunk {
            offset: 0,
            count: 2,
            events: event_bytes(1, 1),
        };
        assert!(integrity
            .accept_chunk(&wrong_count)
            .unwrap_err()
            .to_string()
            .contains("count mismatch"));

        let mut integrity = TraceDumpIntegrity::default();
        let misaligned = TraceDataChunk {
            offset: 0,
            count: 1,
            events: vec![0; TRACE_EVENT_SIZE - 1],
        };
        assert!(integrity
            .accept_chunk(&misaligned)
            .unwrap_err()
            .to_string()
            .contains("multiple"));
    }

    #[test]
    fn dump_integrity_rejects_empty_chunks() {
        let chunk = TraceDataChunk {
            offset: 0,
            count: 0,
            events: Vec::new(),
        };
        assert!(TraceDumpIntegrity::default()
            .accept_chunk(&chunk)
            .unwrap_err()
            .to_string()
            .contains("empty"));
    }

    #[test]
    fn dump_session_bounds_prevent_unbounded_allocation() {
        assert_eq!(trace_dump_byte_count(1, 16).unwrap(), 16);
        assert!(trace_dump_byte_count(0, 16).is_err());
        assert!(trace_dump_byte_count(2, 16).is_err());
        assert!(trace_dump_byte_count(1, 0).is_err());
        assert!(trace_dump_byte_count(1, (MAX_TRACE_DUMP_BYTES + 1) as u32).is_err());
        assert!(trace_dump_byte_count(u32::MAX, MAX_TRACE_DUMP_BYTES as u32).is_err());
    }

    #[test]
    fn dump_integrity_rejects_bad_completion_metadata() {
        let mut integrity = TraceDumpIntegrity::default();
        let chunk = TraceDataChunk {
            offset: 0,
            count: 1,
            events: event_bytes(1, 1),
        };
        integrity.accept_chunk(&chunk).unwrap();

        let wrong_total = TraceDumpComplete {
            total_events: 2,
            checksum: TRACE_EVENT_SIZE as u32,
        };
        assert!(integrity
            .finish(&wrong_total, 1)
            .unwrap_err()
            .to_string()
            .contains("total mismatch"));

        let wrong_checksum = TraceDumpComplete {
            total_events: 1,
            checksum: 0,
        };
        assert!(integrity
            .finish(&wrong_checksum, 1)
            .unwrap_err()
            .to_string()
            .contains("checksum mismatch"));
    }

    #[test]
    fn stream_sequence_tracker_detects_gaps_and_wraps() {
        let mut tracker = StreamSequenceTracker::default();
        tracker.accept(0).unwrap();
        tracker.accept(1).unwrap();
        assert!(tracker.accept(3).unwrap_err().to_string().contains("gap"));

        let mut wrapping = StreamSequenceTracker {
            next_sequence: u32::MAX,
        };
        wrapping.accept(u32::MAX).unwrap();
        wrapping.accept(0).unwrap();
    }

    #[test]
    fn stream_sequence_tracker_rejects_nonzero_initial_sequence() {
        let error = StreamSequenceTracker::default()
            .accept(1)
            .unwrap_err()
            .to_string();
        assert!(error.contains("expected 0, got 1"));
    }

    #[test]
    fn stream_batch_rejects_unexpected_frame_type() {
        let frame = Frame {
            msg_type: TraceMsgType::Ack.as_u8(),
            payload: Vec::new(),
        };
        let error = decode_stream_batch(&frame, &mut StreamSequenceTracker::default())
            .unwrap_err()
            .to_string();
        assert!(error.contains("Unexpected trace stream message type"));
    }

    #[test]
    fn perfetto_json_escapes_task_and_span_names() {
        let event = TraceEvent {
            timestamp: 123,
            task_id: 7,
            event_type: EventType::Instant as u8,
            flags: (Category::Game as u8) << 4,
            arg1: 42,
            arg2: 0,
        };
        let task_name = "task\"\\name\n";
        let span_name = "span\"\\name\n";
        let task_names = HashMap::from([(7, task_name.to_string())]);
        let span_names = HashMap::from([(42, span_name.to_string())]);

        let json = convert_to_perfetto_json(&[event], &task_names, &span_names, 9).unwrap();
        let value: serde_json::Value = serde_json::from_str(&json).unwrap();

        assert_eq!(value[0]["args"]["name"], task_name);
        assert_eq!(value[1]["name"], span_name);
    }

    #[test]
    fn trace_duration_handles_timestamp_wrap() {
        assert_eq!(trace_duration_us(100, 175), 75);
        assert_eq!(trace_duration_us(u32::MAX - 10, 9), 20);
    }
}
