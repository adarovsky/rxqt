#include <QtDebug>
#include "sampledump.h"

SampleDump::SampleDump(QObject *parent) : QObject(parent)
{

}

void SampleDump::debugPrint( const QString& x, int y ) {
    qDebug() << "debugPrint(" << x << ", " << y << ")";
}
