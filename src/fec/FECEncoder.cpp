#include "FECEncoder.h"

#include <cassert>
#include <chrono>
#include <cstring>
#include <memory>

#include "../external/fec/fec_base.h"

#include "FECConstants.hpp"
#include "BlockSizeHelper.hpp"

void FECEncoder::encode_block(
    std::vector<std::shared_ptr<std::vector<uint8_t>>> data_packets,
    int n_secondary_fragments) {
  assert(data_packets.size() <= MAX_N_P_FRAGMENTS_PER_BLOCK);
  assert(n_secondary_fragments <= MAX_N_S_FRAGMENTS_PER_BLOCK);
  const auto n_primary_fragments = data_packets.size();
  // nice to have statistic
  m_block_sizes.add(n_primary_fragments);
  if (m_block_sizes.get_delta_since_last_reset() >= std::chrono::seconds(1)) {
    // wifibroadcast::log::get_default()->debug("Block sizes:
    // {}",m_block_sizes.getAvgReadable());
    m_curr_fec_block_sizes = m_block_sizes.getMinMaxAvg();
    m_block_sizes.reset();
  }
  m_primary_fragments_data_p.clear();
  m_max_packet_size=-1;
  m_fragment_index=0;
  m_fec_payload_hdr.block_idx = m_curr_block_idx;
  m_curr_block_idx++;
  m_fec_payload_hdr.n_primary_fragments = n_primary_fragments;
  // write and forward all the data packets first
  // also calculate the size of the biggest data packet
  for (int i = 0; i < data_packets.size(); i++) {
    const auto& data_fragment = data_packets[i];
    // wifibroadcast::log::get_default()->debug("In:{}",(int)data_fragment->size());
    assert(!data_fragment->empty());
    assert(data_fragment->size() <= FEC_PACKET_MAX_PAYLOAD_SIZE);
    create_forward_save_fragment(data_fragment->data(),data_fragment->size());
  }
  create_fec_packets(n_secondary_fragments);
}

void FECEncoder::fragment_and_encode(const uint8_t* data,int data_len,int n_primary_fragments,int n_secondary_fragments) {
  FECPayloadHdr header{};
  header.block_idx = m_curr_block_idx;
  m_curr_block_idx++;
  header.n_primary_fragments = n_primary_fragments;
  int consumed=0;
  int count=0;
  while (consumed<data_len){
    const int remaining=data_len-consumed;
    // We want to distribute the data as evenly as possible into the n_primary_fragments
    const int max_fragment_size=blocksize::div_ceil(data_len,n_primary_fragments-count);
    if(remaining<=max_fragment_size){
      // we are done
      create_forward_save_fragment(data+consumed,remaining);
      create_fec_packets(n_secondary_fragments);
      consumed+=remaining;
      break ;
    }{
      // not yet done
      create_forward_save_fragment(data+consumed,max_fragment_size);
      consumed+=max_fragment_size;
    }
  }
  assert(consumed==data_len);
}

void FECEncoder::create_forward_save_fragment(const uint8_t* data,int data_len) {
  m_fec_payload_hdr.fragment_idx = m_fragment_index;
  m_fec_payload_hdr.data_size = data_len;
  auto buffer_p = m_block_buffer[m_fragment_index].data();
  // copy over the header
  memcpy(buffer_p, (uint8_t*)&m_fec_payload_hdr, sizeof(FECPayloadHdr));
  // write the actual data
  memcpy(buffer_p + sizeof(FECPayloadHdr),data,
         data_len);
  // zero out the remaining bytes such that FEC always sees zeroes
  // same is done on the rx. These zero bytes are never transmitted via wifi
  const auto writtenDataSize = sizeof(FECPayloadHdr) + data_len;
  memset(buffer_p + writtenDataSize, 0,
         MAX_PAYLOAD_BEFORE_FEC - writtenDataSize);
  m_max_packet_size = std::max(m_max_packet_size, data_len);
  // we can forward the data packet immediately via the callback
  if (m_out_cb) {
    m_out_cb(buffer_p, static_cast<int>(writtenDataSize));
  }
  // Store a pointer where the FEC data begins for performing the FEC step later on -
  // NOTE: The fec data includes FECPayloadHdr::data_size but not the other stuff from the header
  m_primary_fragments_data_p.push_back(buffer_p + sizeof(FECPayloadHdr) -sizeof(uint16_t));
  m_fragment_index++;
}

void FECEncoder::create_fec_packets(int n_secondary_fragments) {
  // then we create as many FEC packets as needed
  if (n_secondary_fragments == 0) {
    // wifibroadcast::log::get_default()->debug("No FEC step performed");
    //  no FEC step is actually performed, usefully for debugging / performance
    //  evaluation
    return;
  }
  const auto before = std::chrono::steady_clock::now();
  // data pointers where the FEC data is stored
  std::vector<uint8_t*> secondary_fragments_data_p;
  for (int i = 0; i < n_secondary_fragments; i++) {
    auto fragment_index = i + m_fragment_index;
    auto buffer_p = m_block_buffer[fragment_index].data();
    m_fec_payload_hdr.fragment_idx = fragment_index;
    // copy over the header
    memcpy(buffer_p, (uint8_t*)&m_fec_payload_hdr, sizeof(FECPayloadHdr));
    // where the FEC packet correction data is written to
    secondary_fragments_data_p.push_back(buffer_p + sizeof(FECPayloadHdr) -
                                         sizeof(uint16_t));
  }
  fec_encode2(m_max_packet_size + sizeof(uint16_t), m_primary_fragments_data_p,
              secondary_fragments_data_p);
  m_fec_block_encode_time.add(std::chrono::steady_clock::now() - before);
  if (m_fec_block_encode_time.get_delta_since_last_reset() >=
      std::chrono::seconds(1)) {
    // wifibroadcast::log::get_default()->debug("FEC encode
    // time:{}",m_fec_block_encode_time.getAvgReadable());
    m_curr_fec_block_encode_time = m_fec_block_encode_time.getMinMaxAvg();
    m_fec_block_encode_time.reset();
  }
  // and forward all the FEC correction packets
  for (int i = 0; i < n_secondary_fragments; i++) {
    auto fragment_index = i + m_fragment_index;
    if (m_out_cb) {
      const int fec_packet_len=sizeof(FECPayloadHdr) + m_max_packet_size;
      m_out_cb(m_block_buffer[fragment_index].data(),fec_packet_len);
    }
  }
}
