#include <QtWidgets/QApplication>
#include <QtCore/QDebug>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // 设置应用程序信息
    app.setApplicationName("FFmpeg音频播放器");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("AudioPlayer");
    
    qDebug() << "启动FFmpeg音频播放器...";
    
    // 创建并显示主窗口
    MainWindow window;
    window.show();
    
    return app.exec();
}
