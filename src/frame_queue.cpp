#include "frame_queue.h"

FrameQueue::FrameQueue(PacketQueue *pktq, int max_size, int keep_last)
    : m_pktq(pktq),
      m_keep_last(!!keep_last) {
    m_max_size = std::min(max_size, FRAME_QUEUE_SIZE);
    for (size_t i = 0; i < m_max_size; i++) {
        m_queue[i].frame = av_frame_alloc();
    }
}
FrameQueue::~FrameQueue() {
    for (size_t i = 0; i < m_max_size; i++) {
        Frame* vp = &m_queue[i];
        av_frame_unref(m_queue[i].frame);
        av_frame_free(&m_queue[i].frame);
    }
}

int FrameQueue::nb_remaining() {
    return m_size - m_rindex_shown;
}

int FrameQueue::rindex_shown() {
    return m_rindex_shown;
}

// �ҵ���д��֡
Frame* FrameQueue::peek_writable() {
    std::unique_lock lock(m_mutex);
    while (m_size >= m_max_size && m_pktq->request_aborted()) {
        m_cond.wait(lock);
    }
    if (m_pktq->request_aborted()) {
        return nullptr;
    }
    return &m_queue[m_windex];
}

// ����һ֡���ݵ�����
void FrameQueue::push() {
    if (++m_windex == m_max_size) {
        m_windex = 0;
    }
    std::lock_guard lock(m_mutex);
    m_size++;
    m_cond.notify_one();
}

// ��ȡһ֡����
Frame* FrameQueue::peek_readable() {
    std::unique_lock lock(m_mutex);
    while (m_size - m_rindex_shown <= 0 && !m_pktq->request_aborted()) {
        m_cond.wait(lock);
    }
    if (m_pktq->request_aborted()) {
        return nullptr;
    }
    return &m_queue[(m_rindex + m_rindex_shown) % m_max_size];
}

// ����һ֡����
void FrameQueue::next() {
    if (m_keep_last && !m_rindex_shown) {
        m_rindex_shown = 1;
        return;
    }
    av_frame_unref(m_queue[m_rindex].frame);
    if (++m_rindex == m_max_size) {
        m_rindex = 0;
    }
    std::lock_guard lock(m_mutex);
    m_size--;
    m_cond.notify_one();
}
// ������һ֡
Frame* FrameQueue::peek_last() {
    return &m_queue[m_rindex];
}
// ���ҵ�ǰ֡
Frame* FrameQueue::peek() {
    return &m_queue[(m_rindex + m_rindex_shown) % m_max_size];
}
// ������һ֡
Frame* FrameQueue::peek_next() {
    return &m_queue[(m_rindex + m_rindex_shown + 1) % m_max_size];
}

void FrameQueue::lock() {
    m_mutex.lock();
}
void FrameQueue::unlock() {
    m_mutex.unlock();
}