#ifndef RXQTEVENTLOOP_HPP
#define RXQTEVENTLOOP_HPP

#pragma once
#include <rxcpp/rx-includes.hpp>
#include <QScopedPointer>
#include <QTimer>
#include <QtDebug>
#include <QLoggingCategory>
#include <QTimerEvent>
#include <QThread>

Q_DECLARE_LOGGING_CATEGORY(rxqtEventLoop)

namespace rxcpp {

namespace schedulers {

struct qt_event_loop : public scheduler_interface
{
private:
    typedef qt_event_loop this_type;
    qt_event_loop(const this_type&);

    struct qtimer_worker : public worker_interface
    {
    private:
        typedef qtimer_worker this_type;
        typedef detail::action_queue queue_type;

        qtimer_worker(const this_type&);

        class qtimer_worker_state : public QObject, public std::enable_shared_from_this<qtimer_worker_state>
        {
        public:
            typedef detail::schedulable_queue<
                typename clock_type::time_point> queue_item_time;

            typedef queue_item_time::item_type item_type;

            virtual ~qtimer_worker_state()
            {
                qCDebug(rxqtEventLoop) << this << ": thread(" << QThread::currentThreadId() << "), : deallocating, timer";
                std::unique_lock<std::mutex> guard(lock);
                kill_timer();
                lifetime.unsubscribe();
                qCDebug(rxqtEventLoop) << this << ": deallocating done, timer";
            }

            explicit qtimer_worker_state(composite_subscription cs)
                : lifetime(cs)
            {
            }

            void timerEvent(QTimerEvent * event)
            {
                qCDebug(rxqtEventLoop) << this << ": thread(" << QThread::currentThreadId() << "), : timer event from timer" << event->timerId();
                handle_queue();
            }

            void handle_queue()
            {
                qCDebug(rxqtEventLoop) << this << ": thread(" << QThread::currentThreadId() << "), : handle_queue()";
                forever {
                    std::unique_lock<std::mutex> guard(lock);
                    if (q.empty()) {
                        kill_timer();
                        break;
                    }

                    auto& peek = q.top();
                    if (!peek.what.is_subscribed()) {
                        qCDebug(rxqtEventLoop) << this << ": thread(" << QThread::currentThreadId() << "), is not subscribed, continuing";
                        q.pop();
                        continue;
                    }

                    auto now = clock_type::now();
                    if (now >= peek.when) {
                        auto what = peek.what;
                        q.pop();
                        r.reset(q.empty());
                        qCDebug(rxqtEventLoop) << this << ": thread(" << QThread::currentThreadId() << "), running item";
                        guard.unlock();
                        what(r.get_recurse());
                        qCDebug(rxqtEventLoop) << this << ": thread(" << QThread::currentThreadId() << "), running item complete";
                        continue;
                    }

                    auto d = std::chrono::duration_cast<std::chrono::milliseconds>(peek.when - now);
                    schedule_timer(d);
                    break;
                }
            }

            void kill_timer() {
                if (!current_timer.empty()) {
                    qCDebug(rxqtEventLoop) << this << ": thread(" << QThread::currentThreadId() << "), killing timer" << current_timer.get();
                    killTimer(current_timer.get());
                }
            }

            void schedule_timer(std::chrono::milliseconds timeout) {
                kill_timer();
                current_timer.reset(startTimer(timeout.count(), Qt::PreciseTimer));
                qCDebug(rxqtEventLoop) << this << ": thread(" << QThread::currentThreadId() << "), started timer" << current_timer.get();
            }

            composite_subscription lifetime;
            mutable std::mutex lock;
            mutable queue_item_time q;
            rxcpp::util::maybe<int> current_timer;
            recursion r;
        };

        std::shared_ptr<qtimer_worker_state> state;

    public:
        virtual ~qtimer_worker()
        {
        }

        qtimer_worker(composite_subscription cs)
            : state(std::make_shared<qtimer_worker_state>(cs))
        {

            auto keepAlive = state;

            state->lifetime.add([keepAlive](){
                std::unique_lock<std::mutex> guard(keepAlive->lock);
                auto expired = std::move(keepAlive->q);
                if (!keepAlive->q.empty()) std::terminate();
                keepAlive->kill_timer();
            });
        }

        virtual clock_type::time_point now() const {
            return clock_type::now();
        }

        virtual void schedule(const schedulable& scbl) const {
            schedule(now(), scbl);
        }

        virtual void schedule(clock_type::time_point when, const schedulable& scbl) const {
            QObject stub; {
                auto keepAlive = state;
                std::unique_lock<std::mutex> guard(keepAlive->lock);
                if (scbl.is_subscribed()) {
                    keepAlive->q.push(qtimer_worker_state::item_type(when, scbl));
                    keepAlive->r.reset(false);
                }
                guard.unlock();

                QObject::connect(&stub, &QObject::destroyed, keepAlive.get(), [keepAlive]() {
                    keepAlive->handle_queue();
                });
            }
        }
    };

public:
    qt_event_loop()
    {
    }

    virtual ~qt_event_loop()
    {
    }

    virtual clock_type::time_point now() const {
        return clock_type::now();
    }

    virtual worker create_worker(composite_subscription cs) const {
        return worker(cs, std::make_shared<qtimer_worker>(cs));
    }
};

inline scheduler make_qt_event_loop() {
    static scheduler instance = make_scheduler<qt_event_loop>();
    return instance;
}

}

inline serialize_one_worker serialize_qt_event_loop() {
    static serialize_one_worker r(rxsc::make_qt_event_loop());
    return r;
}

inline observe_on_one_worker observe_on_qt_event_loop() {
    static observe_on_one_worker r(rxsc::make_qt_event_loop());
    return r;
}

}


#endif // RXQTEVENTLOOP_HPP
