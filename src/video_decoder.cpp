#include "opts.h"
#include "video_decoder.h"
#include <spdlog/spdlog.h>

VideoDecoder::VideoDecoder(std::shared_ptr<Context> ctx)
    : Decoder(ctx, AVMEDIA_TYPE_VIDEO) {
}

int VideoDecoder::_open(int stream_index, AVCodecContext *codec_ctx) {
    m_ctx->video_index = stream_index;
    m_ctx->video_codec_ctx = codec_ctx;
    m_ctx->video_stream = m_ctx->fmt_ctx->streams[stream_index];
    m_ctx->video_frame_rate = av_guess_frame_rate(m_ctx->fmt_ctx, m_ctx->video_stream, nullptr);
    m_queue = &m_ctx->video_packet_queue;
    return 0;
}

void VideoDecoder::run() {
    decode_loop();
}

void VideoDecoder::decode_loop() {
    int got_frame = 0;
    AVFrame *frame = av_frame_alloc();
    for (;;) {
        got_frame = decode(m_ctx->video_codec_ctx, frame);
        if (got_frame < 0) {
            break;
        }

        if (drop_frame(frame)) {
            continue;
        }
        // TODO add video filters, filter may add while 
        // TODO ���� frame_last_filter_delay
        
        if (!enqueue_frame(frame)) {
            break;
        }
        // av_frame_unref(frame);
        if (m_ctx->video_packet_queue.serial() != m_pkt_serial) {
            spdlog::warn("the serial in video packet queue and decoder is different, serial={}, serial={}", 
                m_ctx->video_packet_queue.serial(), m_pkt_serial);
            break;
        }
    }
    av_frame_free(&frame);
    return ;
}

bool VideoDecoder::drop_frame(AVFrame *frame) {
    double dpts = NAN;
    if (frame->pts != AV_NOPTS_VALUE) {
        dpts = av_q2d(m_ctx->video_stream->time_base) * frame->pts;
    }
    frame->sample_aspect_ratio = av_guess_sample_aspect_ratio(m_ctx->fmt_ctx, m_ctx->video_stream, frame);
    // "-framedrop"ѡ���������õ���Ƶ֡ʧȥͬ��ʱ���Ƿ�����Ƶ֡��"-framedrop"ѡ����bool��ʽ�ı����framedropֵ��
    // ����Ƶͬ����ʽ�����֣�Aͬ������Ƶ��Bͬ������Ƶ��Cͬ�����ⲿʱ�ӡ�
    // 1) �������в���"-framedrop"ѡ���"-noframedrop"ʱ��framedropֵΪĬ��ֵ-1����ͬ����ʽ��"ͬ������Ƶ"
    //    �򲻶���ʧȥͬ������Ƶ֡�����򽫶���ʧȥͬ������Ƶ֡��
    // 2) �������д�"-framedrop"ѡ��ʱ��framedropֵΪ1�����ۺ���ͬ����ʽ��������ʧȥͬ������Ƶ֡��
    // 3) �������д�"-noframedrop"ѡ��ʱ��framedropֵΪ0�����ۺ���ͬ����ʽ����������ʧȥͬ������Ƶ֡��
    if (framedrop > 0 || (framedrop && m_ctx->master_clock->sync_type() != SYNC_TYPE_VIDEO) && frame->pts != AV_NOPTS_VALUE) {
        double diff = dpts - m_ctx->master_clock->get();
        if (!isnan(diff) &&
            fabs(diff) < AV_NOSYNC_THRESHOLD &&
            diff - m_ctx->frame_last_filter_delay < 0 &&
            m_pkt_serial == m_ctx->video_clock.serial() &&
            m_ctx->video_packet_queue.count())
            m_ctx->frame_drops_early++;
            av_frame_unref(frame); // ��Ƶ֡ʧȥͬ����ֱ���ӵ�
            return true;
    }
    return false;
}

bool VideoDecoder::enqueue_frame(AVFrame* frame) {
    Frame* vp = m_ctx->video_frame_queue.peek_writable();
    if (!vp) {
        return false;
    }
    vp->uploaded = 0;

    vp->pts = frame->pts;
    vp->duration = frame->duration;
    vp->pos = frame->pkt_pos;
    vp->serial = m_pkt_serial;

    av_frame_move_ref(vp->frame, frame);
    m_ctx->video_frame_queue.push();
    return true;
}