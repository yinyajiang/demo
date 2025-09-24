#include <QCoreApplication>
#include <QTimer>
#include <iostream>

int main(int argc, char *argv[]) {
  QCoreApplication a(argc, argv);
  // 读取命令行参数
  QStringList args = a.arguments();

  return a.exec();
}