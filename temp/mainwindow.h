#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSlider>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QFileDialog>
#include <QtCore/QTimer>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QWidget>
#include <QtCore/QTime>

#include "audiodecoder.h"
#include "audioplayer.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void openFile();
    void playPause();
    void stop();
    void onVolumeChanged(int volume);
    void onBalanceChanged(int balance);
    void onProgressSliderPressed();
    void onProgressSliderReleased();
    void onProgressSliderMoved(int value);
    void onPlayerStateChanged();
    void onPlayerNeedMoreData();
    void onDecoderError(const QString &message);
    void updatePlayback();

private:
    void setupUI();
    void setupConnections();
    void updateUI();
    void loadMoreAudioData();
    void resetPlayer();
    QString formatTime(double seconds);

private:
    // UI 组件
    QWidget *m_centralWidget;
    QVBoxLayout *m_mainLayout;
    
    // 文件控制组
    QGroupBox *m_fileGroup;
    QHBoxLayout *m_fileLayout;
    QPushButton *m_openButton;
    QLabel *m_fileLabel;
    
    // 播放控制组
    QGroupBox *m_controlGroup;
    QHBoxLayout *m_controlLayout;
    QPushButton *m_playPauseButton;
    QPushButton *m_stopButton;
    
    // 进度控制组
    QGroupBox *m_progressGroup;
    QVBoxLayout *m_progressMainLayout;
    QHBoxLayout *m_progressLayout;
    QSlider *m_progressSlider;
    QLabel *m_currentTimeLabel;
    QLabel *m_totalTimeLabel;
    
    // 音量控制组
    QGroupBox *m_volumeGroup;
    QHBoxLayout *m_volumeLayout;
    QLabel *m_volumeLabel;
    QSlider *m_volumeSlider;
    
    // 声道平衡控制组
    QGroupBox *m_balanceGroup;
    QHBoxLayout *m_balanceLayout;
    QLabel *m_balanceLabel;
    QSlider *m_balanceSlider;
    QLabel *m_balanceValueLabel;
    
    // 信息显示组
    QGroupBox *m_infoGroup;
    QVBoxLayout *m_infoLayout;
    QLabel *m_audioInfoLabel;
    QProgressBar *m_bufferProgress;
    
    // 状态栏
    QLabel *m_statusLabel;
    
    // 音频处理
    AudioDecoder *m_decoder;
    AudioPlayer *m_player;
    
    // 定时器
    QTimer *m_updateTimer;
    
    // 状态
    QString m_currentFile;
    bool m_isPlaying;
    bool m_isSliderPressed; // 用户是否在拖动进度条
    double m_totalDuration; // 总时长
};

#endif // MAINWINDOW_H
