#include "frame.h"

double Frame::vf_duration(Frame *nextvp, double max_frame_duration) {
    if (!nextvp) {
        return 0.0;
    }
    if (nextvp->serial != serial) {
        return 0.0;
    }
    double duration = nextvp->pts - pts;
    if (isnan(duration) || duration <= 0 || duration > max_frame_duration) {
        return this->duration;
    }
    return duration;
}

void Frame::reset() {
    av_frame_unref(frame);
    serial = 0; // ��֡�����кţ�����seek����
    pts = 0.0; // ��֡����ʾʱ�����Presentation Time Stamp������ʾ��֡Ӧ���ڲ���ʱ��ʾ��ʱ��
    duration = 0.0; // ��֡�ĳ���ʱ�䣬��ʾ��֡����ʾʱ��
    uploaded = 0; // ��һ֡�Ƿ��Ѿ��ϴ�����Ⱦ������
    pos = 0; // ��֡���ļ��е�λ�ã����ڶ�λ��֡����Դ /* byte position of the frame in the input file */
}