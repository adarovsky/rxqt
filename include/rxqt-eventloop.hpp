#ifndef RXQTEVENTLOOP_HPP
#define RXQTEVENTLOOP_HPP

#pragma once
#include <rxcpp/rx-includes.hpp>
#include <QScopedPointer>
#include <QTimer>
#include <QtDebug>

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
                timer.stop();
                lifetime.unsubscribe();
            }

            explicit qtimer_worker_state(composite_subscription cs)
                : lifetime(cs)
            {
                timer.setSingleShot(true);
                QObject::connect(&timer, &QTimer::timeout, this, &qtimer_worker_state::setup_timer, Qt::QueuedConnection);
            }

            void setup_timer()
            {
                while (!q.empty()) {
                    auto& peek = q.top();
                    if (!peek.what.is_subscribed()) {
                        q.pop();
                        continue;
                    }

                    auto now = clock_type::now();
                    if (now >= peek.when) {
                        auto what = peek.what;
                        q.pop();
                        r.reset(q.empty());
                        what(r.get_recurse());
                        continue;
                    }

                    auto d = std::chrono::duration_cast<std::chrono::milliseconds>(peek.when - now);
                    timer.start(d.count());
                    break;
                }
            }

            composite_subscription lifetime;
            mutable queue_item_time q;
            QTimer timer;
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
                auto expired = std::move(keepAlive->q);
                if (!keepAlive->q.empty()) std::terminate();
                keepAlive->timer.stop();
            });
        }

        virtual clock_type::time_point now() const {
            return clock_type::now();
        }

        virtual void schedule(const schedulable& scbl) const {
            schedule(now(), scbl);
        }

        virtual void schedule(clock_type::time_point when, const schedulable& scbl) const {
            if (scbl.is_subscribed()) {
                state->q.push(qtimer_worker_state::item_type(when, scbl));
                state->r.reset(false);
            }
            QObject stub;
            QObject::connect(&stub, &QObject::destroyed, state.get(), &qtimer_worker_state::setup_timer);
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
