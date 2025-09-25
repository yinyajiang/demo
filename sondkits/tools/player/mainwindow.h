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
#include "audioexporter.h"

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

private slots:
  void onOpenFile(int i);

  void playPause();
  void stop();
  void onVolumeChanged(int stream_index, int volume);
  void onBalanceChanged(int stream_index, int balance);
  void onTempoChanged(int tempo);
  void onProgressSliderPressed();
  void onProgressSliderReleased();
  void onProgressSliderMoved(int value);
  void onTimeProgress(int64_t time_seconds);
  void onSemitoneChanged(int semitone);
  void onExport(QString ext);

private:
  void setupUI();
  QString formatTime(double seconds);

private:
  // 文件控制组
  QPushButton *m_openButton1;
  QLabel *m_fileLabel1;
  QString m_file1;

  QPushButton *m_openButton2;
  QLabel *m_fileLabel2;
  QString m_file2;

  // 播放控制组
  QPushButton *m_playPauseButton;
  QPushButton *m_stopButton;
  QPushButton *m_exportButtonMP3;
  QPushButton *m_exportButtonWAV;

  // 进度控制组
  QSlider *m_progressSlider;
  QLabel *m_currentTimeLabel;
  QLabel *m_totalTimeLabel;

  // 音量控制组
  QLabel *m_volumeLabel1;
  QSlider *m_volumeSlider1;
  QLabel *m_volumeLabel2;
  QSlider *m_volumeSlider2;

  // 声道平衡控制组
  QSlider *m_balanceSlider1;
  QLabel *m_balanceValueLabel1;
  QSlider *m_balanceSlider2;
  QLabel *m_balanceValueLabel2;

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
  std::unique_ptr<AudioExporter> m_exporter;

  // 状态
  QString m_currentFile;
  bool m_isPlaying;
  bool m_isSliderPressed; // 用户是否在拖动进度条
  double m_totalDuration; // 总时长
};

#endif // MAINWINDOW_H
