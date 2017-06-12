#include <rxqt.hpp>
#include "rx-drop_map.hpp"
#include <random>
#include <QDebug>
#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QKeyEvent>
#include <QThread>
#include <QtConcurrent>
#include <QFutureWatcher>

namespace rxo = rxcpp::operators;

std::random_device rd;
std::uniform_int_distribution<int> dist(0, 50);

struct SampleWorker {
    int counter;
    QString name;
    SampleWorker(const QString &n) : counter(0), name(n) {}
    ~SampleWorker() { qDebug() << "~SampleWorker:" << name; }
    QString process( const QString& text ) {
        auto p = 20 + dist(rd);
        QThread::currentThread()->msleep(p);
        return QString("w%1: %2-%3").arg(name, text, QString::number(counter++));
    }
};

void reduce(QStringList& result, const QString& s) {
    result << s;
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QList<QSharedPointer<SampleWorker>> list;
    for (int i = 1; i <= 10; ++i)
        list << QSharedPointer<SampleWorker>(new SampleWorker(QString::number(i)));


    auto widget = std::unique_ptr<QWidget>(new QWidget());
    auto layout = new QVBoxLayout;
    widget->setLayout(layout);
    {
        auto e0 = new QLineEdit("");
        auto e1 = new QLineEdit;
        e1->setEnabled(false);
        layout->addWidget(e0);
        layout->addWidget(e1);

        auto sig =
        rxqt::from_signal(e0, &QLineEdit::textChanged)
                | rxo::drop_map([&list](auto text) {
                    QTime time; time.start();
                    return rxcpp::observable<>::create<QString>([text, list, time](const rxcpp::subscriber<QString>& s){
                        std::function<QString(QSharedPointer<SampleWorker>)> f([text] (QSharedPointer<SampleWorker> w) {
                            return w->process(text);
                        });

                        auto future = QtConcurrent::mappedReduced(list, f, reduce);
                        auto watcher = new QFutureWatcher<QStringList>;
                        QObject::connect(watcher, &QFutureWatcher<QStringList>::resultReadyAt, [watcher, s, time](int) {
                            auto result = watcher->future().result();
                            if (s.is_subscribed())
                                s.on_next(result.join(", "));
                            s.on_completed();
                            delete watcher;
                            qDebug() << "total execution time:" << time.elapsed() << "msec";
                        });
                        watcher->setFuture(future);
                    });
                });
//                | rxo::observe_on(rxcpp::observe_on_qt_event_loop());

        sig.subscribe([e1](const QString& s){
            Q_ASSERT(QApplication::instance()->thread() == QThread::currentThread());
            e1->setText(s);
        });
    }
    widget->resize(700, 200);
    widget->show();
    return a.exec();
}
