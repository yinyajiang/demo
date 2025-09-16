#pragma once
#include <QAudioFormat>
#include <QtMultimedia/QAudioSink>
#include <QtMultimedia/QAudioDevice>
#include <QtMultimedia/QMediaDevices>
#include <memory>
 
class PCMDataSource;
class AudioPlay : public QObject
{
    Q_OBJECT
public:
    explicit AudioPlay(QAudioFormat audio_format, QObject *parent = nullptr);
    ~AudioPlay();

    void play();
    void stop();
    void pause();
    bool is_playing();
    void input_pcm_data(const uint8_t *data, int size);
    //[0.0, 1.0]
    void set_volume(float volume);
    float volume();

  signals:
    void signal_request_more_data();

  protected slots:
    void on_state_changed();

  private:
    QAudioFormat m_audio_format;
    std::unique_ptr<QAudioSink> m_audio_sink;
    std::unique_ptr<PCMDataSource> m_pcm_data_source;
    float m_volume;
    float m_balance;
};

class PCMDataSource: public QIODevice {
    Q_OBJECT
public:
    PCMDataSource(QAudioFormat audio_format, qint64 threshold_ms,  QObject *parent = nullptr);
    ~PCMDataSource();

    qint64 available_ms();
    void   clear();
  signals:
    void signal_request_more_data();

protected:
    qint64 readData(char *data, qint64 maxlen) override;
    qint64 writeData(const char *data, qint64 len) override;

  private:
    QByteArray m_buffer;
    qint64 m_size;
    qint64 m_pos;
    qint64 m_threshold_ms;
    QAudioFormat m_audio_format;
};
