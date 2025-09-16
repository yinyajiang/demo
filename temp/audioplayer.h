#ifndef AUDIOPLAYER_H
#define AUDIOPLAYER_H

#include <QtCore/QObject>
#include <QtMultimedia/QAudioFormat>
#include <QtMultimedia/QAudioSink>
#include <QtCore/QIODevice>
#include <QtCore/QByteArray>
#include <QtCore/QTimer>
#include <QtCore/QTime>
#include <QtMultimedia/QAudioDevice>
#include <QtMultimedia/QMediaDevices>

class PcmDataSource : public QIODevice
{
    Q_OBJECT

public:
    explicit PcmDataSource(QObject *parent = nullptr);
    
    void addData(const QByteArray &data);
    void clear();
    bool atEnd() const override;
    qint64 bytesAvailable() const override;
    
    // 声道平衡控制
    void setBalance(qreal balance);
    void setChannelInfo(int channels, int sampleRate);
    
protected:
    qint64 readData(char *data, qint64 maxlen) override;
    qint64 writeData(const char *data, qint64 len) override;

private:
    void applyBalance(int16_t *samples, int sampleCount);
    
    QByteArray m_buffer;
    qint64 m_pos;
    qreal m_balance; // 声道平衡
    int m_channels;
    int m_sampleRate;
};

class AudioPlayer : public QObject
{
    Q_OBJECT

public:
    explicit AudioPlayer(QObject *parent = nullptr);
    ~AudioPlayer();

    bool initialize(int sampleRate, int channels, int bitsPerSample);
    void play();
    void pause();
    void stop();
    void addPcmData(const QByteArray &data);
    void clearBuffer();
    
    bool isPlaying() const;
    
    // 音量控制 (0.0 - 1.0)
    void setVolume(qreal volume);
    qreal volume() const;
    
    // 声道平衡控制 (-1.0 到 1.0，-1.0为完全左声道，0.0为平衡，1.0为完全右声道)
    void setBalance(qreal balance);
    qreal balance() const;
    
    // 播放进度跟踪
    void setAudioFormat(int sampleRate, int channels);
    double getPlayedDuration() const; // 获取已播放的时长
    void resetPlayedDuration(); // 重置播放时长
    void setPlayedDuration(double seconds); // 设置已播放时长（用于定位）

signals:
    void stateChanged();
    void needMoreData(); // 当缓冲区数据较少时发出信号

private slots:
    void onStateChanged();
    void checkBufferLevel();

private:
    void cleanup();

private:
    QAudioSink *m_audioSink;
    PcmDataSource *m_dataSource;
    QAudioFormat m_format;
    QAudioDevice m_audioDevice;
    QTimer *m_bufferTimer;
    
    bool m_initialized;
    qreal m_volume;
    qreal m_balance; // 声道平衡 (-1.0 到 1.0)
    int m_requestCounter; // 防止频繁请求数据
    
    // 播放进度跟踪
    QTime m_playStartTime; // 播放开始时间
    double m_pausedDuration; // 暂停的总时长
    bool m_isPlayingForProgress; // 用于进度计算的播放状态
    int m_trackingSampleRate;
    int m_trackingChannels;
};

#endif // AUDIOPLAYER_H
