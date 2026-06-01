/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "path-id-stats.h"

namespace ns3 {

std::unordered_map<uint64_t, PathIdCounters> g_pathIdCountSwitch;
std::unordered_map<uint64_t, PathIdCounters> g_pathIdCountAck;

uint64_t GetPathStatsQpKey(uint32_t dip, uint16_t sport, uint16_t dport, uint16_t pg) {
	return (static_cast<uint64_t>(dip) << 32) | (static_cast<uint64_t>(sport) << 16) |
		   static_cast<uint64_t>(dport) | static_cast<uint64_t>(pg);
}

PathIdCounters& GetPathIdCountSwitch(uint64_t qpKey) {
	auto inserted = g_pathIdCountSwitch.emplace(qpKey, PathIdCounters{-1, 0, 0, 0});
	return inserted.first->second;
}

PathIdCounters& GetPathIdCountAck(uint64_t qpKey) {
	auto inserted = g_pathIdCountAck.emplace(qpKey, PathIdCounters{0, 0, 0, 0});
	return inserted.first->second;
}

} // namespace ns3
