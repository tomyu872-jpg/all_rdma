#include "bitmap-retrans-module.h"

#include <algorithm>

namespace ns3 {

void BitmapRetransModule::SetBitmapSize(uint32_t bitmapSize) {
    m_bitmapSize = std::max<uint32_t>(1, bitmapSize);
    for (std::unordered_map<RdmaFlowKey, RxState, RdmaFlowKeyHash>::iterator it = m_rxStates.begin();
         it != m_rxStates.end(); ++it) {
        it->second.bitmap.assign(m_bitmapSize, 0);
    }
}

void BitmapRetransModule::RegisterTxFlow(const RdmaFlowKey &key) { m_txStates[key]; }

void BitmapRetransModule::UnregisterTxFlow(const RdmaFlowKey &key) { m_txStates.erase(key); }

void BitmapRetransModule::RegisterRxFlow(const RdmaFlowKey &key) {
    RxState &state = m_rxStates[key];
    if (state.bitmap.size() != m_bitmapSize) {
        state.bitmap.assign(m_bitmapSize, 0);
    }
}

void BitmapRetransModule::UnregisterRxFlow(const RdmaFlowKey &key) { m_rxStates.erase(key); }

void BitmapRetransModule::OnPacketSent(const RdmaFlowKey &key, uint32_t seq, uint32_t size, bool isRetrans) {
    if (size == 0 || isRetrans) return;
    TxState &state = m_txStates[key];
    if (!state.outstanding.empty()) {
        const TxSegment &tail = state.outstanding.back();
        if (tail.seq == seq) return;
    }
    TxSegment seg;
    seg.seq = seq;
    seg.size = size;
    state.outstanding.push_back(seg);
}

void BitmapRetransModule::OnAck(const RdmaFlowKey &key, uint32_t ackSeq) {
    TxState &state = m_txStates[key];
    while (!state.outstanding.empty()) {
        const TxSegment &seg = state.outstanding.front();
        if (seg.seq + seg.size > ackSeq) break;
        state.outstanding.pop_front();
    }
}

uint32_t BitmapRetransModule::GetOldestUnacked(const RdmaFlowKey &key, uint32_t fallbackSeq) const {
    std::unordered_map<RdmaFlowKey, TxState, RdmaFlowKeyHash>::const_iterator it = m_txStates.find(key);
    if (it == m_txStates.end() || it->second.outstanding.empty()) return fallbackSeq;
    return it->second.outstanding.front().seq;
}

bool BitmapRetransModule::HasOutstanding(const RdmaFlowKey &key) const {
    std::unordered_map<RdmaFlowKey, TxState, RdmaFlowKeyHash>::const_iterator it = m_txStates.find(key);
    return it != m_txStates.end() && !it->second.outstanding.empty();
}

bool BitmapRetransModule::TryScheduleRetrans(const RdmaFlowKey &key, uint32_t seq) {
    TxState &state = m_txStates[key];
    return state.pendingRetransSeqs.insert(seq).second;
}

void BitmapRetransModule::ClearRetransMarker(const RdmaFlowKey &key, uint32_t seq) {
    TxState &state = m_txStates[key];
    state.pendingRetransSeqs.erase(seq);
}

void BitmapRetransModule::ClearRetransUpTo(const RdmaFlowKey &key, uint32_t ackSeq) {
    TxState &state = m_txStates[key];
    std::unordered_set<uint32_t>::iterator it = state.pendingRetransSeqs.begin();
    while (it != state.pendingRetransSeqs.end()) {
        if (*it < ackSeq) {
            it = state.pendingRetransSeqs.erase(it);
        } else {
            ++it;
        }
    }
}

std::vector<uint32_t> BitmapRetransModule::CollectMissingSeqs(const RdmaFlowKey &key, uint32_t ackSeq) {
    m_txStates[key];
    std::vector<uint32_t> missingSeqs;
    missingSeqs.push_back(ackSeq);
    return missingSeqs;
}

BitmapRetransFeedback BitmapRetransModule::OnData(const RdmaFlowKey &key, uint32_t seq, uint32_t size,
                                                  uint32_t packetSize) {
    RxState &state = m_rxStates[key];
    if (state.bitmap.size() != m_bitmapSize) {
        state.bitmap.assign(m_bitmapSize, 0);
    }
    BitmapRetransFeedback feedback;
    feedback.ackSeq = state.expectedSeq;

    uint32_t step = packetSize == 0 ? std::max<uint32_t>(1, size) : packetSize;
    if (seq < state.expectedSeq) {
        feedback.sendControl = true;
        feedback.isNack = false;
        feedback.ackSeq = state.expectedSeq;
        return feedback;
    }

    if (seq == state.expectedSeq) {
        feedback.sendControl = true;
        uint32_t psn = seq / step;
        state.bitmap[psn % m_bitmapSize] = 0;
        state.expectedSeq += size;
        uint32_t nextPsn = state.expectedSeq / step;
        while (state.bitmap[nextPsn % m_bitmapSize] != 0) {
            state.bitmap[nextPsn % m_bitmapSize] = 0;
            state.expectedSeq += step;
            nextPsn++;
        }
        feedback.isNack = false;
        feedback.ackSeq = state.expectedSeq;
        return feedback;
    }

    uint32_t expectedPsn = state.expectedSeq / step;
    uint32_t psn = seq / step;
    if (psn >= expectedPsn + m_bitmapSize) {
        feedback.sendControl = true;
        feedback.isNack = true;
        feedback.droppedForOverflow = true;
        feedback.ackSeq = state.expectedSeq;
        return feedback;
    }

    state.bitmap[psn % m_bitmapSize] = 1;
    feedback.sendControl = false;
    feedback.isNack = false;
    feedback.ackSeq = state.expectedSeq;
    return feedback;
}

}  // namespace ns3
