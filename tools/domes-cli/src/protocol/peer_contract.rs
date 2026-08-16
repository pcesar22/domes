//! Strict portable peer/drill protobuf consumer.

use crate::proto::peer::{
    peer_message::Payload, ContractVersion, PeerLifecycleState, PeerMessage, PeerRole,
};
use prost::Message;

const MAX_PEER_MESSAGE_SIZE: usize = 64;

#[derive(Debug, thiserror::Error, PartialEq, Eq)]
pub enum PeerContractError {
    #[error("peer message is empty or oversized")]
    Size,
    #[error("peer message protobuf is malformed or contains unknown/noncanonical fields")]
    Malformed,
    #[error("peer message has invalid fields")]
    InvalidFields,
    #[error("peer message sender role is invalid for its payload")]
    InvalidRole,
    #[error("peer message is invalid in the receiver lifecycle state")]
    InvalidState,
}

pub fn decode_peer_message(
    bytes: &[u8],
    receiver_role: PeerRole,
    state: PeerLifecycleState,
) -> Result<PeerMessage, PeerContractError> {
    if bytes.is_empty() || bytes.len() > MAX_PEER_MESSAGE_SIZE {
        return Err(PeerContractError::Size);
    }
    let message = PeerMessage::decode(bytes).map_err(|_| PeerContractError::Malformed)?;
    if message.encode_to_vec() != bytes {
        return Err(PeerContractError::Malformed);
    }
    validate_peer_message(&message, receiver_role, state)?;
    Ok(message)
}

pub fn validate_peer_message(
    message: &PeerMessage,
    receiver_role: PeerRole,
    state: PeerLifecycleState,
) -> Result<(), PeerContractError> {
    if !matches!(receiver_role, PeerRole::Master | PeerRole::Slave) {
        return Err(PeerContractError::InvalidRole);
    }
    let header = message
        .header
        .as_ref()
        .ok_or(PeerContractError::InvalidFields)?;
    if ContractVersion::try_from(header.version) != Ok(ContractVersion::ContractVersion1)
        || header.src_pod_id > u16::MAX as u32
        || header.dst_pod_id > u16::MAX as u32
        || (!header.sender_mac.is_empty() && header.sender_mac.len() != 6)
    {
        return Err(PeerContractError::InvalidFields);
    }
    let sender_role =
        PeerRole::try_from(header.sender_role).map_err(|_| PeerContractError::InvalidRole)?;
    let payload = message
        .payload
        .as_ref()
        .ok_or(PeerContractError::InvalidFields)?;

    let fields_valid = match payload {
        Payload::Beacon(_) | Payload::Ping(_) | Payload::Pong(_) | Payload::StopAll(_) => true,
        Payload::JoinGame(value) => value.assigned_role == PeerRole::Slave as i32,
        Payload::ArmTouch(value) => {
            value.round_token != 0
                && (1..=60_000).contains(&value.timeout_ms)
                && value.feedback_mode <= 3
        }
        Payload::SetColor(value) => value.r <= 255 && value.g <= 255 && value.b <= 255,
        Payload::SimulateTouch(value) => value.round_token != 0 && value.pad_index <= 3,
        Payload::TouchEvent(value) => value.round_token != 0 && value.pad_index <= 3,
        Payload::TimeoutEvent(value) => value.round_token != 0,
    };
    if !fields_valid {
        return Err(PeerContractError::InvalidFields);
    }

    let discovery = matches!(
        payload,
        Payload::Beacon(_) | Payload::Ping(_) | Payload::Pong(_)
    );
    let event = matches!(payload, Payload::TouchEvent(_) | Payload::TimeoutEvent(_));
    let expected_sender = if discovery {
        PeerRole::Unspecified
    } else if event {
        PeerRole::Slave
    } else {
        PeerRole::Master
    };
    if sender_role != expected_sender {
        return Err(PeerContractError::InvalidRole);
    }

    let allowed = discovery
        && matches!(
            state,
            PeerLifecycleState::Discovery | PeerLifecycleState::Ready | PeerLifecycleState::Armed
        )
        || match state {
            PeerLifecycleState::Discovery => {
                receiver_role == PeerRole::Slave && matches!(payload, Payload::JoinGame(_))
            }
            PeerLifecycleState::Ready => {
                matches!(payload, Payload::StopAll(_))
                    || (receiver_role == PeerRole::Slave
                        && matches!(payload, Payload::ArmTouch(_) | Payload::SetColor(_)))
                    || (receiver_role == PeerRole::Master && event)
            }
            PeerLifecycleState::Armed => {
                receiver_role == PeerRole::Slave
                    && matches!(payload, Payload::SimulateTouch(_) | Payload::StopAll(_))
            }
            PeerLifecycleState::Unspecified | PeerLifecycleState::Stopped => false,
        };
    if !allowed {
        return Err(PeerContractError::InvalidState);
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::proto::peer::{
        ArmTouch, Beacon, JoinGame, PeerHeader, Ping, Pong, SetColor, SimulateTouch, StopAll,
        TimeoutEvent, TouchEvent,
    };

    fn message(payload: Payload, role: PeerRole) -> PeerMessage {
        PeerMessage {
            header: Some(PeerHeader {
                version: ContractVersion::ContractVersion1 as i32,
                src_pod_id: 1,
                dst_pod_id: 2,
                sender_role: role as i32,
                timestamp_us: 1,
                sequence: 1,
                sender_mac: vec![1, 2, 3, 4, 5, 6],
            }),
            payload: Some(payload),
        }
    }

    #[test]
    fn generated_variants_round_trip_without_independent_contract() {
        let cases = [
            (
                Payload::Beacon(Beacon {}),
                PeerRole::Unspecified,
                PeerRole::Master,
                PeerLifecycleState::Discovery,
            ),
            (
                Payload::Ping(Ping {}),
                PeerRole::Unspecified,
                PeerRole::Master,
                PeerLifecycleState::Discovery,
            ),
            (
                Payload::Pong(Pong {}),
                PeerRole::Unspecified,
                PeerRole::Slave,
                PeerLifecycleState::Discovery,
            ),
            (
                Payload::JoinGame(JoinGame {
                    assigned_role: PeerRole::Slave as i32,
                }),
                PeerRole::Master,
                PeerRole::Slave,
                PeerLifecycleState::Discovery,
            ),
            (
                Payload::ArmTouch(ArmTouch {
                    round_token: 1,
                    timeout_ms: 3000,
                    feedback_mode: 3,
                }),
                PeerRole::Master,
                PeerRole::Slave,
                PeerLifecycleState::Ready,
            ),
            (
                Payload::SetColor(SetColor { r: 1, g: 2, b: 3 }),
                PeerRole::Master,
                PeerRole::Slave,
                PeerLifecycleState::Ready,
            ),
            (
                Payload::StopAll(StopAll {}),
                PeerRole::Master,
                PeerRole::Slave,
                PeerLifecycleState::Ready,
            ),
            (
                Payload::SimulateTouch(SimulateTouch {
                    round_token: 1,
                    pad_index: 0,
                }),
                PeerRole::Master,
                PeerRole::Slave,
                PeerLifecycleState::Armed,
            ),
            (
                Payload::TouchEvent(TouchEvent {
                    round_token: 1,
                    reaction_time_us: 100,
                    pad_index: 0,
                }),
                PeerRole::Slave,
                PeerRole::Master,
                PeerLifecycleState::Ready,
            ),
            (
                Payload::TimeoutEvent(TimeoutEvent { round_token: 1 }),
                PeerRole::Slave,
                PeerRole::Master,
                PeerLifecycleState::Ready,
            ),
        ];
        for (payload, sender, receiver, state) in cases {
            let expected = message(payload, sender);
            let bytes = expected.encode_to_vec();
            assert_eq!(
                decode_peer_message(&bytes, receiver, state).unwrap(),
                expected
            );
        }
    }

    #[test]
    fn malformed_unknown_truncated_and_oversized_fail_closed() {
        let valid = message(Payload::Ping(Ping {}), PeerRole::Unspecified).encode_to_vec();
        assert_eq!(
            decode_peer_message(
                &valid[..valid.len() - 1],
                PeerRole::Master,
                PeerLifecycleState::Discovery
            ),
            Err(PeerContractError::Malformed)
        );
        let mut unknown = valid.clone();
        unknown.extend_from_slice(&[0xf8, 0x01, 0x01]);
        assert_eq!(
            decode_peer_message(&unknown, PeerRole::Master, PeerLifecycleState::Discovery),
            Err(PeerContractError::Malformed)
        );
        assert_eq!(
            decode_peer_message(&[0; 65], PeerRole::Master, PeerLifecycleState::Discovery),
            Err(PeerContractError::Size)
        );
    }

    #[test]
    fn role_and_state_invalid_messages_fail_closed() {
        let invalid_role = message(
            Payload::TimeoutEvent(TimeoutEvent { round_token: 1 }),
            PeerRole::Master,
        )
        .encode_to_vec();
        assert_eq!(
            decode_peer_message(&invalid_role, PeerRole::Master, PeerLifecycleState::Ready),
            Err(PeerContractError::InvalidRole)
        );
        let invalid_state = message(
            Payload::ArmTouch(ArmTouch {
                round_token: 1,
                timeout_ms: 3000,
                feedback_mode: 3,
            }),
            PeerRole::Master,
        )
        .encode_to_vec();
        assert_eq!(
            decode_peer_message(&invalid_state, PeerRole::Slave, PeerLifecycleState::Armed),
            Err(PeerContractError::InvalidState)
        );
        let discovery = message(Payload::Ping(Ping {}), PeerRole::Unspecified).encode_to_vec();
        assert_eq!(
            decode_peer_message(
                &discovery,
                PeerRole::Unspecified,
                PeerLifecycleState::Discovery
            ),
            Err(PeerContractError::InvalidRole)
        );
        assert_eq!(
            decode_peer_message(
                &discovery,
                PeerRole::Master,
                PeerLifecycleState::Unspecified
            ),
            Err(PeerContractError::InvalidState)
        );
    }
}
