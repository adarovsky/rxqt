#include <rxqt.hpp>
#include <QDebug>
#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QKeyEvent>
#include <QThread>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    auto widget = std::unique_ptr<QWidget>(new QWidget());
    auto layout = new QVBoxLayout;
    widget->setLayout(layout);
    {
        auto e0 = new QLineEdit("");
        auto e01 = new QLineEdit("");
        auto e1 = new QLineEdit;
        e1->setEnabled(false);
        layout->addWidget(e0);
        layout->addWidget(e01);
        layout->addWidget(e1);

        rxqt::from_signal(e0, &QLineEdit::textChanged)
                .zip(rxqt::from_signal(e01, &QLineEdit::textChanged))
                .map([](auto tuple){ return "(" + std::get<0>(tuple) + ") - (" + std::get<1>(tuple) + ")"; })
                .delay(rxcpp::serialize_event_loop(), std::chrono::seconds(1))
                .observe_on(rxcpp::observe_on_qt_event_loop())
                .subscribe([e1](const QString& s){
            Q_ASSERT(QApplication::instance()->thread() == QThread::currentThread());
            e1->setText(s);
        });

        rxqt::from_event(e0, QEvent::KeyPress)
                .subscribe([](const QEvent* e){
                    auto ke = static_cast<const QKeyEvent*>(e);
                    qDebug() << ke->key();
                });
    }
    widget->show();
    return a.exec();
}
