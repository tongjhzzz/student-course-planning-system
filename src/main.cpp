#include "ui/main_window.h"

#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);// Qt图形界面程序的运行环境

    MainWindow window;// 创建本项目的主窗口对象
    window.show();// 显示窗口

    return application.exec();// 持续处理用户点击，输入，关闭窗口等
}
