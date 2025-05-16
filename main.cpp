#include "miniproj.h"
#include <QtWidgets/QApplication>
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    miniproj w;
    w.show();
    return a.exec();
}
