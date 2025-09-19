#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtCore/QTime>
#include <QtCore/QTimer>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSlider>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

#include "audioplayer.h"

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

private slots:
  void onOpenFile();
  void playPause();
  void stop();
  void onVolumeChanged(int volume);
  void onBalanceChanged(int balance);
  void onTempoChanged(int tempo);
  void onProgressSliderPressed();
  void onProgressSliderReleased();
  void onProgressSliderMoved(int value);
  void onPlayerStateChanged();
  void onDecoderError(const QString &message);
  void updatePlayback();
  void onSemitoneChanged(int semitone);

private:
  void setupUI();
  QString formatTime(double seconds);

private:
  // 文件控制组
  QPushButton *m_openButton;
  QLabel *m_fileLabel;

  // 播放控制组
  QPushButton *m_playPauseButton;
  QPushButton *m_stopButton;

  // 进度控制组
  QSlider *m_progressSlider;
  QLabel *m_currentTimeLabel;
  QLabel *m_totalTimeLabel;

  // 音量控制组
  QLabel *m_volumeLabel;
  QSlider *m_volumeSlider;

  // 声道平衡控制组
  QSlider *m_balanceSlider;
  QLabel *m_balanceValueLabel;

  // 速度控制组
  QSlider *m_tempoSlider;
  QLabel *m_tempoValueLabel;

  // 升降调控制组
  QSlider *m_semitoneSlider;
  QLabel *m_semitoneValueLabel;

  // 信息显示组
  QLabel *m_audioInfoLabel;
  QProgressBar *m_bufferProgress;

  // 状态栏
  QLabel *m_statusLabel;

  // 音频处理
  std::unique_ptr<AudioPlayer> m_player;

  // 定时器
  QTimer *m_updateTimer;

  // 状态
  QString m_currentFile;
  bool m_isPlaying;
  bool m_isSliderPressed; // 用户是否在拖动进度条
  double m_totalDuration; // 总时长
};

#endif // MAINWINDOW_H
