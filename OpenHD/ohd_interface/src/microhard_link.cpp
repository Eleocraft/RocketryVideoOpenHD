//
// Created by consti10 on 05.04.24.
//

#include "microhard_link.h"

// Master -
static constexpr auto MICROHARD_AIR_IP = "192.168.168.11";
// CLient
static constexpr auto MICROHARD_GND_IP = "192.168.168.12";
// We send data over those port(s)
static constexpr int MICROHARD_UDP_PORT_VIDEO_AIR_TX = 5910;
static constexpr int MICROHARD_UDP_PORT_TELEMETRY_AIR_TX = 5910;

MicrohardLink::MicrohardLink(OHDProfile profile) : m_profile(profile) {
  if (m_profile.is_air) {
    // We send video
    m_video_tx = std::make_unique<openhd::UDPForwarder>(
        MICROHARD_GND_IP, MICROHARD_UDP_PORT_VIDEO_AIR_TX);
    // We send and receive telemetry
    auto cb_telemetry_rx = [this](const uint8_t *data,
                                  const std::size_t data_len) {
      auto shared =
          std::make_shared<std::vector<uint8_t>>(data, data + data_len);
      on_receive_telemetry_data(shared);
    };
    m_telemetry_tx_rx = std::make_unique<openhd::UDPReceiver>(
        MICROHARD_GND_IP, MICROHARD_UDP_PORT_TELEMETRY_AIR_TX, cb_telemetry_rx);
  } else {
    auto cb_video_rx = [this](const uint8_t *payload,
                              const std::size_t payloadSize) {
      on_receive_video_data(0, payload, payloadSize);
    };
    m_video_rx = std::make_unique<openhd::UDPReceiver>(
        MICROHARD_AIR_IP, MICROHARD_UDP_PORT_VIDEO_AIR_TX, cb_video_rx);
    auto cb_telemetry_rx = [this](const uint8_t *data,
                                  const std::size_t data_len) {
      auto shared =
          std::make_shared<std::vector<uint8_t>>(data, data + data_len);
      on_receive_telemetry_data(shared);
    };
    m_telemetry_tx_rx = std::make_unique<openhd::UDPReceiver>(
        MICROHARD_AIR_IP, 5910, cb_telemetry_rx);
  }
}

void MicrohardLink::transmit_telemetry_data(OHDLink::TelemetryTxPacket packet) {
  const auto destination_ip =
      m_profile.is_air ? MICROHARD_GND_IP : MICROHARD_AIR_IP;
  m_telemetry_tx_rx->forwardPacketViaUDP(
      destination_ip, MICROHARD_UDP_PORT_TELEMETRY_AIR_TX, packet.data->data(),
      packet.data->size());
}

void MicrohardLink::transmit_video_data(
    int stream_index,
    const openhd::FragmentedVideoFrame &fragmented_video_frame) {
  assert(m_profile.is_air);
  if (stream_index == 0) {
    for (auto &fragment : fragmented_video_frame.rtp_fragments) {
      m_video_tx->forwardPacketViaUDP(fragment->data(), fragment->size());
    }
  }
}

void MicrohardLink::transmit_audio_data(
    const openhd::AudioPacket &audio_packet) {
  // not impl
}
