/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef PATH_ID_TAG_H
#define PATH_ID_TAG_H

#include "ns3/tag.h"

namespace ns3 {

class PathIdTag : public Tag {
public:
  PathIdTag();
  explicit PathIdTag(uint8_t pathId);

  static TypeId GetTypeId(void);
  virtual TypeId GetInstanceTypeId(void) const;
  virtual void Print(std::ostream &os) const;
  virtual uint32_t GetSerializedSize(void) const;
  virtual void Serialize(TagBuffer i) const;
  virtual void Deserialize(TagBuffer i);

  void SetPathId(uint8_t pathId);
  uint8_t GetPathId(void) const;

private:
  uint8_t m_pathId;
};

} // namespace ns3

#endif /* PATH_ID_TAG_H */
