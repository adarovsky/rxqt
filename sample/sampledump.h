#ifndef SAMPLEDUMP_H
#define SAMPLEDUMP_H

#include <QObject>

class SampleDump : public QObject
{
    Q_OBJECT
public:
    explicit SampleDump(QObject *parent = nullptr);

signals:

public slots:
    void debugPrint( const QString& x, int y );
};

#endif // SAMPLEDUMP_H
