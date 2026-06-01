/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "path-id-tag.h"

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED(PathIdTag);

PathIdTag::PathIdTag()
    : Tag(),
      m_pathId(0) {}

PathIdTag::PathIdTag(uint8_t pathId)
    : Tag(),
      m_pathId(pathId) {}

TypeId PathIdTag::GetTypeId(void) {
  static TypeId tid = TypeId("ns3::PathIdTag")
                          .SetParent<Tag>()
                          .AddConstructor<PathIdTag>();
  return tid;
}

TypeId PathIdTag::GetInstanceTypeId(void) const { return GetTypeId(); }

void PathIdTag::Print(std::ostream &os) const { os << "pathId=" << static_cast<uint32_t>(m_pathId); }

uint32_t PathIdTag::GetSerializedSize(void) const { return sizeof(m_pathId); }

void PathIdTag::Serialize(TagBuffer i) const { i.WriteU8(m_pathId); }

void PathIdTag::Deserialize(TagBuffer i) { m_pathId = i.ReadU8(); }

void PathIdTag::SetPathId(uint8_t pathId) { m_pathId = pathId; }

uint8_t PathIdTag::GetPathId(void) const { return m_pathId; }

} // namespace ns3
