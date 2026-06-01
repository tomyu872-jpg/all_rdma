#include "switch-node.h"

#include "assert.h"
#include "ns3/boolean.h"
#include "ns3/conweave-routing.h"
#include "ns3/double.h"
#include "ns3/flow-id-tag.h"
#include "ns3/int-header.h"
#include "ns3/ipv4-header.h"
#include "ns3/ipv4.h"
#include "ns3/letflow-routing.h"
#include "ns3/packet.h"
#include "ns3/path-id-stats.h"
#include "ns3/path-id-tag.h"
#include "ns3/pause-header.h"
#include "ns3/settings.h"
#include "ns3/uinteger.h"
#include "ppp-header.h"
#include "qbb-net-device.h"

namespace ns3 {

using namespace std;

TypeId SwitchNode::GetTypeId(void) {
    static TypeId tid =
        TypeId("ns3::SwitchNode")
            .SetParent<Node>()
            .AddConstructor<SwitchNode>()
            .AddAttribute("EcnEnabled", "Enable ECN marking.", BooleanValue(false),
                          MakeBooleanAccessor(&SwitchNode::m_ecnEnabled), MakeBooleanChecker())
            .AddAttribute("CcMode", "CC mode.", UintegerValue(0),
                          MakeUintegerAccessor(&SwitchNode::m_ccMode),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("AckHighPrio", "Set high priority for ACK/NACK or not", UintegerValue(0),
                          MakeUintegerAccessor(&SwitchNode::m_ackHighPrio),
                          MakeUintegerChecker<uint32_t>());
    return tid;
}

SwitchNode::SwitchNode() {
    m_ecmpSeed = m_id;
    m_isToR = false;
    m_node_type = 1;
    m_isToR = false;
    m_abandonedPathId = -1;
    m_abandonedPathId2 = -1;
    m_drill_candidate = 2;
    m_mmu = CreateObject<SwitchMmu>();
    // Conga's Callback for switch functions
    m_mmu->m_congaRouting.SetSwitchSendCallback(MakeCallback(&SwitchNode::DoSwitchSend, this));
    m_mmu->m_congaRouting.SetSwitchSendToDevCallback(
        MakeCallback(&SwitchNode::SendToDevContinue, this));
    // ConWeave's Callback for switch functions
    m_mmu->m_conweaveRouting.SetSwitchSendCallback(MakeCallback(&SwitchNode::DoSwitchSend, this));
    m_mmu->m_conweaveRouting.SetSwitchSendToDevCallback(
        MakeCallback(&SwitchNode::SendToDevContinue, this));

    for (uint32_t i = 0; i < pCnt; i++) {
        m_txBytes[i] = 0;
    }
}

/**
 * @brief Load Balancing
 */
#if 0
uint32_t SwitchNode::DoLbFlowECMP(Ptr<Packet> p, CustomHeader& ch,  // 实验一指定路径的单路径
                                  const std::vector<int>& nexthops) {
    // pick one next hop based on hash
    union {
        uint8_t u8[4 + 4 + 2 + 2];
        uint32_t u32[3];
    } buf;
    buf.u32[0] = ch.sip;
    buf.u32[1] = ch.dip;
    if (ch.l3Prot == 0x6)
        buf.u32[2] = ch.tcp.sport | ((uint32_t)ch.tcp.dport << 16);
    else if (ch.l3Prot == 0x11)  // XXX RDMA traffic on UDP
        buf.u32[2] = ch.udp.sport | ((uint32_t)ch.udp.dport << 16);
    else if (ch.l3Prot == 0xFC || ch.l3Prot == 0xFD)  // ACK or NACK
        buf.u32[2] = ch.ack.sport | ((uint32_t)ch.ack.dport << 16);
    else {
        std::cout << "[ERROR] Sw(" << m_id << ")," << PARSE_FIVE_TUPLE(ch)
                  << "Cannot support other protoocls than TCP/UDP (l3Prot:" << ch.l3Prot << ")"
                  << std::endl;
        assert(false && "Cannot support other protoocls than TCP/UDP");
    }
    uint32_t hashVal = EcmpHash(buf.u8, 12, m_ecmpSeed);
    uint32_t idx = hashVal % nexthops.size();
    uint32_t base_sip = 184550913;  // 第一个主机的sip，每个主机之间差512
    if (nexthops.size() != 1)       // nexthops[idx]：1，2，3，4
    {
        if (ch.sip == 184550913) {  // 第一个流走路径1
            idx = 1;
        } else if (ch.sip == 184550913 + 512) {  // 第二个流走路径2
            idx = 2;
        } else if (ch.sip == 184550913 + 512 * 2) {  // 第三个流走路径2制造拥塞
            idx = 2;
        } else if (ch.sip == 184550913 + 512 * 3) {  // 第四个流走路径3
            idx = 3;
        } else if (ch.sip == 184550913 + 512 * 4) {  // 第五个流走路径4制造拥塞
            idx = 3;
            cout<<"5th flow goes to path 4"<<endl;
        }
    }
    return nexthops[idx];
}
#endif


/*
实验三指定路径的单路径
*/
#if 0
uint32_t SwitchNode::DoLbFlowECMP(Ptr<Packet> p, CustomHeader& ch,  
                                  const std::vector<int>& nexthops) {
    // pick one next hop based on hash
    union {
        uint8_t u8[4 + 4 + 2 + 2];
        uint32_t u32[3];
    } buf;
    buf.u32[0] = ch.sip;
    buf.u32[1] = ch.dip;
    if (ch.l3Prot == 0x6)
        buf.u32[2] = ch.tcp.sport | ((uint32_t)ch.tcp.dport << 16);
    else if (ch.l3Prot == 0x11)  // XXX RDMA traffic on UDP
        buf.u32[2] = ch.udp.sport | ((uint32_t)ch.udp.dport << 16);
    else if (ch.l3Prot == 0xFC || ch.l3Prot == 0xFD)  // ACK or NACK
        buf.u32[2] = ch.ack.sport | ((uint32_t)ch.ack.dport << 16);
    else {
        std::cout << "[ERROR] Sw(" << m_id << ")," << PARSE_FIVE_TUPLE(ch)
                  << "Cannot support other protoocls than TCP/UDP (l3Prot:" << ch.l3Prot << ")"
                  << std::endl;
        assert(false && "Cannot support other protoocls than TCP/UDP");
    }
    uint32_t hashVal = EcmpHash(buf.u8, 12, m_ecmpSeed);
    uint32_t idx = hashVal % nexthops.size();
    uint32_t base_sip = 184550913;  // 第一个主机的sip，每个主机之间差512
    if (nexthops.size() != 1)       // 第 1~4 个流 -> 路径 0,1,2,3  第 5~8 个流 -> 路径 0,1,2,3
    {
        if (ch.sip >= base_sip) {
            uint32_t offset = ch.sip - base_sip;
            if (offset % 512 == 0) {
                uint32_t flowId = offset / 512;
                if (flowId < 8) {
                    idx = flowId % 4;
                }
            }
        }

    }
    return nexthops[idx];
}
#endif



/*
实验二指定路径的单路径，所有流都走一条路径
*/
#if 0
uint32_t SwitchNode::DoLbFlowECMP(Ptr<Packet> p, CustomHeader& ch,  
                                  const std::vector<int>& nexthops) {
    // pick one next hop based on hash
    union {
        uint8_t u8[4 + 4 + 2 + 2];
        uint32_t u32[3];
    } buf;
    buf.u32[0] = ch.sip;
    buf.u32[1] = ch.dip;
    if (ch.l3Prot == 0x6)
        buf.u32[2] = ch.tcp.sport | ((uint32_t)ch.tcp.dport << 16);
    else if (ch.l3Prot == 0x11)  // XXX RDMA traffic on UDP
        buf.u32[2] = ch.udp.sport | ((uint32_t)ch.udp.dport << 16);
    else if (ch.l3Prot == 0xFC || ch.l3Prot == 0xFD)  // ACK or NACK
        buf.u32[2] = ch.ack.sport | ((uint32_t)ch.ack.dport << 16);
    else {
        std::cout << "[ERROR] Sw(" << m_id << ")," << PARSE_FIVE_TUPLE(ch)
                  << "Cannot support other protoocls than TCP/UDP (l3Prot:" << ch.l3Prot << ")"
                  << std::endl;
        assert(false && "Cannot support other protoocls than TCP/UDP");
    }
    uint32_t hashVal = EcmpHash(buf.u8, 12, m_ecmpSeed);
    uint32_t idx = hashVal % nexthops.size();
    if (nexthops.size() != 1)       // nexthops[idx]：1，2，3，4
    {
        idx=2;
    }

    return nexthops[idx];
}
#endif

#if 0
uint32_t SwitchNode::DoLbFlowECMP(Ptr<const Packet> p,  CustomHeader &ch,   //原始单路径
                                  const std::vector<int> &nexthops) {
    // pick one next hop based on hash
    union {
        uint8_t u8[4 + 4 + 2 + 2];
        uint32_t u32[3];
    } buf;
    buf.u32[0] = ch.sip;
    buf.u32[1] = ch.dip;
    if (ch.l3Prot == 0x6)
        buf.u32[2] = ch.tcp.sport | ((uint32_t)ch.tcp.dport << 16);
    else if (ch.l3Prot == 0x11)  // XXX RDMA traffic on UDP
        buf.u32[2] = ch.udp.sport | ((uint32_t)ch.udp.dport << 16);
    else if (ch.l3Prot == 0xFC || ch.l3Prot == 0xFD)  // ACK or NACK
        buf.u32[2] = ch.ack.sport | ((uint32_t)ch.ack.dport << 16);
    else {
        std::cout << "[ERROR] Sw(" << m_id << ")," << PARSE_FIVE_TUPLE(ch)
                  << "Cannot support other protoocls than TCP/UDP (l3Prot:" << ch.l3Prot << ")"
                  << std::endl;
        assert(false && "Cannot support other protoocls than TCP/UDP");
    }

    uint32_t hashVal = EcmpHash(buf.u8, 12, m_ecmpSeed);
    uint32_t idx = hashVal % nexthops.size();
    return nexthops[idx];
}
#endif

#if 0
uint32_t SwitchNode::DoLbFlowECMP(Ptr<const Packet> p, CustomHeader &ch,   //单路径
                                  const std::vector<int> &nexthops) {
    uint32_t base_sip = 184549377;
    // pick one next hop based on hash
    union {
        uint8_t u8[4 + 4 + 2 + 2];
        uint32_t u32[3];
    } buf;
    buf.u32[0] = ch.sip;
    buf.u32[1] = ch.dip;
    if (ch.l3Prot == 0x6)
        buf.u32[2] = ch.tcp.sport | ((uint32_t)ch.tcp.dport << 16);
    else if (ch.l3Prot == 0x11)  // XXX RDMA traffic on UDP
        buf.u32[2] = ch.udp.sport | ((uint32_t)ch.udp.dport << 16);
    else if (ch.l3Prot == 0xFC || ch.l3Prot == 0xFD){  // ACK or NACK
        buf.u32[2] = ch.ack.sport | ((uint32_t)ch.ack.dport << 16);
        uint32_t hashVal = EcmpHash(buf.u8, 12, m_ecmpSeed);
        uint32_t idx = hashVal % nexthops.size();
        return nexthops[idx];
    }
    else {
        std::cout << "[ERROR] Sw(" << m_id << ")," << PARSE_FIVE_TUPLE(ch)
                  << "Cannot support other protoocls than TCP/UDP (l3Prot:" << ch.l3Prot << ")"
                  << std::endl;
        assert(false && "Cannot support other protoocls than TCP/UDP");
    }
    uint32_t hashVal = EcmpHash(buf.u8, 12, m_ecmpSeed);
    if (nexthops.size() != 1 && ch.sip == 184550913) {
        uint32_t hash_val1 = (ch.sip-base_sip)/4096;//指定路径
        uint32_t hash_val2 = (ch.sip-base_sip)/256;//指定路径
        // std::cout<<"sip:"<<ch.sip<<std::endl;
        uint32_t idx = (hash_val1+hash_val2+mp_count) % (nexthops.size());

        return nexthops[idx];
    }
    uint32_t idx = hashVal % nexthops.size();
    return nexthops[idx];
}
#endif
/*
实验一三平均多路径
*/ 
#if 0
uint32_t SwitchNode::DoLbFlowECMP(Ptr<Packet> p, CustomHeader& ch,  // 平均多路径
                                  const std::vector<int>& nexthops) {
    // 基于序列号(seq)的路由选择
    uint32_t seq = 0;
    uint32_t base_sip = 184550913;

    // 提取序列号，根据协议类型不同字段
    if (ch.l3Prot == 0x6) {  // TCP
        seq = ch.tcp.seq;
    } else if (ch.l3Prot == 0x11) {  // UDP
        seq = ch.udp.seq;
    } else if (ch.l3Prot == 0xFC || ch.l3Prot == 0xFD) {  // ACK or NACK
        seq = ch.ack.seq;
        // pick one next hop based on hash
        union {
            uint8_t u8[4 + 4 + 2 + 2];
            uint32_t u32[3];
        } buf;
        buf.u32[0] = ch.sip;
        buf.u32[1] = ch.dip;
        if (ch.l3Prot == 0x6)
            buf.u32[2] = ch.tcp.sport | ((uint32_t)ch.tcp.dport << 16);
        else if (ch.l3Prot == 0x11)  // XXX RDMA traffic on UDP
            buf.u32[2] = ch.udp.sport | ((uint32_t)ch.udp.dport << 16);
        else if (ch.l3Prot == 0xFC || ch.l3Prot == 0xFD)  // ACK or NACK
            buf.u32[2] = ch.ack.sport | ((uint32_t)ch.ack.dport << 16);
        else {
            std::cout << "[ERROR] Sw(" << m_id << ")," << PARSE_FIVE_TUPLE(ch)
                      << "Cannot support other protoocls than TCP/UDP (l3Prot:" << ch.l3Prot <<
                      ")"
                      << std::endl;
            assert(false && "Cannot support other protoocls than TCP/UDP");
        }

        uint32_t hashVal = EcmpHash(buf.u8, 12, m_ecmpSeed);
        uint32_t idx = hashVal % nexthops.size();
        return nexthops[idx];
    }

    else {
        std::cout << "[ERROR] Sw(" << m_id << ")," << PARSE_FIVE_TUPLE(ch)
                  << "Cannot support other protocols than TCP/UDP (l3Prot:" << ch.l3Prot << ")"
                  << std::endl;
        assert(false && "Cannot support other protocols than TCP/UDP");
    }
    uint32_t off = (seq / PACKET_SIZE) % mp_count;
    if (nexthops.size() != 1 ) {
        return nexthops[off % nexthops.size()];
    }

    return nexthops[0];
}
#endif


/*
实验二平均多路径
短流只走俩条
*/ 
#if 0
uint32_t SwitchNode::DoLbFlowECMP(Ptr<Packet> p, CustomHeader& ch,  // 平均多路径
                                  const std::vector<int>& nexthops) {
    // 基于序列号(seq)的路由选择
    uint32_t seq = 0;
    uint32_t base_sip = 184550913;

    // 提取序列号，根据协议类型不同字段
    if (ch.l3Prot == 0x6) {  // TCP
        seq = ch.tcp.seq;
    } else if (ch.l3Prot == 0x11) {  // UDP
        seq = ch.udp.seq;
    } else if (ch.l3Prot == 0xFC || ch.l3Prot == 0xFD) {  // ACK or NACK
        seq = ch.ack.seq;
        // pick one next hop based on hash
        union {
            uint8_t u8[4 + 4 + 2 + 2];
            uint32_t u32[3];
        } buf;
        buf.u32[0] = ch.sip;
        buf.u32[1] = ch.dip;
        if (ch.l3Prot == 0x6)
            buf.u32[2] = ch.tcp.sport | ((uint32_t)ch.tcp.dport << 16);
        else if (ch.l3Prot == 0x11)  // XXX RDMA traffic on UDP
            buf.u32[2] = ch.udp.sport | ((uint32_t)ch.udp.dport << 16);
        else if (ch.l3Prot == 0xFC || ch.l3Prot == 0xFD)  // ACK or NACK
            buf.u32[2] = ch.ack.sport | ((uint32_t)ch.ack.dport << 16);
        else {
            std::cout << "[ERROR] Sw(" << m_id << ")," << PARSE_FIVE_TUPLE(ch)
                      << "Cannot support other protoocls than TCP/UDP (l3Prot:" << ch.l3Prot <<
                      ")"
                      << std::endl;
            assert(false && "Cannot support other protoocls than TCP/UDP");
        }

        uint32_t hashVal = EcmpHash(buf.u8, 12, m_ecmpSeed);
        uint32_t idx = hashVal % nexthops.size();
        return nexthops[idx];
    }

    else {
        std::cout << "[ERROR] Sw(" << m_id << ")," << PARSE_FIVE_TUPLE(ch)
                  << "Cannot support other protocols than TCP/UDP (l3Prot:" << ch.l3Prot << ")"
                  << std::endl;
        assert(false && "Cannot support other protocols than TCP/UDP");
    }
    uint32_t active_mp = (ch.sip == base_sip) ? mp_count : 2;
    uint32_t off = (seq / PACKET_SIZE) % active_mp;
    if (nexthops.size() != 1 ) {
        return nexthops[off % nexthops.size()];
    }                             

    return nexthops[0];
}
#endif

#if 0
uint32_t SwitchNode::DoLbFlowECMP(Ptr<Packet> p, CustomHeader& ch,  // 选择ack多路径
                                  const std::vector<int>& nexthops) {
    // 基于序列号(seq)的路由选择
    uint32_t seq = 0;
    uint16_t qpSport = 0;
    uint16_t qpDport = 0;
    uint16_t qpPg = 0;
    uint32_t base_sip = 184550913;

    // 提取序列号，根据协议类型不同字段
    if (ch.l3Prot == 0x6) {  // TCP
        seq = ch.tcp.seq;
        qpSport = ch.tcp.sport;
        qpDport = ch.tcp.dport;
    } else if (ch.l3Prot == 0x11) {  // UDP
        seq = ch.udp.seq;
        qpSport = ch.udp.sport;
        qpDport = ch.udp.dport;
        qpPg = ch.udp.pg;
    } else if (ch.l3Prot == 0xFC || ch.l3Prot == 0xFD) {  // ACK or NACK
        seq = ch.ack.seq;
        // pick one next hop based on hash
        union {
            uint8_t u8[4 + 4 + 2 + 2];
            uint32_t u32[3];
        } buf;
        buf.u32[0] = ch.sip;
        buf.u32[1] = ch.dip;
        if (ch.l3Prot == 0x6)
            buf.u32[2] = ch.tcp.sport | ((uint32_t)ch.tcp.dport << 16);
        else if (ch.l3Prot == 0x11)  // XXX RDMA traffic on UDP
            buf.u32[2] = ch.udp.sport | ((uint32_t)ch.udp.dport << 16);
        else if (ch.l3Prot == 0xFC || ch.l3Prot == 0xFD)  // ACK or NACK
            buf.u32[2] = ch.ack.sport | ((uint32_t)ch.ack.dport << 16);
        else {
            std::cout << "[ERROR] Sw(" << m_id << ")," << PARSE_FIVE_TUPLE(ch)
                      << "Cannot support other protoocls than TCP/UDP (l3Prot:" << ch.l3Prot <<
                      ")"
                      << std::endl;
            assert(false && "Cannot support other protoocls than TCP/UDP");
        }

        uint32_t hashVal = EcmpHash(buf.u8, 12, m_ecmpSeed);
        uint32_t idx = hashVal % nexthops.size();
        return nexthops[idx];
    }

    else {
        std::cout << "[ERROR] Sw(" << m_id << ")," << PARSE_FIVE_TUPLE(ch)
                  << "Cannot support other protocols than TCP/UDP (l3Prot:" << ch.l3Prot << ")"
                  << std::endl;
        assert(false && "Cannot support other protocols than TCP/UDP");
    }

    // 基于序列号选择路由
    // uint32_t hash_val = (ch.sip - base_sip) / 4096;
    //     uint32_t hash_val1 = (ch.sip - base_sip) / 4096;  // 指定路径，第几组
    // uint32_t hash_val2 = (ch.sip - base_sip) / 256;   // 指定路径，一组中的第几个
    // // std::cout<<"sip:"<<ch.sip<<std::endl;
    // uint32_t idx = (hash_val1 + hash_val2 + mp_count) % (nexthops.size() - mp_count);

    // 使用 dip/sport/dport/pg 还原该包所属 qp 队列，并按 qp 做 4 路统计。
    uint64_t qpKey = GetPathStatsQpKey(ch.dip, qpSport, qpDport, qpPg);
    PathIdCounters& switchCounters = GetPathIdCountSwitch(qpKey);
    PathIdCounters& ackCounters = GetPathIdCountAck(qpKey);
    uint32_t pathCount = 4;

    // 目标流在4号交换机上的选择
    uint32_t off = (seq / PACKET_SIZE) % pathCount;
    // 如果是第一次看到这个流，直接选择第一条路径，因为最开始会发送俩个off为0的包
    // 根据发送包的sip来选择
    if (nexthops.size() != 1 ) {
        if (switchCounters[0] == -1) {
            switchCounters[0] = 0;
            return nexthops[0];
        }
        //cout << "-----------交换机选择路径-----------" << endl;
        uint32_t bestOff = off;
        int64_t bestDiff = static_cast<int64_t>(switchCounters[off]) -
                           static_cast<int64_t>(ackCounters[off]);
        for (uint32_t i = 0; i < pathCount; ++i) {
            int64_t diff = static_cast<int64_t>(switchCounters[i]) -
                           static_cast<int64_t>(ackCounters[i]);
        if (ch.sip == 184550913 + 512 * 2) {
            // 打印每条路径的发送数量和ack数量
                 cout << i << "路径发送数量:" << switchCounters[i]
                      << ",ack数量:" << ackCounters[i] <<
                      ",差值为"<<switchCounters[i]-ackCounters[i]<<endl;
        }
            if (diff < bestDiff) {
                bestDiff = diff;
                bestOff = i;
            }
        }
        // cout << "选择的路径:" << bestOff << endl;
        off = bestOff;
        // 给每个包打上路径ID标签，并更新g_pathIdCountSwitch
        uint8_t pathId = static_cast<uint8_t>(off & 0xff);
        if (ch.l3Prot == 0x11) {
            ch.udp.pathId = pathId;
        }
        p->AddPacketTag(PathIdTag(pathId));
        if (pathId < pathCount) {
            switchCounters[pathId]++;
        }
        return nexthops[off % nexthops.size()];
    }

    return nexthops[0];
}
#endif

/*
实验一选择ack多路径+路径替换
短流走4条多路径
裁剪1条路径
*/ 
#if 0
uint32_t SwitchNode::DoLbFlowECMP(Ptr<Packet> p, CustomHeader& ch,  // 选择ack多路径+路径替换
                                  const std::vector<int>& nexthops) {
    // 基于序列号(seq)的路由选择
    uint32_t seq = 0;
    uint16_t qpSport = 0;
    uint16_t qpDport = 0;
    uint16_t qpPg = 0;
    uint32_t base_sip = 184550913;
    // cout << "ch.sip " << ch.sip << endl;
    // 提取序列号，根据协议类型不同字段
    if (ch.l3Prot == 0x6) {  // TCP
        seq = ch.tcp.seq;
        qpSport = ch.tcp.sport;
        qpDport = ch.tcp.dport;
    } else if (ch.l3Prot == 0x11) {  // UDP
        seq = ch.udp.seq;
        qpSport = ch.udp.sport;
        qpDport = ch.udp.dport;
        qpPg = ch.udp.pg;
    } else if (ch.l3Prot == 0xFC || ch.l3Prot == 0xFD) {  // ACK or NACK
        seq = ch.ack.seq;
        // pick one next hop based on hash
        union {
            uint8_t u8[4 + 4 + 2 + 2];
            uint32_t u32[3];
        } buf;
        buf.u32[0] = ch.sip;
        buf.u32[1] = ch.dip;
        if (ch.l3Prot == 0x6)
            buf.u32[2] = ch.tcp.sport | ((uint32_t)ch.tcp.dport << 16);
        else if (ch.l3Prot == 0x11)  // XXX RDMA traffic on UDP
            buf.u32[2] = ch.udp.sport | ((uint32_t)ch.udp.dport << 16);
        else if (ch.l3Prot == 0xFC || ch.l3Prot == 0xFD)  // ACK or NACK
            buf.u32[2] = ch.ack.sport | ((uint32_t)ch.ack.dport << 16);
        else {
            std::cout << "[ERROR] Sw(" << m_id << ")," << PARSE_FIVE_TUPLE(ch)
                      << "Cannot support other protoocls than TCP/UDP (l3Prot:" << ch.l3Prot << ")"
                      << std::endl;
            assert(false && "Cannot support other protoocls than TCP/UDP");
        }

        uint32_t hashVal = EcmpHash(buf.u8, 12, m_ecmpSeed);
        uint32_t idx = hashVal % nexthops.size();
        return nexthops[idx];
    }

    else {
        std::cout << "[ERROR] Sw(" << m_id << ")," << PARSE_FIVE_TUPLE(ch)
                  << "Cannot support other protocols than TCP/UDP (l3Prot:" << ch.l3Prot << ")"
                  << std::endl;
        assert(false && "Cannot support other protocols than TCP/UDP");
    }

    // 使用 dip/sport/dport/pg 还原该包所属 qp 队列，并按 qp 做 4 路统计。
    uint64_t qpKey = GetPathStatsQpKey(ch.dip, qpSport, qpDport, qpPg);
    PathIdCounters& switchCounters = GetPathIdCountSwitch(qpKey);
    PathIdCounters& ackCounters = GetPathIdCountAck(qpKey);
    uint32_t pathCount = 4;
    // 目标流在4号交换机上的选择
    uint32_t off = (seq / PACKET_SIZE) % pathCount;
    // 如果是第一次看到这个流，直接选择第一条路径，因为最开始会发送俩个off为0的包
    // 根据发送包的sip来选择
    if (nexthops.size() != 1) {
        // if (ch.sip == base_sip) {
        // for(auto i:nexthops){
        //     cout<<"nexthops:"<<i<<" ";
        // }
        // cout<<endl;
        if (switchCounters[0] == -1) {
            switchCounters[0] = 0;
            return nexthops[0];
        }
        // cout << "-----------交换机选择路径-----------" << endl;

        // Find max(switch-ack) path and the min/second-min ACK counts.
        int64_t minAck = std::numeric_limits<int64_t>::max();
        int64_t secondMinAck = std::numeric_limits<int64_t>::max();
        int64_t maxDiff = std::numeric_limits<int64_t>::min();
        uint32_t maxDiffPath = 0;
        for (uint32_t i = 0; i < pathCount; ++i) {
            int64_t ack = ackCounters[i];
            if (ack < minAck) {
                secondMinAck = minAck;
                minAck = ack;
                maxDiffPath = i;
            } else if (ack < secondMinAck) {
                secondMinAck = ack;
            }

            int64_t diff =
                static_cast<int64_t>(switchCounters[i]) - static_cast<int64_t>(ackCounters[i]);
            if (diff > maxDiff) {
                maxDiff = diff;
            }
        }
        cout << "统计结果 -> minAck:" << minAck << ", secondMinAck:" << secondMinAck
             << ", maxDiffPath:" << maxDiffPath << ", maxDiff:" << maxDiff << endl;

        for (uint32_t i = 0; i < pathCount; ++i) {
            // 打印每条路径的发送数量和ack数量
            cout << i << "路径发送数量:" << switchCounters[i] << ",ack数量:" << ackCounters[i]
                 << ",差值为" << switchCounters[i] - ackCounters[i] << endl;
        }

        // Trigger once: deprecate the max-diff path and shift all future matches to
        // nexthops[4].
        if (m_abandonedPathId < 0 && secondMinAck != std::numeric_limits<int64_t>::max() &&
            (secondMinAck - minAck) > maxDiff ) {
            cout << "更新路径" << endl;
            m_abandonedPathId = static_cast<int>(maxDiffPath);
            cout << "废弃路径:" << m_abandonedPathId << endl;
            for (uint32_t i = 0; i < pathCount; ++i) {
                switchCounters[i] -= ackCounters[i];
                ackCounters[i] -= ackCounters[i];
            }
        }

        uint32_t bestOff = off;
        int64_t bestDiff =
            static_cast<int64_t>(switchCounters[off]) - static_cast<int64_t>(ackCounters[off]);
        for (uint32_t i = 0; i < pathCount; ++i) {
            int64_t diff =
                static_cast<int64_t>(switchCounters[i]) - static_cast<int64_t>(ackCounters[i]);
            // if (ch.sip == 184550913 + 512 * 2) {
            //     // 打印每条路径的发送数量和ack数量
            //     cout << i << "路径发送数量:" << g_pathIdCountSwitch[i]
            //          << ",ack数量:" << g_pathIdCountAck[i] << ",差值为"
            //          << g_pathIdCountSwitch[i] - g_pathIdCountAck[i] << endl;
            // }
            if (diff < bestDiff) {
                bestDiff = diff;
                bestOff = i;
            }
        }
        // cout << "选择的路径:" << bestOff << endl;
        off = bestOff;
        // 给每个包打上路径ID标签，并更新g_pathIdCountSwitch
        uint8_t pathId = static_cast<uint8_t>(off & 0xff);
        if (ch.l3Prot == 0x11) {
            ch.udp.pathId = pathId;
        }
        p->AddPacketTag(PathIdTag(pathId));
        if (pathId < kPathChoiceCount) {
            switchCounters[pathId]++;
        }

        if (m_abandonedPathId >= 0 && static_cast<uint32_t>(m_abandonedPathId) == off) {
            if (nexthops.size() > 4) {
                cout << "选择新路径" << endl;
                return nexthops[4];
            }
            return nexthops[off % nexthops.size()];
        }

        return nexthops[off % nexthops.size()];
    }
    return nexthops[0];
}
#endif

/*
实验二选择ack多路径+路径替换
短流只走2条多路径
裁剪2条路径
*/ 
#if 1
uint32_t SwitchNode::DoLbFlowECMP(Ptr<Packet> p, CustomHeader& ch,  
                                  const std::vector<int>& nexthops) {
    // 基于序列号(seq)的路由选择
    uint32_t seq = 0;
    uint16_t qpSport = 0;
    uint16_t qpDport = 0;
    uint16_t qpPg = 0;
    uint32_t base_sip = 184550913;
    // cout << "ch.sip " << ch.sip << endl;
    // 提取序列号，根据协议类型不同字段
    if (ch.l3Prot == 0x6) {  // TCP
        seq = ch.tcp.seq;
        qpSport = ch.tcp.sport;
        qpDport = ch.tcp.dport;
    } else if (ch.l3Prot == 0x11) {  // UDP
        seq = ch.udp.seq;
        qpSport = ch.udp.sport;
        qpDport = ch.udp.dport;
        qpPg = ch.udp.pg;
    } else if (ch.l3Prot == 0xFC || ch.l3Prot == 0xFD) {  // ACK or NACK
        seq = ch.ack.seq;
        // pick one next hop based on hash
        union {
            uint8_t u8[4 + 4 + 2 + 2];
            uint32_t u32[3];
        } buf;
        buf.u32[0] = ch.sip;
        buf.u32[1] = ch.dip;
        if (ch.l3Prot == 0x6)
            buf.u32[2] = ch.tcp.sport | ((uint32_t)ch.tcp.dport << 16);
        else if (ch.l3Prot == 0x11)  // XXX RDMA traffic on UDP
            buf.u32[2] = ch.udp.sport | ((uint32_t)ch.udp.dport << 16);
        else if (ch.l3Prot == 0xFC || ch.l3Prot == 0xFD)  // ACK or NACK
            buf.u32[2] = ch.ack.sport | ((uint32_t)ch.ack.dport << 16);
        else {
            std::cout << "[ERROR] Sw(" << m_id << ")," << PARSE_FIVE_TUPLE(ch)
                      << "Cannot support other protoocls than TCP/UDP (l3Prot:" << ch.l3Prot << ")"
                      << std::endl;
            assert(false && "Cannot support other protoocls than TCP/UDP");
        }

        uint32_t hashVal = EcmpHash(buf.u8, 12, m_ecmpSeed);
        uint32_t idx = hashVal % nexthops.size();
        return nexthops[idx];
    }

    else {
        std::cout << "[ERROR] Sw(" << m_id << ")," << PARSE_FIVE_TUPLE(ch)
                  << "Cannot support other protocols than TCP/UDP (l3Prot:" << ch.l3Prot << ")"
                  << std::endl;
        assert(false && "Cannot support other protocols than TCP/UDP");
    }

    uint32_t pathCount = 4;
    bool isBaseSipFlow = (ch.sip == base_sip);
    // base_sip 流走 4 路统计；其它流走 0/1 两路，但同样参与差值选路和路径更新。
    uint32_t activePathCount = isBaseSipFlow ? pathCount : std::min<uint32_t>(2, pathCount);

    // 使用 dip/sport/dport/pg 还原该包所属 qp 队列，并按 qp 做 4 路统计。
    uint64_t qpKey = GetPathStatsQpKey(ch.dip, qpSport, qpDport, qpPg);
    PathIdCounters& switchCounters = GetPathIdCountSwitch(qpKey);
    PathIdCounters& ackCounters = GetPathIdCountAck(qpKey);
    // 目标流在4号交换机上的选择
    uint32_t off = (seq / PACKET_SIZE) % activePathCount;
    // 如果是第一次看到这个流，直接选择第一条路径，因为最开始会发送俩个off为0的包
    // 根据发送包的sip来选择
    if (nexthops.size() != 1) {
        // if (ch.sip == base_sip) {
        // for(auto i:nexthops){
        //     cout<<"nexthops:"<<i<<" ";
        // }
        // cout<<endl;
        if (switchCounters[0] == -1) {
            switchCounters[0] = 0;
            return nexthops[0];
        }
        // cout << "-----------交换机选择路径-----------" << endl;

        // Find max(switch-ack) path and the min/second-min ACK counts.
        int64_t minAck = std::numeric_limits<int64_t>::max();
        int64_t secondMinAck = std::numeric_limits<int64_t>::max();
        int64_t maxDiff = std::numeric_limits<int64_t>::min();
        int64_t secondMaxDiff = std::numeric_limits<int64_t>::min();
        uint32_t maxDiffPath = 0;
        uint32_t secondMaxDiffPath = 0;
        for (uint32_t i = 0; i < activePathCount; ++i) {
            int64_t ack = ackCounters[i];
            if (ack < minAck) {
                secondMinAck = minAck;
                minAck = ack;
            } else if (ack < secondMinAck) {
                secondMinAck = ack;
            }

            int64_t diff =
                static_cast<int64_t>(switchCounters[i]) - static_cast<int64_t>(ackCounters[i]);
            if (diff > maxDiff) {
                secondMaxDiff = maxDiff;
                secondMaxDiffPath = maxDiffPath;
                maxDiff = diff;
                maxDiffPath = i;
            } else if (diff > secondMaxDiff) {
                secondMaxDiff = diff;
                secondMaxDiffPath = i;
            }
        }
        cout << "统计结果 -> minAck:" << minAck << ", secondMinAck:" << secondMinAck
             << ", maxDiffPath:" << maxDiffPath << ", maxDiff:" << maxDiff << endl;

           for (uint32_t i = 0; i < activePathCount; ++i) {
            // 打印每条路径的发送数量和ack数量
            cout << i << "路径发送数量:" << switchCounters[i] << ",ack数量:" << ackCounters[i]
                 << ",差值为" << switchCounters[i] - ackCounters[i] << endl;
        }

        // Trigger updates: deprecate up to two paths and shift future matches to
        // nexthops[4] / nexthops[5].
        bool shouldUpdate = (secondMinAck != std::numeric_limits<int64_t>::max() &&
                             (secondMinAck - minAck) > maxDiff );
        if (shouldUpdate && m_abandonedPathId < 0) {
            cout << "更新路径" << endl;
            m_abandonedPathId = static_cast<int>(maxDiffPath);
            cout << "废弃路径:" << m_abandonedPathId << endl;
            for (uint32_t i = 0; i < activePathCount; ++i) {
                switchCounters[i] -= ackCounters[i];
                ackCounters[i] -= ackCounters[i];
            }
        } else if (shouldUpdate && m_abandonedPathId2 < 0) {
            uint32_t candidate = maxDiffPath;
            if (candidate == static_cast<uint32_t>(m_abandonedPathId)) {
                candidate = secondMaxDiffPath;
            }
            if (candidate != static_cast<uint32_t>(m_abandonedPathId) &&
                secondMaxDiff != std::numeric_limits<int64_t>::min()) {
                cout << "更新第二条路径" << endl;
                m_abandonedPathId2 = static_cast<int>(candidate);
                cout << "第二条废弃路径:" << m_abandonedPathId2 << endl;
                for (uint32_t i = 0; i < activePathCount; ++i) {
                    switchCounters[i] -= ackCounters[i];
                    ackCounters[i] -= ackCounters[i];
                }
            }
        }

        uint32_t bestOff = off;
        int64_t bestDiff =
            static_cast<int64_t>(switchCounters[off]) - static_cast<int64_t>(ackCounters[off]);
        for (uint32_t i = 0; i < activePathCount; ++i) {
            int64_t diff =
                static_cast<int64_t>(switchCounters[i]) - static_cast<int64_t>(ackCounters[i]);
            // if (ch.sip == 184550913 + 512 * 2) {
            //     // 打印每条路径的发送数量和ack数量
            //     cout << i << "路径发送数量:" << g_pathIdCountSwitch[i]
            //          << ",ack数量:" << g_pathIdCountAck[i] << ",差值为"
            //          << g_pathIdCountSwitch[i] - g_pathIdCountAck[i] << endl;
            // }
            if (diff < bestDiff) {
                bestDiff = diff;
                bestOff = i;
            }
        }
        // cout << "选择的路径:" << bestOff << endl;
        off = bestOff;
        // 给每个包打上路径ID标签，并更新g_pathIdCountSwitch
        uint8_t pathId = static_cast<uint8_t>(off & 0xff);
        if (ch.l3Prot == 0x11) {
            ch.udp.pathId = pathId;
        }
        p->AddPacketTag(PathIdTag(pathId));
        if (pathId < kPathChoiceCount) {
            switchCounters[pathId]++;
        }

        if (m_abandonedPathId >= 0 && static_cast<uint32_t>(m_abandonedPathId) == off) {
            if (nexthops.size() > 4) {
                cout << "选择新路径" << endl;
                return nexthops[4];
            }
            return nexthops[off % nexthops.size()];
        }

        if (m_abandonedPathId2 >= 0 && static_cast<uint32_t>(m_abandonedPathId2) == off) {
            if (nexthops.size() > 5) {
                cout << "选择第二条新路径" << endl;
                return nexthops[5];
            }
            if (nexthops.size() > 4) {
                cout << "选择新路径" << endl;
                return nexthops[4];
            }
            return nexthops[off % nexthops.size()];
        }

        return nexthops[off % nexthops.size()];
    }
    return nexthops[0];
}
#endif

// if(off!=3){
//     return nexthops[(idx+1+off)% nexthops.size()];
// }else{
//     // if(counter%4==0){
//     //     counter++;
//     //     uint32_t counter_type=counter%3;
//     //     return nexthops[(idx+1+counter_type)% nexthops.size()];
//     // }else{
//     //     counter++;
//         return nexthops[idx];
//     }

// idx = (idx+hash_val) % nexthops.size(); // 保证不越界

/*-----------------CONGA-----------------*/
uint32_t SwitchNode::DoLbConga(Ptr<Packet> p, CustomHeader& ch, const std::vector<int>& nexthops) {
    return DoLbFlowECMP(p, ch, nexthops);  // flow ECMP (dummy)
}

/*-----------------Letflow-----------------*/
uint32_t SwitchNode::DoLbLetflow(Ptr<Packet> p, CustomHeader& ch,
                                 const std::vector<int>& nexthops) {
    if (m_isToR && nexthops.size() == 1) {
        if (m_isToR_hostIP.find(ch.sip) != m_isToR_hostIP.end() &&
            m_isToR_hostIP.find(ch.dip) != m_isToR_hostIP.end()) {
            return nexthops[0];  // intra-pod traffic
        }
    }

    /* ONLY called for inter-Pod traffic */
    uint32_t outPort = m_mmu->m_letflowRouting.RouteInput(p, ch);
    if (outPort == LETFLOW_NULL) {
        assert(nexthops.size() == 1);  // Receiver's TOR has only one interface to receiver-server
        outPort = nexthops[0];         // has only one option
    }
    assert(std::find(nexthops.begin(), nexthops.end(), outPort) !=
           nexthops.end());  // Result of Letflow cannot be found in nexthops
    return outPort;
}

/*-----------------DRILL-----------------*/
uint32_t SwitchNode::CalculateInterfaceLoad(uint32_t interface) {
    Ptr<QbbNetDevice> device = DynamicCast<QbbNetDevice>(m_devices[interface]);
    NS_ASSERT_MSG(!!device && !!device->GetQueue(),
                  "Error of getting a egress queue for calculating interface load");
    return device->GetQueue()->GetNBytesTotal();  // also used in HPCC
}

uint32_t SwitchNode::DoLbDrill(Ptr<const Packet> p, const CustomHeader& ch,
                               const std::vector<int>& nexthops) {
    // find the Egress (output) link with the smallest local Egress Queue length
    uint32_t leastLoadInterface = 0;
    uint32_t leastLoad = std::numeric_limits<uint32_t>::max();
    auto rand_nexthops = nexthops;
    std::random_shuffle(rand_nexthops.begin(), rand_nexthops.end());

    std::map<uint32_t, uint32_t>::iterator itr = m_previousBestInterfaceMap.find(ch.dip);
    if (itr != m_previousBestInterfaceMap.end()) {
        leastLoadInterface = itr->second;
        leastLoad = CalculateInterfaceLoad(itr->second);
    }

    uint32_t sampleNum =
        m_drill_candidate < rand_nexthops.size() ? m_drill_candidate : rand_nexthops.size();
    for (uint32_t samplePort = 0; samplePort < sampleNum; samplePort++) {
        uint32_t sampleLoad = CalculateInterfaceLoad(rand_nexthops[samplePort]);
        if (sampleLoad < leastLoad) {
            leastLoad = sampleLoad;
            leastLoadInterface = rand_nexthops[samplePort];
        }
    }
    m_previousBestInterfaceMap[ch.dip] = leastLoadInterface;
    return leastLoadInterface;
}

/*------------------ConWeave Dummy ----------------*/
uint32_t SwitchNode::DoLbConWeave(Ptr<Packet> p, CustomHeader& ch,
                                  const std::vector<int>& nexthops) {
    return DoLbFlowECMP(p, ch, nexthops);  // flow ECMP (dummy)
}
/*----------------------------------*/

void SwitchNode::CheckAndSendPfc(uint32_t inDev, uint32_t qIndex) {
    Ptr<QbbNetDevice> device = DynamicCast<QbbNetDevice>(m_devices[inDev]);
    bool pClasses[qCnt] = {0};
    m_mmu->GetPauseClasses(inDev, qIndex, pClasses);
    for (int j = 0; j < qCnt; j++) {
        if (pClasses[j]) {
            uint32_t paused_time = device->SendPfc(j, 0);
            m_mmu->SetPause(inDev, j, paused_time);
            m_mmu->m_pause_remote[inDev][j] = true;
            /** PAUSE SEND COUNT ++ */
        }
    }

    for (int j = 0; j < qCnt; j++) {
        if (!m_mmu->m_pause_remote[inDev][j]) continue;

        if (m_mmu->GetResumeClasses(inDev, j)) {
            device->SendPfc(j, 1);
            m_mmu->SetResume(inDev, j);
            m_mmu->m_pause_remote[inDev][j] = false;
        }
    }
}
void SwitchNode::CheckAndSendResume(uint32_t inDev, uint32_t qIndex) {
    Ptr<QbbNetDevice> device = DynamicCast<QbbNetDevice>(m_devices[inDev]);
    if (m_mmu->GetResumeClasses(inDev, qIndex)) {
        device->SendPfc(qIndex, 1);
        m_mmu->SetResume(inDev, qIndex);
    }
}

/********************************************
 *              MAIN LOGICS                 *
 *******************************************/

// This function can only be called in switch mode
bool SwitchNode::SwitchReceiveFromDevice(Ptr<NetDevice> device, Ptr<Packet> packet,
                                         CustomHeader& ch) {
    SendToDev(packet, ch);
    return true;
}

void SwitchNode::SendToDev(Ptr<Packet> p, CustomHeader& ch) {
    /** HIJACK: hijack the packet and run DoSwitchSend internally for Conga and ConWeave.
     * Note that DoLbConWeave() and DoLbConga() are flow-ECMP function for control packets
     * or intra-ToR traffic.
     */

    // Conga
    if (Settings::lb_mode == 3) {
        m_mmu->m_congaRouting.RouteInput(p, ch);
        return;
    }

    // ConWeave
    if (Settings::lb_mode == 9) {
        m_mmu->m_conweaveRouting.RouteInput(p, ch);
        return;
    }

    // Others
    SendToDevContinue(p, ch);
}

void SwitchNode::SendToDevContinue(Ptr<Packet> p, CustomHeader& ch) {
    int idx = GetOutDev(p, ch);
    if (idx >= 0) {
        NS_ASSERT_MSG(m_devices[idx]->IsLinkUp(),
                      "The routing table look up should return link that is up");

        // determine the qIndex
        uint32_t qIndex;
        if (ch.l3Prot == 0xFF || ch.l3Prot == 0xFE ||
            (m_ackHighPrio &&
             (ch.l3Prot == 0xFD ||
              ch.l3Prot == 0xFC))) {  // QCN or PFC or ACK/NACK, go highest priority
            qIndex = 0;               // high priority
        } else {
            qIndex = (ch.l3Prot == 0x06 ? 1 : ch.udp.pg);  // if TCP, put to queue 1. Otherwise, it
                                                           // would be 3 (refer to trafficgen)
        }

        DoSwitchSend(p, ch, idx, qIndex);  // m_devices[idx]->SwitchSend(qIndex, p, ch);
        return;
    }
    std::cout << "WARNING - Drop occurs in SendToDevContinue()" << std::endl;
    return;  // Drop otherwise
}

int SwitchNode::GetOutDev(Ptr<Packet> p, CustomHeader& ch) {
    // look up entries
    auto entry = m_rtTable.find(ch.dip);

    // no matching entry
    if (entry == m_rtTable.end()) {
        std::cout << "[ERROR] Sw(" << m_id << ")," << PARSE_FIVE_TUPLE(ch)
                  << "No matching entry, so drop this packet at SwitchNode (l3Prot:" << ch.l3Prot
                  << ")" << std::endl;
        assert(false);
    }

    // entry found
    const auto& nexthops = entry->second;
    bool control_pkt =
        (ch.l3Prot == 0xFF || ch.l3Prot == 0xFE || ch.l3Prot == 0xFD || ch.l3Prot == 0xFC);

    if (Settings::lb_mode == 0 || control_pkt) {  // control packet (ACK, NACK, PFC, QCN)
        return DoLbFlowECMP(p, ch, nexthops);     // ECMP routing path decision (4-tuple)
    }

    switch (Settings::lb_mode) {
        case 2:
            return DoLbDrill(p, ch, nexthops);
        case 3:
            return DoLbConga(p, ch, nexthops); /** DUMMY: Do ECMP */
        case 6:
            return DoLbLetflow(p, ch, nexthops);
        case 9:
            return DoLbConWeave(p, ch, nexthops); /** DUMMY: Do ECMP */
        default:
            std::cout << "Unknown lb_mode(" << Settings::lb_mode << ")" << std::endl;
            assert(false);
    }
}

/*
 * The (possible) callback point when conweave dequeues packets from buffer
 */
void SwitchNode::DoSwitchSend(Ptr<Packet> p, CustomHeader& ch, uint32_t outDev, uint32_t qIndex) {
    // admission control
    FlowIdTag t;
    p->PeekPacketTag(t);
    uint32_t inDev = t.GetFlowId();

    /** NOTE:
     * ConWeave control packets have the high priority as ACK/NACK/PFC/etc with qIndex = 0.
     */
    if (inDev == Settings::CONWEAVE_CTRL_DUMMY_INDEV) {  // sanity check
        // ConWeave reply is on ACK protocol with high priority, so qIndex should be 0
        assert(qIndex == 0 && m_ackHighPrio == 1 &&
               "ConWeave's reply packet follows ACK, so its qIndex should be 0");
    }

    if (qIndex != 0) {  // not highest priority
        if (m_mmu->CheckEgressAdmission(outDev, qIndex,
                                        p->GetSize())) {  // Egress Admission control
            if (m_mmu->CheckIngressAdmission(inDev, qIndex,
                                             p->GetSize())) {  // Ingress Admission control
                m_mmu->UpdateIngressAdmission(inDev, qIndex, p->GetSize());
                m_mmu->UpdateEgressAdmission(outDev, qIndex, p->GetSize());
            } else { /** DROP: At Ingress */
#if (0)
                // /** NOTE: logging dropped pkts */
                // std::cout << "LostPkt ingress - Sw(" << m_id << ")," << PARSE_FIVE_TUPLE(ch)
                //           << "L3Prot:" << ch.l3Prot
                //           << ",Size:" << p->GetSize()
                //           << ",At " << Simulator::Now() << std::endl;
#endif
                Settings::dropped_pkt_sw_ingress++;
                return;  // drop
            }
        } else { /** DROP: At Egress */
#if (0)
            // /** NOTE: logging dropped pkts */
            // std::cout << "LostPkt egress - Sw(" << m_id << ")," << PARSE_FIVE_TUPLE(ch)
            //           << "L3Prot:" << ch.l3Prot << ",Size:" << p->GetSize() << ",At "
            //           << Simulator::Now() << std::endl;
#endif
            Settings::dropped_pkt_sw_egress++;
            cout<<"交换机丢包"<<endl;
            return;  // drop
        }

        CheckAndSendPfc(inDev, qIndex);
    }

    m_devices[outDev]->SwitchSend(qIndex, p, ch);
}

void SwitchNode::SwitchNotifyDequeue(uint32_t ifIndex, uint32_t qIndex, Ptr<Packet> p) {
    FlowIdTag t;
    p->PeekPacketTag(t);
    if (qIndex != 0) {
        uint32_t inDev = t.GetFlowId();
        if (inDev != Settings::CONWEAVE_CTRL_DUMMY_INDEV) {
            // NOTE: ConWeave's probe/reply does not need to pass inDev interface,
            // so skip for conweave's queued packets
            m_mmu->RemoveFromIngressAdmission(inDev, qIndex, p->GetSize());
        }
        m_mmu->RemoveFromEgressAdmission(ifIndex, qIndex, p->GetSize());
        if (m_ecnEnabled) {
            bool egressCongested = m_mmu->ShouldSendCN(ifIndex, qIndex);
            if (egressCongested) {
                PppHeader ppp;
                Ipv4Header h;
                p->RemoveHeader(ppp);
                p->RemoveHeader(h);
                h.SetEcn((Ipv4Header::EcnType)0x03);
                p->AddHeader(h);
                p->AddHeader(ppp);
            }
        }
        // NOTE: ConWeave's probe/reply does not need to pass inDev interface
        if (inDev != Settings::CONWEAVE_CTRL_DUMMY_INDEV) {
            CheckAndSendResume(inDev, qIndex);
        }
    }

    // HPCC's INT
    if (1) {
        uint8_t* buf = p->GetBuffer();
        if (buf[PppHeader::GetStaticSize() + 9] == 0x11) {  // udp packet
            IntHeader* ih = (IntHeader*)&buf[PppHeader::GetStaticSize() + 20 + 8 +
                                             6];  // ppp, ip, udp, SeqTs, INT
            Ptr<QbbNetDevice> dev = DynamicCast<QbbNetDevice>(m_devices[ifIndex]);
            if (m_ccMode == 3) {  // HPCC
                ih->PushHop(Simulator::Now().GetTimeStep(), m_txBytes[ifIndex],
                            dev->GetQueue()->GetNBytesTotal(), dev->GetDataRate().GetBitRate());
            }
        }
    }
    m_txBytes[ifIndex] += p->GetSize();
}

uint32_t SwitchNode::EcmpHash(const uint8_t* key, size_t len, uint32_t seed) {
    uint32_t h = seed;
    if (len > 3) {
        const uint32_t* key_x4 = (const uint32_t*)key;
        size_t i = len >> 2;
        do {
            uint32_t k = *key_x4++;
            k *= 0xcc9e2d51;
            k = (k << 15) | (k >> 17);
            k *= 0x1b873593;
            h ^= k;
            h = (h << 13) | (h >> 19);
            h += (h << 2) + 0xe6546b64;
        } while (--i);
        key = (const uint8_t*)key_x4;
    }
    if (len & 3) {
        size_t i = len & 3;
        uint32_t k = 0;
        key = &key[i - 1];
        do {
            k <<= 8;
            k |= *key--;
        } while (--i);
        k *= 0xcc9e2d51;
        k = (k << 15) | (k >> 17);
        k *= 0x1b873593;
        h ^= k;
    }
    h ^= len;
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}

void SwitchNode::SetEcmpSeed(uint32_t seed) { m_ecmpSeed = seed; }

void SwitchNode::AddTableEntry(Ipv4Address& dstAddr, uint32_t intf_idx) {
    uint32_t dip = dstAddr.Get();
    m_rtTable[dip].push_back(intf_idx);
}

void SwitchNode::ClearTable() { m_rtTable.clear(); }

uint64_t SwitchNode::GetTxBytesOutDev(uint32_t outdev) {
    assert(outdev < pCnt);
    return m_txBytes[outdev];
}

} /* namespace ns3 */
