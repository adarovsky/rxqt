#include <rxqt.hpp>
#include "rx-drop_map.hpp"
#include <QDebug>
#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QKeyEvent>
#include <QThread>
#include <QThreadPool>
namespace rxo = rxcpp::operators;

class SampleRunnable : public QRunnable {
    std::function<void()> func;
public:
    SampleRunnable(const std::function<void()>& f) : func(f) {}
    virtual void run() {
        qDebug() << "starting long calculation";
        QThread::currentThread()->sleep( 1 );
        qDebug() << "sending result";
        func();
        qDebug() << "sending result complete";
    }
};

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    auto widget = std::unique_ptr<QWidget>(new QWidget());
    auto layout = new QVBoxLayout;
    widget->setLayout(layout);
    {
        auto e0 = new QLineEdit("");
        auto e1 = new QLineEdit;
        e1->setEnabled(false);
        layout->addWidget(e0);
        layout->addWidget(e1);

        rxqt::from_signal(e0, &QLineEdit::textChanged)
                | rxo::drop_map([](auto text) {
                    auto r = rxcpp::subjects::subject<QString>();
                    QThreadPool::globalInstance()->start( new SampleRunnable([r, text]() {
                        r.get_subscriber().on_next(text);
                        r.get_subscriber().on_completed();
                    }));
                    return r.get_observable();
                })
                | rxo::observe_on(rxcpp::observe_on_qt_event_loop())
                | rxo::subscribe<QString>([e1](const QString& s){
                    Q_ASSERT(QApplication::instance()->thread() == QThread::currentThread());
                    e1->setText(s);
                });

//        rxqt::from_event(e0, QEvent::KeyPress)
//                .subscribe([](const QEvent* e){
//                    auto ke = static_cast<const QKeyEvent*>(e);
//                    qDebug() << ke->key();
//                });
    }
    widget->show();
    return a.exec();
}
