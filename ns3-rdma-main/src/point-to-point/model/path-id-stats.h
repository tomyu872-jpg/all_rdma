/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef PATH_ID_STATS_H
#define PATH_ID_STATS_H

#include <array>
#include <cstdint>
#include <unordered_map>

namespace ns3 {

static const uint32_t kPathChoiceCount = 4;
using PathIdCounters = std::array<int64_t, kPathChoiceCount>;

extern std::unordered_map<uint64_t, PathIdCounters> g_pathIdCountSwitch;
extern std::unordered_map<uint64_t, PathIdCounters> g_pathIdCountAck;

uint64_t GetPathStatsQpKey(uint32_t dip, uint16_t sport, uint16_t dport, uint16_t pg);
PathIdCounters& GetPathIdCountSwitch(uint64_t qpKey);
PathIdCounters& GetPathIdCountAck(uint64_t qpKey);

} // namespace ns3

#endif /* PATH_ID_STATS_H */
