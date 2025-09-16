#ifndef AUDIODECODER_H
#define AUDIODECODER_H

#include <QtCore/QObject>
#include <QtCore/QByteArray>
#include <QtCore/QString>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
}

class AudioDecoder : public QObject
{
    Q_OBJECT

public:
    explicit AudioDecoder(QObject *parent = nullptr);
    ~AudioDecoder();

    bool openFile(const QString &filename);
    void close();
    
    // 获取音频信息
    int getSampleRate() const { return m_sampleRate; }
    int getChannels() const { return m_channels; }
    int getBitsPerSample() const { return 16; } // 固定输出16位PCM
    
    // 获取总时长（秒）
    double getTotalDuration() const { return m_totalDuration; }
    
    // 获取当前播放位置（秒）
    double getCurrentPosition() const { return m_currentPosition; }
    
    // 解码下一帧音频数据
    QByteArray decodeNextFrame();
    
    // 重置到文件开头
    void seekToStart();
    
    // 定位到指定时间位置（秒）
    bool seekToPosition(double seconds);
    
    // 检查是否到达文件末尾
    bool isAtEnd() const { return m_isAtEnd; }

signals:
    void error(const QString &message);

private:
    void cleanup();
    bool initializeResampler();
    QByteArray processFrame();

private:
    AVFormatContext *m_formatContext;
    AVCodecContext *m_codecContext;
    SwrContext *m_swrContext;
    AVPacket *m_packet;
    AVFrame *m_frame;
    
    int m_audioStreamIndex;
    int m_sampleRate;
    int m_channels;
    bool m_isAtEnd;
    
    // 时间相关
    double m_totalDuration;
    double m_currentPosition;
    int64_t m_totalSamples;
    int64_t m_processedSamples;
    
    // 重采样相关
    uint8_t **m_resampledData;
    int m_resampledDataSize;
};

#endif // AUDIODECODER_H
