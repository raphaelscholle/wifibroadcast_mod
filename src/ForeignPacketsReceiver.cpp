//
// Created by consti10 on 17.12.22.
//

#include "ForeignPacketsReceiver.h"

#include <utility>

ForeignPacketsReceiver::ForeignPacketsReceiver(std::vector<std::string> wlans,std::vector<uint16_t> openhd_radio_ports):
  m_openhd_radio_ports(std::move(openhd_radio_ports)) {
  auto cb=[this](const uint8_t wlan_idx, const pcap_pkthdr &hdr, const uint8_t *pkt){
    on_foreign_packet(wlan_idx,hdr,pkt);
  };
  auto cb2=[this](){
  };
  m_receiver=std::make_unique<MultiRxPcapReceiver>(wlans,100,std::chrono::milliseconds(100),cb,cb2);
  m_receiver->loop();
  m_thread=std::make_unique<std::thread>(&ForeignPacketsReceiver::m_loop, this);
}

void ForeignPacketsReceiver::on_foreign_packet(const uint8_t wlan_idx,const pcap_pkthdr &hdr,const uint8_t *pkt) {
  wifibroadcast::log::get_default()->debug("X got packet");
}

void ForeignPacketsReceiver::m_loop() {
  m_receiver->loop();
}
